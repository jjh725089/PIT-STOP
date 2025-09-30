// Standard Library
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <mutex>
#include <unordered_map>

// OpenCV
#include <opencv2/dnn.hpp>

// Project headers
#include "fall_detector.h"
#include "config.h"

// === Static Shared ORT Environment ===
Ort::Env& FallDetector::getEnv() {
	static Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "FallDetector");

	return env;
}

// === Shared Session per Model Path (thread-safe) ===
std::shared_ptr<Ort::Session> FallDetector::getSharedSession(const std::string& model_path) {
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
FallDetector::FallDetector(const std::string& model_path) {
	try {
		session = getSharedSession(model_path);
		runWarmUp();
	}
	catch (const Ort::Exception& e) {
		std::cerr << "[FallDetector::FallDetector] Failed to initialize session: " << e.what() << std::endl;
		session = nullptr;
	}
}

// === Public Inference Entry Point ===
std::vector<FallInfo> FallDetector::detect(const cv::Mat& image) {
	std::vector<FallInfo> falls;

	try {
		if (!session) {
			std::cerr << "[FallDetector::detect] Skipping detection: session not initialized." << std::endl;

			return falls;
		}

		float scale;
		int top, left;
		std::vector<float> input_tensor = preprocessYoloInput(image, scale, top, left);
		Ort::Value output = runYoloInference(input_tensor);

		std::vector<FallInfo> detections;
		postprocessYoloOutput(output, scale, top, left, detections);

		for (auto& det : detections) {
			const float ar = static_cast<float>(det.bbox.width) / det.bbox.height;

			if (ar > 1.125f) {
				det.pred = 0;  // FALL
				det.svm_conf = std::min(1.0f, ar / 2.0f);
			}
			else {
				det.pred = 1;  // SAFE
				det.svm_conf = 1.0f - ar;
			}		

			falls.push_back(det);
		}
	}
	catch (const std::exception& e) {
		std::cerr << "[FallDetector::detect] Error: " << e.what() << std::endl;
	}

	return falls;
}

// === Get Centers of Fall BBoxes ===
std::vector<cv::Point> FallDetector::getFallCenters(const std::vector<FallInfo>& fall_info) {
	std::vector<cv::Point> centers;

	for (const auto& info : fall_info) {
		if (info.pred == 0) {
			const cv::Point center = info.bbox.tl() + cv::Point(info.bbox.width / 2, info.bbox.height / 2);
			centers.emplace_back(center);
		}
	}

	return centers;
}

// === Warm-up dummy inference ===
void FallDetector::runWarmUp() {
	try {
		cv::Mat dummy(YOLO_INPUT_HEIGHT, YOLO_INPUT_WIDTH, CV_8UC3, cv::Scalar(114, 114, 114));
		detect(dummy);
	}
	catch (const std::exception& e) {
		std::cerr << "[FallDetector::runWarmUp] Warm-up failed: " << e.what() << std::endl;
	}
}

// === Preprocess YOLO input ===
std::vector<float> FallDetector::preprocessYoloInput(const cv::Mat& image, float& scale, int& top, int& left) {
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
Ort::Value FallDetector::runYoloInference(const std::vector<float>& input_tensor) {
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

// === Postprocess output tensor to get fall ===
void FallDetector::postprocessYoloOutput(Ort::Value& output_tensor, float scale, int top, int left, std::vector<FallInfo>& results) {
	float* data = output_tensor.GetTensorMutableData<float>();
	if (!data) throw std::runtime_error("Output tensor data is null.");

	const auto shape = output_tensor.GetTensorTypeAndShapeInfo().GetShape();
	size_t num_classes = shape[1] - 4;
	size_t num_boxes = shape[2];

	std::vector<cv::Rect> boxes;
	std::vector<float> scores;

	for (size_t i = 0; i < num_boxes; ++i) {
		float x = data[i + 0 * num_boxes];
		float y = data[i + 1 * num_boxes];
		float w = data[i + 2 * num_boxes];
		float h = data[i + 3 * num_boxes];

		float max_conf = 0.0f;
		int cls = -1;

		for (size_t c = 0; c < num_classes; ++c) {
			float score = data[i + (4 + c) * num_boxes];
			if (score > max_conf) {
				max_conf = score;
				cls = static_cast<int>(c);
			}
		}

		if (max_conf > FALL_CONF_THRESHOLD && cls == FALL_PERSON_CLASS_ID) {
			cv::Rect box = convertXywhToXyxy(x, y, w, h);

			int adj_x = static_cast<int>(std::round((box.x - left) / scale));
			int adj_y = static_cast<int>(std::round((box.y - top) / scale));
			int adj_w = static_cast<int>(std::round(box.width / scale));
			int adj_h = static_cast<int>(std::round(box.height / scale));

			boxes.emplace_back(adj_x, adj_y, adj_w, adj_h);
			scores.push_back(max_conf);
		}
	}

	std::vector<int> keep = applyNms(boxes, scores);

	for (int idx : keep) {
		FallInfo info;
		info.bbox = boxes[idx];
		info.yolo_conf = scores[idx];
		info.class_id = FALL_PERSON_CLASS_ID;
		results.push_back(info);
	}
}

// === NMS ===
std::vector<int> FallDetector::applyNms(const std::vector<cv::Rect>& boxes, const std::vector<float>& scores) {
	std::vector<int> indices;
	cv::dnn::NMSBoxes(boxes, scores, FALL_CONF_THRESHOLD, NMS_THRESHOLD, indices);

	return indices;
}