#include <atomic>
#include <csignal>
#include <fstream>
#include <glib-unix.h>
#include <gst/app/gstappsink.h>
#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>
#include <iostream>
#include <mqtt/async_client.h>
#include <mutex>
#include <opencv2/opencv.hpp>
#include <thread>
#include "crowd_detector.h"

#define CERT_FILE "/opt/rtsp/server.cert.pem"
#define KEY_FILE "/opt/rtsp/server.key.pem"
// ====== Configuration ======
static GMainLoop* loop;
const std::string MQTT_BROKER = "ssl://192.168.0.115:8883";
const std::string MQTT_EVENT_TOPIC = "main/data/cap";
const std::string MQTT_PERIODIC_TOPIC = "main/data/periodic";
const std::string crowd_onnx_path = "model/crowd.onnx";
const std::string MQTT_TOPIC_OFF_ORDER_FROM_QT = "qt/off";

std::string SUB_ID;
std::string MQTT_PUB_TOPIC;
std::string MQTT_QT_PUB_TOPIC;
std::string MQTT_TOPIC_LED_ON;
std::string LED_DEVICE_PATH = "/dev/gpioled";

std::atomic<bool> running(true);
std::atomic<bool> capture_requested(false);
//std::atomic<bool> led_on_flag(false);

CrowdDetector crowd_detector(crowd_onnx_path);

// Frame capture globals
static cv::Mat latest_frame_mat;
static std::mutex frame_mutex;
static bool new_frame_available = false;

// AWB globals - awb
static GstElement* camera_source = nullptr; // awb
static std::atomic<long> last_awb_time(0); // awb

// ====== AWB Functions ======
// awb - simple white pixel check and AWB trigger
static void check_and_apply_awb(const cv::Mat& frame) { // awb
    // printf("[AWB DEBUG] Function called!\n"); fflush(stdout); // awb - commented debug

    if (frame.empty()) { // awb
        // printf("[AWB DEBUG] Frame is empty!\n"); fflush(stdout); // awb - commented debug
        return; // awb
    } // awb

    if (!camera_source) { // awb
        // printf("[AWB DEBUG] Camera source is NULL!\n"); fflush(stdout); // awb - commented debug
        return; // awb
    } // awb

    long current_time = time(nullptr); // awb
    if (current_time - last_awb_time < 2) { // awb - faster cooldown: 2 seconds instead of 10
        // printf("[AWB DEBUG] Still in cooldown, skipping\n"); fflush(stdout); // awb - commented debug
        return; // awb
    } // awb

    // Simple white pixel count - awb - use better threshold
    cv::Mat gray; // awb
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY); // awb
    int bright_pixels = cv::countNonZero(gray > 240); // awb - count very bright pixels (240 instead of 200)
    int total_pixels = gray.rows * gray.cols; // awb
    float bright_ratio = (float)bright_pixels / total_pixels; // awb

    // printf("[AWB DEBUG] Bright pixels (>240): %d/%d = %.3f ratio\n", bright_pixels, total_pixels, bright_ratio); fflush(stdout); // awb - commented debug

    if (bright_ratio > 0.05 && bright_ratio < 0.4) { // awb - normalized threshold 5%-40%
        printf("[AWB] *** AWB ON *** bright ratio: %.3f\n", bright_ratio); fflush(stdout); // awb
        g_object_set(camera_source, "awb-mode", 3, NULL); // awb - fluorescent mode (moderate strength)
        last_awb_time = current_time; // awb
    }
    else if (bright_ratio <= 0.02) { // awb - turn AWB off when very few bright pixels
        printf("[AWB] *** AWB OFF *** bright ratio: %.3f\n", bright_ratio); fflush(stdout); // awb
        g_object_set(camera_source, "awb-mode", 0, NULL); // awb - auto white balance OFF
        last_awb_time = current_time; // awb
    } // awb
    // else { // awb - commented debug
    //     printf("[AWB DEBUG] Bright ratio %.3f outside normal range (0.02-0.4)\n", bright_ratio); fflush(stdout); // awb - commented debug
    // } // awb
} // awb

// ====== Signal Handlers ======
static gboolean intr_handler(gpointer user_data) {
    running = false;
    g_main_loop_quit(loop);
    return TRUE;
}


