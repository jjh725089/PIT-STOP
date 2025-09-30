// Standard Library
#include <iostream>
#include <fstream>
#include <filesystem>

// System Library
#include <unistd.h>
#include <fcntl.h>
#include <sys/inotify.h>
#include <sys/time.h>

// MQTT
#include <mqtt/async_client.h>
#include <mqtt/ssl_options.h>

// JSON
#include <nlohmann/json.hpp>

// Project headers
#include "fall_detector.h"
#include "crowd_detector.h"
#include "congestion_analyzer.h"
#include "path_finder.h"
#include "renderer.h"
#include "speaker.h"
#include "config.h"

#define EVENT_SIZE       (sizeof(struct inotify_event))
#define EVENT_BUF_LEN    (1024 * (EVENT_SIZE + 16))

using json = nlohmann::json;
namespace fs = std::filesystem;

// MQTT configuration
const std::string mqtt_broker_address = "ssl://localhost:8883";
const std::string mqtt_topic_fall_trigger = "pi/data/fall";
const std::string mqtt_topic_exit_info = "qt/data/exits";
const std::string mqtt_topic_capture_request = "main/data/cap";
const std::string mqtt_topic_periodic_receive = "pi/data/Count";
const std::string mqtt_topic_periodic_send = "main/data/Count";
const std::string mqtt_topic_off_order_from_qt = "qt/off";
const std::vector<std::string> sub_camera_ids = { "1", "2", "3" };
const std::string mqtt_client_id = "main_pi";
const std::string mqtt_cert_path = "/usr/local/share/ca-certificates/ca.crt";

// Global state
int fall_center_x = -1, fall_center_y = -1;
std::vector<int> sub_camera_crowd_counts = { -1, -1, -1 };
std::vector<int> congestion_grid_counts;
int congestion_grid_rows, congestion_grid_cols;
int image_width = 0, image_height = 0;

std::atomic<bool> fall_event_response_enabled(true);
std::atomic<bool> periodic_event_response_enabled(true);
std::atomic<bool> off_order_from_qt(false);
std::atomic<unsigned long> fall_event_start_time(0);
std::atomic<unsigned long> gate_led_turn_on_time(0);

std::mutex state_mutex;
std::condition_variable main_camera_condition;

std::string fall_event_timestamp;
fs::path capture_result_folder = "./";

cv::Mat crowd_incident_image;
std::vector<cv::Point> detected_people_coordinates;

bool gate_led_is_on = false;

std::vector<Exit> dynamic_exit_points;
std::mutex exit_mutex;

std::atomic<bool> crowd_result_expected(false);

Speaker global_speaker;
std::thread speaker_thread;

std::chrono::steady_clock::time_point g_t_mqtt_fall_triggered;
std::chrono::steady_clock::time_point g_t_ch1_detected;
std::chrono::steady_clock::time_point g_t_fall_done;
std::chrono::steady_clock::time_point g_t_subcrowd_done;
std::chrono::steady_clock::time_point g_t_congestion_done;
std::chrono::steady_clock::time_point g_t_path_done;
std::chrono::steady_clock::time_point g_t_visual_done;
std::chrono::steady_clock::time_point g_t_publish_done;

unsigned long millis();

void periodicPublishThread(mqtt::async_client* _mqtt_client, std::atomic<bool>& _running_flag);
void waitAndProcessNew1jpg(const std::string& _watch_directory, int _timeout_sec, int _poll_interval_ms, FallDetector& _fall_detector);
void findFallOnCH1(const cv::Mat& _image, FallDetector& _fall_detector);
void safeMoveImage(const std::string& _source_path, const std::string& _destination_dir);
void crowdCountingSub(mqtt::async_client* _mqtt_client, int _people_count, std::size_t _camera_index);
void controlGateLed(mqtt::async_client* _mqtt_client, const cv::Mat& _incident_image, const std::vector<cv::Point>& _people_coordinates);
void saveFallLog(mqtt::async_client* _mqtt_client, const std::string& _event_timestamp, int _fall_x, int _fall_y, int _selected_gate_index);
void clearCaptureRepoDirectory(const std::string& directory_path);

void waitAndProcessPeriodicjpg(const std::string& _watch_directory, int _timeout_sec, int _poll_interval_ms, FallDetector& _fall_detector, mqtt::async_client* mqtt_client_);
void crowdCountingPeriodic(const cv::Mat& _image, FallDetector& _fall_detector, mqtt::async_client* mqtt_client_);
void safeDeleteImage(const std::string& _source_path);

class MainCallback : public virtual mqtt::callback
{
public:
    MainCallback(FallDetector* _fall_detector,
        CrowdDetector* _crowd_detector,
        mqtt::async_client* _mqtt_client)
        : fall_detector_instance_(_fall_detector),
        crowd_detector_instance_(_crowd_detector),
        mqtt_client_(_mqtt_client)
    {
    }

