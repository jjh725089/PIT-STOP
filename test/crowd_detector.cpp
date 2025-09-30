// Standard Library
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <mutex>
#include <unordered_map>

// Project headers
#include "crowd_detector.h"
#include "config.h"

// === Static Shared ORT Environment ===
Ort::Env& CrowdDetector::getEnv() {
    static Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "CrowdDetector");

    return env;
}

// === Shared Session per Model Path (thread-safe) ===
std::shared_ptr<Ort::Session> CrowdDetector::getSharedSession(const std::string& model_path) {
    static std::mutex session_mutex;
    static std::unordered_map<std::string, std::shared_ptr<Ort::Session>> session_map;

    std::lock_guard<std::mutex> lock(session_mutex);

    auto it = session_map.find(model_path);
    if (it != session_map.end()) return it->second;

    Ort::SessionOptions options;
    options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    options.SetIntraOpNumThreads(2);

    auto session = std::make_shared<Ort::Session>(getEnv(), model_path.c_str(), options);
    session_map[model_path] = session;

    return session;
}

// === Constructor ===
CrowdDetector::CrowdDetector(const std::string& model_path) {
    try {
        session = getSharedSession(model_path);
        runWarmUp();
    }
    catch (const Ort::Exception& e) {
        std::cerr << "[CrowdDetector::CrowdDetector] Failed to initialize session: " << e.what() << std::endl;
        session = nullptr;
    }
}

// === Public Inference Entry Point ===
std::vector<CrowdInfo> CrowdDetector::detect(const cv::Mat& image) {
    std::vector<CrowdInfo> crowd;
    try {
        if (!session) {
            std::cerr << "[CrowdDetector::detect] Skipping detection: session not initialized." << std::endl;

            return crowd;
        }

        float scale;
        int top, left;
        std::vector<float> input_tensor = preprocessYoloInput(image, scale, top, left);
        Ort::Value output = runYoloInference(input_tensor);
        postprocessYoloOutput(output, scale, top, left, crowd);

    }
    catch (const std::exception& e) {
        std::cerr << "[CrowdDetector::detect] Error: " << e.what() << std::endl;
    }
    return crowd;
}

// === Get Centers of Crowd BBoxes ===
std::vector<cv::Point> CrowdDetector::getCrowdCenters(const std::vector<CrowdInfo>& crowd_info) {
    std::vector<cv::Point> centers;

    for (const auto& info : crowd_info) {
        const cv::Point center = info.bbox.tl() + cv::Point(info.bbox.width / 2, info.bbox.height / 2);
        centers.emplace_back(center);
    }

    return centers;
}

// === Warm-up dummy inference ===
void CrowdDetector::runWarmUp() {
    try {
        cv::Mat dummy(YOLO_INPUT_HEIGHT, YOLO_INPUT_WIDTH, CV_8UC3, cv::Scalar(114, 114, 114));
        detect(dummy);
    }
    catch (const std::exception& e) {
        std::cerr << "[CrowdDetector::runWarmUp] Error: " << e.what() << std::endl;
    }
}

// === Preprocess YOLO input ===
std::vector<float> CrowdDetector::preprocessYoloInput(const cv::Mat& image, float& scale, int& top, int& left) {
    if (image.empty()) throw std::runtime_error("Input image is empty.");

    cv::Mat padded = letterbox(image, scale, top, left);
    cv::cvtColor(padded, padded, cv::COLOR_BGR2RGB);
    padded.convertTo(padded, CV_32FC3, 1.0 / 255.0);

    std::vector<cv::Mat> channels(3);
    cv::split(padded, channels);

    std::vector<float> input_tensor;
    input_tensor.reserve(3 * YOLO_INPUT_WIDTH * YOLO_INPUT_HEIGHT);

    for (int c = 0; c < 3; ++c) {
        input_tensor.insert(input_tensor.end(), (float*)channels[c].datastart, (float*)channels[c].dataend);
    }

    return input_tensor;
}