static gboolean timeout(GstRTSPServer* server) {
    GstRTSPSessionPool* pool = gst_rtsp_server_get_session_pool(server);
    gst_rtsp_session_pool_cleanup(pool);
    g_object_unref(pool);
    return TRUE;
}

gboolean accept_certificate(GstRTSPAuth* auth, GTlsConnection* conn,
    GTlsCertificate* peer_cert,
    GTlsCertificateFlags errors, gpointer user_data) {
    return TRUE; // TLS handled automatically with NONE mode
}

// ====== Frame Capture Probe ======
static GstPadProbeReturn buffer_probe_cb(GstPad* pad, GstPadProbeInfo* info,
    gpointer user_data) {
    // awb - always process frames for AWB, not just when capture_requested
    static int frame_counter = 0; // awb
    frame_counter++; // awb

    if (!capture_requested.load() && frame_counter % 10 != 0) { // awb - check AWB every 10 frames (faster) instead of 30
        return GST_PAD_PROBE_OK;
    }
    //std::cout << "[DEBUG][Probe] Invoked - buffer ready for capture." << std::endl; // awb - commented debug

    GstBuffer* buffer = GST_PAD_PROBE_INFO_BUFFER(info);
    if (!buffer) {
        std::cerr << "[ERROR][Probe] Buffer is null." << std::endl;
        return GST_PAD_PROBE_OK;
    }

    GstCaps* caps = gst_pad_get_current_caps(pad);
    if (!caps) {
        std::cerr << "[ERROR][Probe] Caps is null." << std::endl;
        return GST_PAD_PROBE_OK;
    }

    GstStructure* structure = gst_caps_get_structure(caps, 0);
    const gchar* format_str = gst_structure_get_string(structure, "format");
    gint width, height;

    if (gst_structure_get_int(structure, "width", &width) &&
        gst_structure_get_int(structure, "height", &height) && format_str &&
        g_str_equal(format_str, "NV21")) {

        GstMapInfo map;

        if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
            try {
                int y_size = width * height;
                int uv_size = y_size / 2;

                std::vector<unsigned char> nv12_data(y_size + uv_size);
                memcpy(nv12_data.data(), map.data, y_size);

                const unsigned char* nv21_vu = map.data + y_size;
                unsigned char* nv12_uv = nv12_data.data() + y_size;

                for (int i = 0; i < uv_size; i += 2) {
                    nv12_uv[i] = nv21_vu[i + 1]; // U
                    nv12_uv[i + 1] = nv21_vu[i]; // V
                }

                cv::Mat yuv_mat(height + height / 2, width, CV_8UC1, nv12_data.data());
                cv::Mat frame_bgr;
                cv::cvtColor(yuv_mat, frame_bgr, cv::COLOR_YUV2BGR_NV12);

                cv::Mat resized;
                cv::resize(frame_bgr, resized, cv::Size(640, 480));

                check_and_apply_awb(resized); // awb - check white pixels and apply AWB if needed

                // awb - only save frame if actually capturing
                if (capture_requested.load()) { // awb
                    std::lock_guard<std::mutex> lock(frame_mutex);
                    latest_frame_mat = resized.clone();
                    capture_requested = false;
                    new_frame_available = true;
                } // awb
            }
            catch (const cv::Exception& e) {
                std::cerr << "[ERROR][Probe] OpenCV exception: " << e.what() << std::endl;
            }
            gst_buffer_unmap(buffer, &map);
        }
    }

    gst_caps_unref(caps);
    return GST_PAD_PROBE_OK;
}