    void message_arrived(mqtt::const_message_ptr _message) override
    {
        std::string topic = _message->get_topic();

        if (topic == mqtt_topic_fall_trigger)
        {
            if (!fall_event_response_enabled)
            {
                std::cout << "[EVENT] Ignored fall trigger: waiting for LED timeout reset" << std::endl;
                return;
            }

            fall_event_response_enabled = false;
            fall_event_start_time = millis();
            crowd_result_expected = true;

            {
                std::lock_guard<std::mutex> lock(state_mutex);
                sub_camera_crowd_counts = { -1, -1, -1 };
            }

            std::cout << "[EVENT] Fall event triggered via MQTT" << std::endl;

            g_t_mqtt_fall_triggered = std::chrono::steady_clock::now();

            auto request_message = mqtt::make_message(mqtt_topic_capture_request, "");
            request_message->set_qos(1);
            mqtt_client_->publish(request_message);
            std::cout << "[MQTT] Published capture request: " << mqtt_topic_capture_request << std::endl;

            std::thread(
                waitAndProcessNew1jpg,
                "./cap_repo",
                60,
                500,
                std::ref(*fall_detector_instance_)
            ).detach();
        }
        else if (topic == mqtt_topic_exit_info)
        {
            std::string payload(_message->get_payload().begin(), _message->get_payload().end());
            std::cout << "mqtt_topic_exit_info arrived" << std::endl;
            try
            {
                json j = json::parse(payload);

                if (!j.contains("cameras") || !j["cameras"].is_array())
                {
                    std::cerr << "[EXITS] Invalid message format: missing 'cameras'" << std::endl;
                    return;
                }

                std::vector<Exit> parsed_exits;
                size_t idx = 0;
                for (const auto& exit_json : j["cameras"])
                {
                    if (!exit_json.contains("x") || !exit_json.contains("y"))
                    {
                        std::cerr << "[EXITS] Invalid coordinate: missing x or y" << std::endl;
                        continue;
                    }
                    int x = exit_json["x"];
                    int y = exit_json["y"];
                    parsed_exits.push_back({ cv::Point(x, y), idx });
                    idx++;
                }

                if (!parsed_exits.empty())
                {
                    std::lock_guard<std::mutex> lock(exit_mutex);
                    dynamic_exit_points = parsed_exits;
                    std::cout << "[EXITS] Updated camera points (count: " << dynamic_exit_points.size() << ")" << std::endl;
                }
                else
                {
                    std::cerr << "[EXITS] No valid camera coordinates found in message." << std::endl;
                }
            }
            catch (const std::exception& ex)
            {
                std::cerr << "[EXITS] Failed to parse camera JSON: " << ex.what() << std::endl;
            }
        }

        else if (topic.find("sub/capture/") == 0)
        {
            std::string sub_camera_id = topic.substr(topic.find_last_of('/') + 1);
            std::string payload(_message->get_payload().begin(), _message->get_payload().end());

            std::cout << "[MQTT] Received sub-camera (" << sub_camera_id << ") crowd count: " << payload << std::endl;

            int camera_index = std::stoi(sub_camera_id);
            int people_count = std::stoi(payload);

            crowdCountingSub(mqtt_client_, people_count, camera_index);
        }
        else if (topic == mqtt_topic_periodic_receive)
        {
            if (!periodic_event_response_enabled)
            {
                //std::cout << "[EVENT] Ignored fall trigger: waiting for LED timeout reset" << std::endl;
                return;
            }
            std::thread(
                waitAndProcessPeriodicjpg,
                "./cap_repo",
                60,
                500,
                std::ref(*fall_detector_instance_),
                mqtt_client_
            ).detach();
        }
        else if (topic == mqtt_topic_off_order_from_qt)
        {
            off_order_from_qt = true;
        }
    }

private:
    FallDetector* fall_detector_instance_;
    CrowdDetector* crowd_detector_instance_;
    mqtt::async_client* mqtt_client_;
};