// === Run ONNX inference ===
Ort::Value CrowdDetector::runYoloInference(const std::vector<float>& input_tensor) {
    if (input_tensor.empty()) throw std::runtime_error("Input tensor is empty.");

    Ort::MemoryInfo mem_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    auto input_shape = std::vector<int64_t>{ static_cast<int64_t>(1), static_cast<int64_t>(3), static_cast<int64_t>(YOLO_INPUT_HEIGHT), static_cast<int64_t>(YOLO_INPUT_WIDTH) };

    Ort::Value input = Ort::Value::CreateTensor<float>(mem_info, const_cast<float*>(input_tensor.data()), input_tensor.size(), input_shape.data(), input_shape.size());

    Ort::AllocatorWithDefaultOptions allocator;
    auto input_name_ptr = session->GetInputNameAllocated(0, allocator);
    auto output_name_ptr = session->GetOutputNameAllocated(0, allocator);

    const char* input_name = input_name_ptr.get();
    const char* output_name = output_name_ptr.get();

    std::vector<const char*> input_names = { input_name };
    std::vector<const char*> output_names = { output_name };

    std::vector<Ort::Value> outputs = session->Run(Ort::RunOptions{ nullptr }, input_names.data(), &input, 1, output_names.data(), 1);
    if (outputs.empty() || !outputs[0].IsTensor()) throw std::runtime_error("Output tensor is invalid.");

    return std::move(outputs[0]);
}

// === Postprocess output tensor to get crowd ===
void CrowdDetector::postprocessYoloOutput(Ort::Value& output_tensor, float scale, int top, int left, std::vector<CrowdInfo>& results) {
    float* data = output_tensor.GetTensorMutableData<float>();
    if (!data) throw std::runtime_error("Output tensor data is null.");

    std::vector<int64_t> shape = output_tensor.GetTensorTypeAndShapeInfo().GetShape();
    size_t num_boxes = shape.size() >= 2 ? static_cast<size_t>(shape[1]) : 0;

    std::vector<cv::Rect> boxes;
    std::vector<float> scores;

    for (size_t i = 0; i < num_boxes; ++i) {
        float conf = data[i * 6 + 4];
        if (conf < CROWD_CONF_THRESHOLD) continue;

        float x = data[i * 6 + 0];
        float y = data[i * 6 + 1];
        float w = data[i * 6 + 2];
        float h = data[i * 6 + 3];

        cv::Rect box = convertXywhToXyxy(x, y, w, h);

        box.x = static_cast<int>(std::round((box.x - left) / scale));
        box.y = static_cast<int>(std::round((box.y - top) / scale));
        box.width = static_cast<int>(std::round(box.width / scale));
        box.height = static_cast<int>(std::round(box.height / scale));

        boxes.emplace_back(box);
        scores.push_back(conf);
    }

    std::vector<int> keep = applyNms(boxes, scores);

    for (int idx : keep) {
        CrowdInfo info;
        info.bbox = boxes[idx];
        info.conf = scores[idx];
        results.push_back(info);
    }
}

// === NMS ===
std::vector<int> CrowdDetector::applyNms(const std::vector<cv::Rect>& boxes, const std::vector<float>& scores) {
    std::vector<int> keep;
    std::vector<std::pair<float, int>> order;

    for (size_t i = 0; i < scores.size(); ++i) {
        order.emplace_back(scores[i], static_cast<int>(i));
    }

    std::sort(order.begin(), order.end(), [](const auto& a, const auto& b) {
        return a.first > b.first || (a.first == b.first && a.second < b.second);
        });

    std::vector<bool> suppressed(scores.size(), false);

    for (size_t i = 0; i < order.size(); ++i) {
        int idx = order[i].second;
        if (suppressed[idx]) continue;

        keep.push_back(idx);

        for (size_t j = i + 1; j < order.size(); ++j) {
            int next_idx = order[j].second;
            if (computeIoU(boxes[idx], boxes[next_idx]) > NMS_THRESHOLD) {
                suppressed[next_idx] = true;
            }
        }
    }

    return keep;
}

// === Compute Intersection of Union ===
float CrowdDetector::computeIoU(const cv::Rect& a, const cv::Rect& b) {
    float inter = static_cast<float>((a & b).area());
    float uni = static_cast<float>(a.area() + b.area() - inter);

    return inter / (uni + 1e-6f);
}