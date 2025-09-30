#ifndef FALL_DETECTOR_H
#define FALL_DETECTOR_H

// Standard Library
#include <memory>
#include <vector>
#include <string>

// ONNX Runtime
#include <onnxruntime_cxx_api.h>

// Project headers
#include "fall_info.h"

/**
 * @brief Fall detection class using YOLO model via ONNX Runtime
 */
class FallDetector {
public:
	/**
	 * @brief Constructor with model path
	 * @param Path to YOLO ONNX model
	 */
	explicit FallDetector(const std::string& model_path);

	~FallDetector() = default;

	/**
	 * @brief Perform fall detection on an input image
	 * @param Input BGR image
	 * @return List of fall information
	 */
	std::vector<FallInfo> detect(const cv::Mat& image);

	/**
	 * @brief Get center points of bboxes
	 * @param Vector of fall information
	 * @return List of center points
	 */
	std::vector<cv::Point> getFallCenters(const std::vector<FallInfo>& fall_info);

	/**
	 * @brief Run warm-up inference to initialize ONNX engine
	 */
	void runWarmUp();

private:
	// === Inference core ===
	static Ort::Env& getEnv();
	static std::shared_ptr<Ort::Session> getSharedSession(const std::string& model_path);
	Ort::Value runYoloInference(const std::vector<float>& input_tensor);

	// === Pre/Post-processing ===
	std::vector<float> preprocessYoloInput(const cv::Mat& image, float& scale, int& top, int& left);
	void postprocessYoloOutput(Ort::Value& output_tensor, float scale, int top, int left, std::vector<FallInfo>& results);

	// === Utilities ===
	std::vector<int> applyNms(const std::vector<cv::Rect>& boxes, const std::vector<float>& scores);

	// === Members ===
	std::shared_ptr<Ort::Session> session;
};

#endif  // FALL_DETECTOR_H