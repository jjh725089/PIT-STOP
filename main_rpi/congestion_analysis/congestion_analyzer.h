#ifndef CONGESTION_ANALYZER_H
#define CONGESTION_ANALYZER_H

// Standard Library
#include <vector>

// Project headers
#include "config.h"

/**
 * @brief Computes a congestion heatmap based on detected crowd positions.
 */
class CongestionAnalyzer {
public:
    /**
     * @brief Constructs a congestion analyzer with the image resolution.
     * @param Width of the input image
     * @param Height of the input image
     */
    CongestionAnalyzer(int image_width, int image_height);

    ~CongestionAnalyzer() = default;

    /**
     * @brief Analyzes the congestion grid based on current crowd locations.
     * @param Vector of crowd positions in pixel coordinates
     * @return A 2D float matrix representing the congestion weight per grid cell
     */
    std::vector<std::vector<float>> analyzeCongestionGrid(const std::vector<cv::Point>& crowd_pixel);

    /**
     * @brief Calculates the average congestion around a specific position.
     * @param Center pixel position
     * @param Radius in grid cells (default: CONGESTION_INFLUENCE_RADIUS)
     * @return Averaged congestion weight
     */
    float calculateAverageCongestionAround(const cv::Point& center_pixel, int radius = CONGESTION_INFLUENCE_RADIUS);

private:
    // === Members ===
    cv::Size image_size;
    int grid_size;
    float decay_alpha;
    int influence_radius;
};

#endif  // CONGESTION_ANALYZER_H