int main()
{
    FallDetector fall_detector(FALL_MODEL_PATH);
    CrowdDetector crowd_detector(CROWD_MODEL_PATH);

    if (!global_speaker.init()) {
        std::cerr << "[SPEAKER] Initialization failed." << std::endl;
        return 1;
    }
    speaker_thread = std::thread([]() {
        global_speaker.run();
        });

    mqtt::ssl_options ssl_options;
    ssl_options.set_trust_store(mqtt_cert_path);

    mqtt::async_client mqtt_client(mqtt_broker_address, mqtt_client_id);
    MainCallback main_callback(&fall_detector, &crowd_detector, &mqtt_client);

    mqtt::connect_options connect_options = mqtt::connect_options_builder()
        .clean_session(true)
        .automatic_reconnect(true)
        .ssl(ssl_options)
        .finalize();

    std::atomic<bool> heartbeat_running(true);
    std::thread heartbeat_thread(periodicPublishThread, &mqtt_client, std::ref(heartbeat_running));

    try
    {
        mqtt_client.set_callback(main_callback);
        mqtt_client.connect(connect_options)->wait();

        mqtt_client.subscribe(mqtt_topic_fall_trigger, 1);
        std::cout << "[MQTT] Subscribed: " << mqtt_topic_fall_trigger << std::endl;

        mqtt_client.subscribe(mqtt_topic_exit_info, 1);
        std::cout << "[MQTT] Subscribed: " << mqtt_topic_exit_info << std::endl;

        mqtt_client.subscribe(mqtt_topic_periodic_receive, 1);
        std::cout << "[MQTT] Subscribed: " << mqtt_topic_periodic_receive << std::endl;

        for (const auto& sub_id : sub_camera_ids)
        {
            std::string topic = "sub/capture/" + sub_id;
            mqtt_client.subscribe(topic, 1);
            std::cout << "[MQTT] Subscribed: " << topic << std::endl;
        }

        std::cout << "[MAIN] System ready. Waiting for fall events..." << std::endl;

        while (true)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            unsigned long current_time_ms = millis();

            if (gate_led_is_on && current_time_ms - gate_led_turn_on_time >= 60000)
            {
                fall_center_x = -1;
                fall_center_y = -1;
                gate_led_is_on = false;
                fall_event_start_time = 0;
                periodic_event_response_enabled = true;
                fall_event_response_enabled = true;
                std::cout << "[TIMEOUT] Auto-reset: LED off, fall detection re-enabled" << std::endl;
            }

            if (!fall_event_response_enabled && fall_event_start_time > 0 &&
                (current_time_ms - fall_event_start_time >= 60000))
            {
                fall_center_x = -1;
                fall_center_y = -1;
                periodic_event_response_enabled = true;
                fall_event_response_enabled = true;
                fall_event_start_time = 0;
                std::cout << "[TIMEOUT] No LED response. Fall detection re-enabled after 30s." << std::endl;
            }

            if (off_order_from_qt)
            {
                off_order_from_qt = false;
                fall_center_x = -1;
                fall_center_y = -1;
                gate_led_is_on = false;
                fall_event_start_time = 0;
                periodic_event_response_enabled = true;
                fall_event_response_enabled = true;
                std::cout << "[TIMEOUT] Qt announced emergency situation is over. Fall detection re-enabled." << std::endl;
            }
        }
    }
    catch (const mqtt::exception& ex)
    {
        std::cerr << "[FATAL] MQTT connection error: " << ex.what() << std::endl;
        heartbeat_running = false;
        if (heartbeat_thread.joinable()) heartbeat_thread.join();
        return 1;
    }

    heartbeat_running = false;
    if (heartbeat_thread.joinable()) heartbeat_thread.join();

    global_speaker.requestShutdown();
    global_speaker.stop();
    if (speaker_thread.joinable()) speaker_thread.join();

    return 0;
}

unsigned long millis()
{
    struct timeval time_value;
    gettimeofday(&time_value, nullptr);
    return (time_value.tv_sec * 1000) + (time_value.tv_usec / 1000);
}

void periodicPublishThread(mqtt::async_client* _mqtt_client, std::atomic<bool>& _running_flag)
{
    while (_running_flag.load())
    {
        try
        {
            auto heartbeat_message = mqtt::make_message("main/data/periodic", "");
            heartbeat_message->set_qos(1);
            _mqtt_client->publish(heartbeat_message);
            // std::cout << "[MQTT] Heartbeat published to main/data/periodic" << std::endl;
        }
        catch (const mqtt::exception& ex)
        {
            std::cerr << "[MQTT ERROR] Failed to publish heartbeat: " << ex.what() << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::seconds(10));
    }
}

