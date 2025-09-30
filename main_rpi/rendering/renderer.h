#ifndef RENDERER_H
#define RENDERER_H

// Project headers
#include "fall_info.h"
#include "crowd_info.h"
#include "path_finder.h"

/**
 * @brief Responsible for rendering visualization overlays onto frames.
 */
class Renderer {
public:
    Renderer() = default;
    ~Renderer() = default;

    void drawFallBoxes(cv::Mat& image, const std::vector<FallInfo>& fall_info);
    void drawCrowdBoxes(cv::Mat& image, const std::vector<CrowdInfo>& crowd_info);
    void drawFall(cv::Mat& image, const std::vector<cv::Point>& people, int radius = 5);
    void drawCrowd(cv::Mat& image, const std::vector<cv::Point>& people, int radius = 5);
    void drawCongestionHeatmap(cv::Mat& image, const std::vector<std::vector<float>>& congestion_grid_map);
    void drawExits(cv::Mat& image, const std::vector<Exit>& exits);
    void drawPath(cv::Mat& image, const std::vector<cv::Point>& path, const cv::Point& start_pixel, const Exit& exit);
};

#endif  // RENDERER_H