#ifndef CROWD_DETECTOR_H
#define CROWD_DETECTOR_H

// Standard Library
#include <memory>
#include <vector>
#include <string>

// ONNX Runtime
#include <onnxruntime_cxx_api.h>

// Project headers
#include "crowd_info.h"

/**
 * @brief Performs crowd detection using a YOLO-based ONNX model.
 */
class CrowdDetector {
public:
    /**
     * @brief Constructor with model path
     * @param Path to YOLO ONNX model
     */
    explicit CrowdDetector(const std::string& model_path);

    ~CrowdDetector() = default;

    /**
     * @brief Perform crowd detection on an input image
     * @param Input BGR image
     * @return List of crowd information
     */
    std::vector<CrowdInfo> detect(const cv::Mat& image);

    /**
     * @brief Get center points of bboxes
     * @param Vector of crowd information
     * @return List of center points
     */
    std::vector<cv::Point> getCrowdCenters(const std::vector<CrowdInfo>& crowd_info);

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
    void postprocessYoloOutput(Ort::Value& output_tensor, float scale, int top, int left, std::vector<CrowdInfo>& results);

    // === Utilities ===
    std::vector<int> applyNms(const std::vector<cv::Rect>& boxes, const std::vector<float>& scores);
    float computeIoU(const cv::Rect& a, const cv::Rect& b);

    // === Members ===
    std::shared_ptr<Ort::Session> session;
};

#endif  // CROWD_DETECTOR_H