void waitAndProcessNew1jpg(const std::string& _watch_directory,
    int _timeout_sec,
    int _poll_interval_ms,
    FallDetector& _fall_detector)
{
    periodic_event_response_enabled = false;
    int fd = inotify_init1(IN_NONBLOCK);
    if (fd < 0)
    {
        std::cerr << "[INOTIFY] Failed to initialize inotify." << std::endl;
        return;
    }

    int wd = inotify_add_watch(fd, _watch_directory.c_str(), IN_CREATE);
    if (wd < 0)
    {
        std::cerr << "[INOTIFY] Failed to add watch on directory: " << _watch_directory << std::endl;
        close(fd);
        return;
    }

    char buffer[EVENT_BUF_LEN];
    auto start_time = std::chrono::steady_clock::now();

    while (true)
    {
        int length = read(fd, buffer, EVENT_BUF_LEN);
        if (length < 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(_poll_interval_ms));
        }
        else
        {
            int i = 0;
            while (i < length)
            {
                struct inotify_event* event = (struct inotify_event*)&buffer[i];
                if ((event->mask & IN_CREATE) && event->len > 0)
                {
                    std::string filename(event->name);
                    if (filename.find("EventRule_1-CH1") != std::string::npos)
                    {
                        std::string image_path = _watch_directory + "/" + filename;

                        unsigned long arrival_time = millis();
                        unsigned long elapsed = arrival_time - fall_event_start_time;
                        std::cout << "[PERF] Time from fall trigger to EventRule_1-CH1 image arrival: " << elapsed << " ms" << std::endl;

                        std::cout << "[MONITOR] Detected new EventRule_1-CH1 image: " << image_path << std::endl;

                        size_t pos = filename.find("-EventRule_1-CH1");
                        if (pos != std::string::npos)
                            fall_event_timestamp = filename.substr(0, pos);
                        else
                        {
                            fall_event_timestamp = "unknown";
                            std::cerr << "[WARN] Failed to extract timestamp from filename: " << filename << std::endl;
                        }

                        capture_result_folder = "./prev_cap_repo/" + fall_event_timestamp;

                        try
                        {
                            if (fs::create_directory(capture_result_folder))
                                std::cout << "[FS] Created folder: " << capture_result_folder << std::endl;
                            else
                                std::cout << "[FS] Folder already exists or failed to create: " << capture_result_folder << std::endl;
                        }
                        catch (const fs::filesystem_error& ex)
                        {
                            std::cerr << "[FS ERROR] Failed to create folder: " << ex.what() << std::endl;
                        }

                        cv::Mat image = cv::imread(image_path);
                        if (image.empty())
                        {
                            std::cerr << "[MONITOR] Failed to read image: " << image_path << std::endl;
                            break;
                        }

                        g_t_ch1_detected = std::chrono::steady_clock::now();

                        {
                            std::lock_guard<std::mutex> lock(state_mutex);
                            image_width = image.cols;
                            image_height = image.rows;
                            congestion_grid_cols = (image_width + GRID_CELL_SIZE - 1) / GRID_CELL_SIZE;
                            congestion_grid_rows = (image_height + GRID_CELL_SIZE - 1) / GRID_CELL_SIZE;
                            std::cout << "[INIT] Resolution set: " << image_width << "x" << image_height << std::endl;
                        }

                        findFallOnCH1(image, _fall_detector);
                        g_t_fall_done = std::chrono::steady_clock::now();

                        safeMoveImage(image_path, capture_result_folder.string());

                        inotify_rm_watch(fd, wd);
                        close(fd);
                        return;
                    }
                }
                i += sizeof(struct inotify_event) + event->len;
            }
        }

        if (std::chrono::steady_clock::now() - start_time > std::chrono::seconds(_timeout_sec))
        {
            std::cerr << "[TIMEOUT] No new CH1 image received within timeout." << std::endl;
            break;
        }
    }

    inotify_rm_watch(fd, wd);
    close(fd);

}

void waitAndProcessPeriodicjpg(const std::string& _watch_directory,
    int _timeout_sec,
    int _poll_interval_ms,
    FallDetector& _fall_detector,
    mqtt::async_client* mqtt_client_)
{
    int fd = inotify_init1(IN_NONBLOCK);
    if (fd < 0)
    {
        std::cerr << "[INOTIFY] Failed to initialize inotify." << std::endl;
        return;
    }

    int wd = inotify_add_watch(fd, _watch_directory.c_str(), IN_CREATE);
    if (wd < 0)
    {
        std::cerr << "[INOTIFY] Failed to add watch on directory: " << _watch_directory << std::endl;
        close(fd);
        return;
    }

    char buffer[EVENT_BUF_LEN];
    auto start_time = std::chrono::steady_clock::now();

    while (true)
    {
        int length = read(fd, buffer, EVENT_BUF_LEN);
        if (length < 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(_poll_interval_ms));
        }
        else
        {
            int i = 0;
            while (i < length)
            {
                struct inotify_event* event = (struct inotify_event*)&buffer[i];
                if ((event->mask & IN_CREATE) && event->len > 0)
                {
                    std::string filename(event->name);
                    if (filename.find("EventRule_2-CH1") != std::string::npos)
                    {
                        std::string image_path = _watch_directory + "/" + filename;

                        cv::Mat image = cv::imread(image_path);
                        if (image.empty())
                        {
                            std::cerr << "[MONITOR] Failed to read image: " << image_path << std::endl;
                            break;
                        }

                        crowdCountingPeriodic(image, _fall_detector, mqtt_client_);
                        safeDeleteImage(image_path);

                        inotify_rm_watch(fd, wd);
                        close(fd);
                        return;
                    }
                }
                i += sizeof(struct inotify_event) + event->len;
            }
        }

        if (std::chrono::steady_clock::now() - start_time > std::chrono::seconds(_timeout_sec))
        {
            std::cerr << "[TIMEOUT] No new CH1 image received within timeout." << std::endl;
            break;
        }
    }

    inotify_rm_watch(fd, wd);
    close(fd);
}