// ====== Media Factory Callback ======
static void media_prepared_cb(GstRTSPMediaFactory* factory, GstRTSPMedia* media,
    gpointer user_data) {
    if (!media || !GST_IS_RTSP_MEDIA(media))
        return;

    GstElement* pipeline = gst_rtsp_media_get_element(media);
    if (!pipeline)
        return;

    // awb - get camera source reference for AWB control
    camera_source = gst_bin_get_by_name(GST_BIN(pipeline), "camerasrc"); // awb
    if (camera_source) { // awb
        std::cout << "[AWB] Camera source reference captured for AWB control" << std::endl; // awb
    } // awb

    GstElement* capture_queue =
        gst_bin_get_by_name(GST_BIN(pipeline), "capture_queue");
    if (capture_queue) {
        GstPad* pad = gst_element_get_static_pad(capture_queue, "sink");
        if (pad) {
            std::cout << "[DEBUG] Probe attached to capture_queue sink pad" << std::endl;
            gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER, buffer_probe_cb, NULL,
                NULL);
            gst_object_unref(pad);
        }
        else {
            std::cerr << "[ERROR] Failed to get static pad 'sink' from capture_queue" << std::endl;
        }
        gst_object_unref(capture_queue);
    }
    else {
        std::cerr << "[ERROR] Failed to get static pad 'sink' from capture_queue" << std::endl;
    }
    gst_object_unref(pipeline);
}

void turn_on_led() {
    int fd = open(LED_DEVICE_PATH.c_str(), O_WRONLY);
    if (fd >= 0) {
        write(fd, "1", 1);
        close(fd);
    }
}

void turn_off_led() {
    int fd = open(LED_DEVICE_PATH.c_str(), O_WRONLY);
    if (fd >= 0) {
        write(fd, "0", 1);
        close(fd);
    }
}

void process_frame_and_publish(const std::string& topic, mqtt::async_client* client,
    CrowdDetector* detector, int qos = 1) {
    cv::Mat frame;
    {
        std::lock_guard<std::mutex> lock(frame_mutex);
        if (new_frame_available && !latest_frame_mat.empty()) {
            frame = latest_frame_mat.clone();
            new_frame_available = false;
        }
    }

    if (!frame.empty()) {
        std::vector<cv::Point> people = detector->detect(frame);
        int people_count = static_cast<int>(people.size());
        std::string payload = std::to_string(people_count);
        std::string topic_send;

        if (topic == MQTT_EVENT_TOPIC) topic_send = MQTT_PUB_TOPIC;
        else if (topic == MQTT_PERIODIC_TOPIC) topic_send = MQTT_QT_PUB_TOPIC;
        else topic_send = topic; // fallback

        try {
            auto msg = mqtt::make_message(topic_send, payload);
            msg->set_qos(qos);
            client->publish(msg);
            std::cout << "[MQTT] People count (" << people_count << ") sent to topic: " << topic_send << std::endl;
        }
        catch (const mqtt::exception& e) {
            std::cerr << "[MQTT ERROR] Publish failed on " << topic_send << ": " << e.what() << std::endl;
        }
    }
    else {
        std::cerr << "[FRAME] No valid frame available for topic " << topic << std::endl;
    }
}

// ====== MQTT Thread & Callback ======
void mqtt_thread_func() {
    mqtt::async_client client(MQTT_BROKER, "sub_pi_" + SUB_ID);
    mqtt::ssl_options sslopts;
    sslopts.set_trust_store("/usr/local/share/ca-certificates/ca.crt");

    auto connOpts = mqtt::connect_options_builder()
        .clean_session(true)
        .automatic_reconnect(true)
        .ssl(sslopts)
        .finalize();

    class callback : public virtual mqtt::callback {
    public:
        callback(mqtt::async_client* cli, CrowdDetector* det)
            : client(cli), detector(det) {
        }

        void message_arrived(mqtt::const_message_ptr msg) override {
            std::string topic = msg->get_topic();
            if (topic == MQTT_EVENT_TOPIC) {
                capture_requested = true;
                std::thread([this]() {
                    int attempts = 0;
                    while (attempts++ < 60 && capture_requested.load()) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    }
                    process_frame_and_publish(MQTT_EVENT_TOPIC, client, detector);
                    }).detach();
            }
            else if (topic == MQTT_PERIODIC_TOPIC) {
                capture_requested = true;
                std::thread([this]() {
                    int attempts = 0;
                    while (attempts++ < 60 && capture_requested.load()) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    }
                    process_frame_and_publish(MQTT_PERIODIC_TOPIC, client, detector);
                    }).detach();
            }
            else if (topic == MQTT_TOPIC_LED_ON) {
                // if (!led_on_flag) {
                //     turn_on_led();
                //     led_on_flag = true;
                // }
                turn_on_led();
                std::cout << "[MQTT] led on: " << topic << std::endl;
            }
            else if (topic == MQTT_TOPIC_OFF_ORDER_FROM_QT) {
                // if (led_on_flag) {
                //     turn_off_led();
                //     led_on_flag = false;
                // }
                turn_off_led();
                std::cout << "[MQTT] led off: " << topic << std::endl;
            }
            else {
                std::cout << "[MQTT] Unknown topic received: " << topic << std::endl;
            }
        }

    private:
        mqtt::async_client* client;
        CrowdDetector* detector;
    };

    callback cb(&client, &crowd_detector);
    client.set_callback(cb);

    try {
        client.connect(connOpts)->wait();
        client.subscribe(MQTT_EVENT_TOPIC, 1);
        client.subscribe(MQTT_PERIODIC_TOPIC, 1);
        client.subscribe(MQTT_TOPIC_OFF_ORDER_FROM_QT, 1);
        client.subscribe(MQTT_TOPIC_LED_ON, 1);

        while (running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        client.disconnect()->wait();
    }
    catch (const mqtt::exception& exc) {
        std::cerr << "[MQTT] Error: " << exc.what() << std::endl;
    }
}


