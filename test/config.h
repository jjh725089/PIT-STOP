#ifndef CONFIG_H
#define CONFIG_H

// OpenCV
#include <opencv2/opencv.hpp>

// YOLO Input
constexpr int YOLO_INPUT_WIDTH = 640;
constexpr int YOLO_INPUT_HEIGHT = 640;
constexpr int INPUT_LONG_SIDE_TARGET = 640;

// Fall Detection Model
constexpr const char* FALL_MODEL_PATH = "fall.onnx";
constexpr float FALL_CONF_THRESHOLD = 0.3125f;
constexpr int FALL_PERSON_CLASS_ID = 0;

// Crowd Detection Model
constexpr const char* CROWD_MODEL_PATH = "crowd.onnx";
constexpr float CROWD_CONF_THRESHOLD = 0.25f;
constexpr float NMS_THRESHOLD = 0.4375f;

// Grid & Congestion
constexpr int GRID_CELL_SIZE = 20;
constexpr float CONGESTION_DECAY_ALPHA = 0.0625f;
constexpr int CONGESTION_INFLUENCE_RADIUS = 2;

// Pathfinding Parameters
constexpr float PATH_ALPHA_HIGH = 0.625f;
constexpr float PATH_ALPHA_LOW = 0.25f;
constexpr float PATH_EXIT_OUTER_WEIGHT = 0.625f;
constexpr float PATH_INNER_CONGESTION_THRESHOLD = 5.0f;

// Rendering
constexpr float RENDERER_HEATMAP_ALPHA = 0.625f;
constexpr float RENDERER_HEATMAP_GAMMA = 0.5f;
constexpr int RENDERER_HEATMAP_BLUR = 9;

/**
 * @brief Converts pixel coordinates to grid coordinates
 * @param Pixel point
 * @return Corresponding grid point
 */
inline cv::Point toGrid(const cv::Point& pixel_pt) {
    return cv::Point(pixel_pt.x / GRID_CELL_SIZE,
        pixel_pt.y / GRID_CELL_SIZE);
}

/**
 * @brief Converts grid coordinates to center pixel position
 * @param Grid point
 * @return Center of corresponding pixel block
 */
inline cv::Point toPixelCenter(const cv::Point& grid_pt) {
    return cv::Point(grid_pt.x * GRID_CELL_SIZE + GRID_CELL_SIZE / 2,
        grid_pt.y * GRID_CELL_SIZE + GRID_CELL_SIZE / 2);
}

/**
 * @brief Converts grid coordinates to original (top-left) pixel position
 * @param Grid point
 * @return Top-left of corresponding pixel block
 */
inline cv::Point toPixelOrigin(const cv::Point& grid_pt) {
    return cv::Point(grid_pt.x * GRID_CELL_SIZE,
        grid_pt.y * GRID_CELL_SIZE);
}

/**
 * @brief Resize image with padding to match YOLO input size while preserving aspect ratio.
 * @param Input image
 * @param Scale factor used
 * @param Top padding in pixels
 * @param Left padding in pixels
 * @return Letterboxed image
 */
inline cv::Mat letterbox(const cv::Mat& src, float& scale, int& top, int& left) {
    int src_w = src.cols;
    int src_h = src.rows;

    float long_side = static_cast<float>(std::max(src_w, src_h));
    scale = static_cast<float>(INPUT_LONG_SIDE_TARGET) / long_side;

    int new_w = static_cast<int>(src_w * scale);
    int new_h = static_cast<int>(src_h * scale);

    left = (YOLO_INPUT_WIDTH - new_w) / 2;
    top = (YOLO_INPUT_HEIGHT - new_h) / 2;

    cv::Mat resized;
    cv::resize(src, resized, cv::Size(new_w, new_h));

    cv::Mat padded(YOLO_INPUT_HEIGHT, YOLO_INPUT_WIDTH, src.type(), cv::Scalar(114, 114, 114));
    resized.copyTo(padded(cv::Rect(left, top, new_w, new_h)));

    return padded;
}

/**
 * @brief Converts YOLO (x, y, w, h) format to OpenCV Rect (x1, y1, width, height)
 * @param Center x
 * @param Center y
 * @param Width
 * @param Height
 * @return OpenCV Rect
 */
inline cv::Rect convertXywhToXyxy(float x, float y, float w, float h) {
    int left = static_cast<int>(std::round(x - w / 2));
    int top = static_cast<int>(std::round(y - h / 2));
    int width = static_cast<int>(std::round(w));
    int height = static_cast<int>(std::round(h));
    return cv::Rect(left, top, width, height);
}

#endif  // CONFIG_H