std::mutex file_operation_mutex;

void safeMoveImage(const std::string& _source_path, const std::string& _destination_dir)
{
    std::lock_guard<std::mutex> lock(file_operation_mutex);

    if (!fs::exists(_destination_dir))
    {
        fs::create_directories(_destination_dir);
    }

    std::string destination_path = _destination_dir + "/" + fs::path(_source_path).filename().string();

    if (fs::exists(_source_path))
    {
        try
        {
            fs::rename(_source_path, destination_path);
            std::cout << "[FS] Moved file: " << _source_path << " -> " << destination_path << std::endl;
        }
        catch (const fs::filesystem_error& ex)
        {
            std::cerr << "[FS ERROR] File move failed: " << ex.what() << std::endl;
        }
    }
}

void safeDeleteImage(const std::string& _source_path)
{
    std::lock_guard<std::mutex> lock(file_operation_mutex);

    try
    {
        if (fs::exists(_source_path))
        {
            fs::remove(_source_path);
        }
        else
        {
            std::cerr << "[FS WARN] File does not exist: " << _source_path << std::endl;
        }
    }
    catch (const fs::filesystem_error& ex)
    {
        std::cerr << "[FS ERROR] File delete failed: " << ex.what() << std::endl;
    }
}

void findFallOnCH1(const cv::Mat& _image, FallDetector& _fall_detector)
{
    if (_image.empty())
    {
        std::cerr << "[FALL] Input image is empty!" << std::endl;
        return;
    }

    std::vector<FallInfo> fall_detections = _fall_detector.detect(_image);

    std::cout << "[FALL] Detection complete. Total: " << fall_detections.size() << std::endl;
    if (fall_detections.empty())
    {
        std::cout << "[FALL] No fall candidates detected." << std::endl;
    }

    std::vector<cv::Point> people_coordinates;
    bool is_fall_detected_flag = false;
    for (const auto& detection : fall_detections)
    {
        if (detection.pred == 0)
        {
            is_fall_detected_flag = true;
            {
                std::lock_guard<std::mutex> lock(state_mutex);
                fall_center_x = detection.bbox.x + detection.bbox.width / 2;
                fall_center_y = detection.bbox.y + detection.bbox.height / 2;
            }

            std::cout << "[FALL] Fall detected at center: (" << fall_center_x << ", " << fall_center_y << ")" << std::endl;
            std::cout << "[FALL] Bounding box: ("
                << detection.bbox.x << ", " << detection.bbox.y << ") -> ("
                << detection.bbox.x + detection.bbox.width << ", "
                << detection.bbox.y + detection.bbox.height << ")" << std::endl;
        }
        else
        {
            int person_center_x = detection.bbox.x + detection.bbox.width / 2;
            int person_center_y = detection.bbox.y + detection.bbox.height / 2;

            people_coordinates.push_back(cv::Point(person_center_x, person_center_y));
        }
    }

    if (!is_fall_detected_flag) {
        std::cout << "[FALL] no fall detected." << std::endl;
        return;
    }

    detected_people_coordinates = people_coordinates;
    crowd_incident_image = _image.clone();

    std::cout << "[CROWD] CH1 crowd detection complete. People count: " << people_coordinates.size() << std::endl;

    Renderer renderer;
    cv::Mat image_copy = _image.clone();
    renderer.drawFallBoxes(image_copy, fall_detections);

    std::string result_path = (capture_result_folder / "result.jpg").string();
    cv::imwrite(result_path, image_copy);
    std::cout << "[FALL] Visualized result saved: " << result_path << std::endl;
}

void crowdCountingPeriodic(const cv::Mat& _image, FallDetector& _fall_detector, mqtt::async_client* mqtt_client_)
{
    if (_image.empty())
    {
        std::cerr << "[CROWD] Input image is empty!" << std::endl;
        return;
    }

    std::vector<FallInfo> fall_detections = _fall_detector.detect(_image);
    int people_count = fall_detections.size();

    std::string payload = std::to_string(people_count);
    try {
        auto msg = mqtt::make_message(mqtt_topic_periodic_send, payload);
        msg->set_qos(1);
        mqtt_client_->publish(msg);
        std::cout << "[MQTT] Periodic people count (" << people_count << ") sent to topic: " << mqtt_topic_periodic_send << std::endl;
    }
    catch (const mqtt::exception& e) {
        std::cerr << "[MQTT ERROR] Publish failed on " << mqtt_topic_periodic_send << ": " << e.what() << std::endl;
    }
}

