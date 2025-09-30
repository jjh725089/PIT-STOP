#ifndef PATH_FINDER_H
#define PATH_FINDER_H

// Standard Library
#include <vector>

// OpenCV
#include <opencv2/core.hpp>

// Project headers
#include "congestion_analyzer.h"

/**
 * @brief Represents an exit point in the environment.
 */
struct Exit {
    cv::Point location;
    size_t index;
};

/**
 * @brief Represents path information.
 */
struct PathInfo {
    Exit exit;
    float exit_inner_congestion = 0.0f;
    float exit_outer_congestion = 0.0f;
    std::vector<cv::Point> path;
    float path_cost = 0.0f;
    float score = 0.0f;
};

/**
 * @brief A* pathfinding class considering congestion data.
 */
class Pathfinder {
public:
    /**
     * @brief Constructs a Pathfinder with image dimensions.
     * @param Width of the image in pixels.
     * @param Height of the image in pixels.
     */
    Pathfinder(int image_width, int image_height);

    ~Pathfinder() = default;

    /**
     * @brief Sets the internal congestion map.
     * @param 2D vector of congestion values.
     */
    void setCongestionMap(const std::vector<std::vector<float>>& congestion_grid_map);

    /**
     * @brief Generates path information.
     * @param Incident pixel location.
     * @param Exit information.
     * @param Congestion analyzer.
     * @param Outer crowd counts of exit.
     * @return A structure of path information.
     */
    PathInfo generatePathInfo(const cv::Point& incident_location_pixel, const Exit& exit, CongestionAnalyzer& analyzer, const int& exit_outer_crowd_counts);

    // === Utilities ===
    float calculateExitInnerCongestion(const cv::Point& exit_location, CongestionAnalyzer& analyzer);
    float calculateExitOuterCongestion(const int& exit_outer_crowd_counts);
    std::vector<cv::Point> calculatePath(const cv::Point& start_pixel, const cv::Point& goal_pixel);
    float calculatePathCost(const std::vector<cv::Point>& path);
    float calculateScore(const float& path_cost, const float& exit_inner_congestion, const float& exit_outer_congestion);

private:
    // === Members ===
    cv::Size image_size;
    std::vector<std::vector<float>> congestion_grid_map;
};

#endif  // PATH_FINDER_H