int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <sub_id>" << std::endl;
        return 1;
    }

    SUB_ID = argv[1];
    MQTT_PUB_TOPIC = "sub/capture/" + SUB_ID;
    MQTT_QT_PUB_TOPIC = "pop/" + SUB_ID;
    MQTT_TOPIC_LED_ON = "sub/led/on/" + SUB_ID;
    LED_DEVICE_PATH += SUB_ID;

    gst_init(&argc, &argv);
    g_unix_signal_add(SIGINT, intr_handler, NULL);
    loop = g_main_loop_new(NULL, FALSE);

    g_print("RTSP TLS Server - ECC Certificate Infrastructure\n");
    g_print("Certificate: %s\n", CERT_FILE);
    g_print("Private Key: %s\n", KEY_FILE);

    if (!g_file_test(CERT_FILE, G_FILE_TEST_EXISTS) ||
        !g_file_test(KEY_FILE, G_FILE_TEST_EXISTS)) {
        g_printerr("ERROR: Certificate files not found\n");
        return -1;
    }

    GstRTSPServer* server = gst_rtsp_server_new();
    gst_rtsp_server_set_address(server, "0.0.0.0");
    gst_rtsp_server_set_service(server, "8555");

    GstRTSPAuth* auth = gst_rtsp_auth_new();

    g_print("Loading ECC TLS certificate...\n");
    GError* error = NULL;
    GTlsCertificate* cert =
        g_tls_certificate_new_from_files(CERT_FILE, KEY_FILE, &error);
    if (!cert) {
        g_printerr("Failed to load TLS certificate: %s\n", error->message);
        return -1;
    }
    g_print("ECC certificate loaded successfully\n");

    gst_rtsp_auth_set_tls_certificate(auth, cert);

    GTlsDatabase* database = g_tls_file_database_new(CERT_FILE, &error);
    if (database) {
        gst_rtsp_auth_set_tls_database(auth, database);
        g_print("TLS database initialized\n");
    }
    else if (error) {
        g_print("Warning: TLS database creation failed: %s\n", error->message);
        g_clear_error(&error);
    }

    gst_rtsp_auth_set_tls_authentication_mode(auth, G_TLS_AUTHENTICATION_NONE);

    GstRTSPToken* token = gst_rtsp_token_new(
        GST_RTSP_TOKEN_MEDIA_FACTORY_ROLE, G_TYPE_STRING, "media-factory", NULL);
    gchar* basic = gst_rtsp_auth_make_basic("username", "password");
    gst_rtsp_auth_add_basic(auth, basic, token);
    g_free(basic);
    gst_rtsp_token_unref(token);

    gst_rtsp_server_set_auth(server, auth);

    GstRTSPMountPoints* mounts = gst_rtsp_server_get_mount_points(server);
    GstRTSPMediaFactory* factory = gst_rtsp_media_factory_new();

    GstElement* test_v4l2convert = gst_element_factory_make("v4l2convert", NULL);
    GstElement* test_v4l2h264enc = gst_element_factory_make("v4l2h264enc", NULL);

    if (test_v4l2convert && test_v4l2h264enc) {
        g_print("Using optimized low-latency pipeline with v4l2convert and "
            "v4l2h264enc (RTSP only - stable baseline)\n");
        gst_rtsp_media_factory_set_launch(
            factory,
            "( libcamerasrc name=camerasrc ! "
            "video/x-raw,width=1920,height=1080,framerate=30/1 ! "
            "queue name=capture_queue max-size-buffers=10 max-size-bytes=0 "
            "max-size-time=100000000 ! "
            "v4l2convert ! video/x-raw,format=NV12 ! "
            "v4l2h264enc capture-io-mode=2 "
            "extra-controls=\"controls,repeat_sequence_header=1,video_bitrate_mode="
            "0,video_bitrate=8000000,h264_i_frame_period=30,h264_profile=4\" ! "
            "video/x-h264,level=(string)4 ! h264parse ! rtph264pay "
            "config-interval=1 name=pay0 pt=96 )");
    }
    else {
        g_print("Optimized pipeline elements not available, using software encoder "
            "fallback (RTSP only - stable baseline)\n");
        gst_rtsp_media_factory_set_launch(
            factory, "( libcamerasrc ! videoconvert ! videoscale ! "
            "video/x-raw,width=1920,height=1080,framerate=30/1 ! "
            "x264enc tune=zerolatency bitrate=4000 speed-preset=ultrafast "
            "key-int-max=30 bframes=0 aud=false cabac=false dct8x8=false "
            "threads=4 ! "
            "rtph264pay config-interval=1 name=pay0 pt=96 )");
    }

    if (test_v4l2convert)
        gst_object_unref(test_v4l2convert);
    if (test_v4l2h264enc)
        gst_object_unref(test_v4l2h264enc);

    GstRTSPPermissions* permissions = gst_rtsp_permissions_new();
    gst_rtsp_permissions_add_role(
        permissions, "media-factory", GST_RTSP_PERM_MEDIA_FACTORY_ACCESS,
        G_TYPE_BOOLEAN, TRUE, GST_RTSP_PERM_MEDIA_FACTORY_CONSTRUCT,
        G_TYPE_BOOLEAN, TRUE, NULL);
    gst_rtsp_media_factory_set_permissions(factory, permissions);
    gst_rtsp_permissions_unref(permissions);

    gst_rtsp_media_factory_set_protocols(factory, GST_RTSP_LOWER_TRANS_TCP);
    gst_rtsp_media_factory_set_profiles(factory, GST_RTSP_PROFILE_SAVP);
    gst_rtsp_media_factory_set_shared(factory, TRUE);

    g_signal_connect(factory, "media-constructed", G_CALLBACK(media_prepared_cb),
        NULL);
    gst_rtsp_mount_points_add_factory(mounts, "/stream", factory);
    g_object_unref(mounts);

    if (!gst_rtsp_server_attach(server, NULL)) {
        g_printerr("Failed to attach RTSP server\n");
        return -1;
    }

    g_timeout_add_seconds(2, (GSourceFunc)timeout, server);

    g_print("\nRTSP TLS Server Started Successfully!\n");
    g_print("=====================================\n");
    g_print("Stream URL: rtsps://<Pi-IP>:8555/stream\n");
    g_print("Security: ECC P-384 + Perfect Forward Secrecy\n");
    g_print("Username: username\n");
    g_print("Password: password\n");
    g_print("Certificate: %s\n", CERT_FILE);
    g_print("Private Key: %s\n", KEY_FILE);
    g_print("\nServer ready for connections...\n");
    g_print("Press Ctrl+C to stop\n\n");
    g_print("[INIT] RTSP TLS Server and pipeline launched, ready for MQTT requests.\n");

    std::thread mqtt_thread(mqtt_thread_func);
    //std::thread led_thread(led_control_thread);
    mqtt_thread.detach();
    //led_thread.join();

    //if (led_on_flag) turn_off_led();
    g_main_loop_run(loop);

    // Cleanup
    if (database)
        g_object_unref(database);
    g_object_unref(cert);
    g_object_unref(auth);
    g_object_unref(server);
    g_main_loop_unref(loop);

    return 0;
}