void crowdCountingSub(mqtt::async_client* _mqtt_client, int _people_count, std::size_t _camera_index)
{
    if (!crowd_result_expected)
    {
        std::cout << "[CROWD] Ignored sub-camera crowd count: not expecting results now." << std::endl;
        return;
    }

    if (_camera_index < 1 || _camera_index > 3)
    {
        std::cerr << "[CROWD] Invalid camera index: " << _camera_index << std::endl;
        return;
    }

    {
        std::lock_guard<std::mutex> lock(state_mutex);
        sub_camera_crowd_counts[_camera_index - 1] = _people_count;
        std::cout << "[CROWD] sub_camera_crowd_counts[" << (_camera_index - 1)
            << "] = " << _people_count << std::endl;
    }

    if (fall_center_x >= 0 && fall_center_y >= 0)
    {
        bool all_counts_ready = true;
        for (int i = 0; i < 3; ++i)
        {
            if (sub_camera_crowd_counts[i] < 0)
            {
                all_counts_ready = false;
                break;
            }
        }

        if (all_counts_ready)
        {
            g_t_subcrowd_done = std::chrono::steady_clock::now();

            controlGateLed(_mqtt_client, crowd_incident_image, detected_people_coordinates);

            for (int i = 0; i < 3; ++i)
            {
                sub_camera_crowd_counts[i] = -1;
            }
        }
    }
}

std::vector<PathInfo> paths_info = {};

void controlGateLed(mqtt::async_client* _mqtt_client,
    const cv::Mat& _incident_image,
    const std::vector<cv::Point>& _people_coordinates)
{
    if (fall_center_x < 0 || fall_center_y < 0)
    {
        std::cerr << "[ERROR] Fall center coordinates are not set." << std::endl;
        return;
    }

    CongestionAnalyzer congestion_analyzer(image_width, image_height);
    std::vector<std::vector<float>> congestion_grid = congestion_analyzer.analyzeCongestionGrid(_people_coordinates);

    g_t_congestion_done = std::chrono::steady_clock::now();

    std::vector<Exit> exits;
    {
        std::lock_guard<std::mutex> lock(exit_mutex);

        if (!dynamic_exit_points.empty())
        {
            std::cout << "[CONTROL] Using dynamic exits from MQTT" << std::endl;

            for (const auto& exit : dynamic_exit_points)
            {
                cv::Point grid_pos = toGrid(exit.location);
                cv::Point corrected_pixel = toPixelCenter(grid_pos);
                exits.push_back({ corrected_pixel, exit.index });
            }
        }
        else
        {
            std::cout << "[CONTROL] No dynamic exits. Using fallback defaults." << std::endl;

            int max_grid_cols = image_width / GRID_CELL_SIZE;
            int max_grid_rows = image_height / GRID_CELL_SIZE;

            exits = {
                            { toPixelCenter(cv::Point(0, 0)), 0 },
                            { toPixelCenter(cv::Point(max_grid_cols - 1, 0)), 1 },
                            { toPixelCenter(cv::Point(0, max_grid_rows - 1)), 2 }
            };
        }
    }

    Pathfinder pathfinder(image_width, image_height);
    pathfinder.setCongestionMap(congestion_grid);

    cv::Point fall_center_pixel = toPixelCenter(toGrid(cv::Point(fall_center_x, fall_center_y)));

    PathInfo best_path_info = {};
    best_path_info.score = std::numeric_limits<float>::max();

    for (size_t i = 0; i < exits.size(); ++i)
    {
        PathInfo path_info = pathfinder.generatePathInfo(fall_center_pixel, exits.at(i), congestion_analyzer, sub_camera_crowd_counts.at(i));
        paths_info.push_back(path_info);

        if (best_path_info.score > path_info.score) best_path_info = path_info;
    }

    if (best_path_info.path.empty())
    {
        std::cerr << "[PATH] No valid path found to any exit." << std::endl;

        try
        {
            std::string err_topic = "main/result/error/" + fall_event_timestamp;
            std::string err_payload = "Pathfinding failed: no path found to exits.";
            auto err_msg = mqtt::make_message(err_topic, err_payload);
            err_msg->set_qos(1);
            _mqtt_client->publish(err_msg);
        }
        catch (...)
        {
            std::cerr << "[MQTT ERROR] Failed to publish pathfinding error." << std::endl;
        }

        return;
    }

    g_t_path_done = std::chrono::steady_clock::now();

    Renderer renderer;
    cv::Mat visualized_image = _incident_image.clone();
    renderer.drawCongestionHeatmap(visualized_image, congestion_grid);
    renderer.drawExits(visualized_image, exits);
    renderer.drawPath(visualized_image, best_path_info.path, fall_center_pixel, best_path_info.exit);

    std::string output_path = "./prev_cap_repo/" + fall_event_timestamp + "/path.jpg";
    cv::imwrite(output_path, visualized_image);

    g_t_visual_done = std::chrono::steady_clock::now();

    std::cout << "[RENDER] Path image saved: " << output_path << std::endl;

    int selected_gate_index = best_path_info.exit.index;
    float final_score = best_path_info.score;
    std::cout << "[PATH] Selected gate index: " << selected_gate_index + 1 << ", Score: " << final_score << std::endl;

    try
    {
        std::string led_topic = "sub/led/on/" + std::to_string(selected_gate_index + 1);
        auto led_msg = mqtt::make_message(led_topic, "");
        led_msg->set_qos(1);
        _mqtt_client->publish(led_msg);

        std::cout << "[MQTT] LED ON command published: " << led_topic << std::endl;

        gate_led_turn_on_time = millis();
        gate_led_is_on = true;
    }
    catch (const mqtt::exception& ex)
    {
        std::cerr << "[MQTT ERROR] Failed to publish LED command: " << ex.what() << std::endl;
    }

    saveFallLog(_mqtt_client, fall_event_timestamp, fall_center_x, fall_center_y, selected_gate_index);
}

