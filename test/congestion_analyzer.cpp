// Standard Library
#include <iostream>
#include <stdexcept>
#include <cmath>

// Project headers
#include "congestion_analyzer.h"

// === Constructor ===
CongestionAnalyzer::CongestionAnalyzer(int image_width, int image_height)
    : image_size(image_width, image_height),
    grid_size(GRID_CELL_SIZE),
    decay_alpha(CONGESTION_DECAY_ALPHA),
    influence_radius(CONGESTION_INFLUENCE_RADIUS) {
}

// === Analyze Congestion based on Crowd Information ===
std::vector<std::vector<float>> CongestionAnalyzer::analyzeCongestionGrid(const std::vector<cv::Point>& crowd_pixel) {
    std::vector<std::vector<float>> empty_result;

    try {
        if (image_size.width <= 0 || image_size.height <= 0 || grid_size <= 0) throw std::runtime_error("Invalid image or grid size.");

        const int rows = image_size.height / grid_size;
        const int cols = image_size.width / grid_size;
        std::vector<std::vector<float>> congestion(rows, std::vector<float>(cols, 0.0f));

        for (const auto& person : crowd_pixel) {
            const cv::Point grid_center = toGrid(person);

            for (int dy = -influence_radius; dy <= influence_radius; ++dy) {
                for (int dx = -influence_radius; dx <= influence_radius; ++dx) {
                    const int gx = grid_center.x + dx;
                    const int gy = grid_center.y + dy;
                    if (gx < 0 || gx >= cols || gy < 0 || gy >= rows) continue;

                    const float dist = std::hypot(static_cast<float>(dx), static_cast<float>(dy));
                    const float weight = std::exp(-decay_alpha * dist);
                    congestion[gy][gx] += weight;
                }
            }
        }

        return congestion;
    }
    catch (const std::exception& e) {
        std::cerr << "[CongestionAnalyzer::analyzeCongestionGrid] Error: " << e.what() << std::endl;

        return empty_result;
    }
}

// === Calculate Average Congestion Around Small Area ===
float CongestionAnalyzer::calculateAverageCongestionAround(const cv::Point& center_pixel, int radius) {
    const cv::Point grid_center = toGrid(center_pixel);

    const int rows = image_size.height / grid_size;
    const int cols = image_size.width / grid_size;

    float sum = 0.0f;
    int count = 0;

    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            const int gx = grid_center.x + dx;
            const int gy = grid_center.y + dy;
            if (gx < 0 || gx >= cols || gy < 0 || gy >= rows) continue;

            const float dist = std::hypot(static_cast<float>(dx), static_cast<float>(dy));
            const float weight = std::exp(-decay_alpha * dist);
            sum += weight;
            ++count;
        }
    }

    return (count > 0) ? (sum / static_cast<float>(count)) : 0.0f;
}