void saveFallLog(mqtt::async_client* _mqtt_client,
    const std::string& _event_timestamp,
    int _fall_center_x,
    int _fall_center_y,
    int _selected_gate_index)
{
    float fall_to_gate_dist[3];
    for (const auto& exit : dynamic_exit_points)
    {
        int x = exit.location.x;
        int y = exit.location.y;
        int idx = exit.index;
        fall_to_gate_dist[idx] = std::sqrt(
            (fall_center_x - x) * (fall_center_x - x) +
            (fall_center_y - y) * (fall_center_y - y)
        );
    }

    json fall_log_json =
    {
                    { "event_time", _event_timestamp },
                    { "fall_point_x", _fall_center_x },
                    { "fall_point_y", _fall_center_y },
                    { "min_gate_idx", _selected_gate_index + 1 },
                    { "gate_1_people_count", sub_camera_crowd_counts.at(0)},
                    { "gate_2_people_count", sub_camera_crowd_counts.at(1) },
                    { "gate_3_people_count", sub_camera_crowd_counts.at(2) },
                    { "indoor_people_count", detected_people_coordinates.size()},
                    { "gate_1_dist", fall_to_gate_dist[0]},
                    { "gate_2_dist", fall_to_gate_dist[1]},
                    { "gate_3_dist", fall_to_gate_dist[2]},
                    { "gate_1_score", paths_info.at(0).score},
                    { "gate_2_score", paths_info.at(1).score},
                    { "gate_3_score", paths_info.at(2).score}
    };

    std::string log_directory = "./log";
    if (!fs::exists(log_directory))
    {
        fs::create_directories(log_directory);
    }

    std::string log_file_path = log_directory + "/" + _event_timestamp + ".json";

    try
    {
        std::ofstream output_file(log_file_path);
        output_file << fall_log_json.dump(4);
        output_file.close();
        std::cout << "[LOG] JSON log saved: " << log_file_path << std::endl;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "[LOG ERROR] Failed to write JSON file: " << ex.what() << std::endl;

        try
        {
            std::string err_topic = "main/result/error/" + _event_timestamp;
            std::string err_payload = "Failed to write JSON log: " + std::string(ex.what());
            auto err_msg = mqtt::make_message(err_topic, err_payload);
            err_msg->set_qos(1);
            _mqtt_client->publish(err_msg);
        }
        catch (...)
        {
            std::cerr << "[MQTT ERROR] Failed to publish JSON log error message." << std::endl;
        }

        return;
    }

    try
    {
        std::string log_topic = "main/result/log/";
        auto json_msg = mqtt::make_message(log_topic, fall_log_json.dump());
        json_msg->set_qos(1);
        _mqtt_client->publish(json_msg);
        std::cout << "[MQTT] Published fall log JSON to topic: " << log_topic << std::endl;
    }
    catch (const mqtt::exception& ex)
    {
        std::cerr << "[MQTT ERROR] Failed to publish JSON log: " << ex.what() << std::endl;
    }

    std::string image_path = "./prev_cap_repo/" + _event_timestamp + "/path.jpg";
    if (fs::exists(image_path))
    {
        try
        {
            std::ifstream image_stream(image_path, std::ios::binary);
            std::vector<char> image_buffer((std::istreambuf_iterator<char>(image_stream)), std::istreambuf_iterator<char>());

            std::string image_topic = "main/result/image/";
            auto image_msg = mqtt::make_message(image_topic, image_buffer.data(), image_buffer.size());
            image_msg->set_qos(1);
            _mqtt_client->publish(image_msg);

            std::cout << "[MQTT] Published path image to topic: " << image_topic << std::endl;
        }
        catch (const std::exception& ex)
        {
            std::cerr << "[MQTT ERROR] Failed to publish image: " << ex.what() << std::endl;

            try
            {
                std::string err_topic = "main/result/error/" + _event_timestamp;
                std::string err_payload = "Failed to publish image: " + std::string(ex.what());
                auto err_msg = mqtt::make_message(err_topic, err_payload);
                err_msg->set_qos(1);
                _mqtt_client->publish(err_msg);
            }
            catch (...)
            {
                std::cerr << "[MQTT ERROR] Failed to publish image-publish error message." << std::endl;
            }
        }
    }
    else
    {
        std::cerr << "[ERROR] Image not found: " << image_path << std::endl;

        try
        {
            std::string err_topic = "main/result/error/" + _event_timestamp;
            std::string err_payload = "Path image not found: " + image_path;
            auto err_msg = mqtt::make_message(err_topic, err_payload);
            err_msg->set_qos(1);
            _mqtt_client->publish(err_msg);
        }
        catch (...)
        {
            std::cerr << "[MQTT ERROR] Failed to publish image-not-found error." << std::endl;
        }
    }

    g_t_publish_done = std::chrono::steady_clock::now();

    std::cout << "\n======= [TIME LOG SUMMARY] =======" << std::endl;
    std::cout << "[TIME] CH1 image detected ("
        << std::chrono::duration_cast<std::chrono::milliseconds>(g_t_ch1_detected - g_t_mqtt_fall_triggered).count()
        << " ms)" << std::endl;

    std::cout << "[TIME] Fall detection ("
        << std::chrono::duration_cast<std::chrono::milliseconds>(g_t_fall_done - g_t_ch1_detected).count()
        << " ms)" << std::endl;

    // std::cout << "[TIME] Crowd detection ("
    //      << std::chrono::duration_cast<std::chrono::milliseconds>(g_t_crowd_done - g_t_fall_done).count()
    //      << " ms)" << std::endl;

    std::cout << "[TIME] All sub-camera counts received ("
        << std::chrono::duration_cast<std::chrono::milliseconds>(g_t_subcrowd_done - g_t_ch1_detected).count()
        << " ms)" << std::endl;

    std::cout << "[TIME] Congestion analysis ("
        << std::chrono::duration_cast<std::chrono::milliseconds>(g_t_congestion_done - g_t_subcrowd_done).count()
        << " ms)" << std::endl;

    std::cout << "[TIME] Pathfinding ("
        << std::chrono::duration_cast<std::chrono::milliseconds>(g_t_path_done - g_t_congestion_done).count()
        << " ms)" << std::endl;

    std::cout << "[TIME] Visualization save ("
        << std::chrono::duration_cast<std::chrono::milliseconds>(g_t_visual_done - g_t_path_done).count()
        << " ms)" << std::endl;

    std::cout << "[TIME] LED control + MQTT publish ("
        << std::chrono::duration_cast<std::chrono::milliseconds>(g_t_publish_done - g_t_visual_done).count()
        << " ms)" << std::endl;

    std::cout << "[TIME] Total response time: "
        << std::chrono::duration_cast<std::chrono::milliseconds>(g_t_publish_done - g_t_ch1_detected).count()
        << " ms" << std::endl;

    std::cout << "===================================\n" << std::endl;

    fall_center_x = -1;
    fall_center_y = -1;
    periodic_event_response_enabled = true;
    fall_event_response_enabled = true;
    fall_event_start_time = 0;
    gate_led_is_on = false;
    crowd_result_expected = false;
    std::cout << "[STATE] Fall cycle complete. Detection re-enabled." << std::endl;
}

void clearCaptureRepoDirectory(const std::string& directory_path) {
    try
    {
        if (fs::exists(directory_path) && fs::is_directory(directory_path))
        {
            for (const auto& entry : fs::directory_iterator(directory_path))
            {
                fs::remove_all(entry);
            }

            std::cout << "[FS] Cleared capture repo: " << directory_path << std::endl;
        }
        else
        {
            fs::create_directories(directory_path);
            std::cout << "[FS] Created capture repo: " << directory_path << std::endl;
        }
    }
    catch (const fs::filesystem_error& ex)
    {
        std::cerr << "[FS ERROR] Failed to clear capture repo: " << ex.what() << std::endl;
    }
}