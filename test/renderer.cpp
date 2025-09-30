// Standard Library
#include <cmath>
#include <algorithm>
#include <iomanip>
#include <sstream>

// Project headers
#include "renderer.h"
#include "config.h"

// === Draw Fall Boxes ===
void Renderer::drawFallBoxes(cv::Mat& image, const std::vector<FallInfo>& fall_info) {
    // Compute resolution-aware rendering parameters
    const int box_thickness = std::max(3, image.cols / 800);
    const float font_scale = std::max(0.5f, image.cols / 800.0f);
    const int font_thickness = std::max(2, image.cols / 400);
    const int padding = std::max(2, image.cols / 400);

    for (const auto& f : fall_info) {
        if (f.pred == 0) {
            const cv::Scalar box_color(0, 0, 255); // Red color for fall boxes

            // Construct label text: "FALL <confidence>"
            std::string label = "FALL";
            char conf_buf[32];
            std::snprintf(conf_buf, sizeof(conf_buf), " %.2f", f.svm_conf);
            label += conf_buf;

            // Draw bounding box
            cv::rectangle(image, f.bbox, box_color, box_thickness);

            // Compute text size and origin
            int baseLine = 0;
            cv::Size label_size = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, font_scale, font_thickness, &baseLine);
            cv::Point label_origin(f.bbox.x, std::max(f.bbox.y - baseLine - padding, 0));

            // Define label background rectangle with padding
            cv::Point bg_tl = label_origin + cv::Point(-padding, baseLine);
            cv::Point bg_br = label_origin + cv::Point(label_size.width + padding, -label_size.height - padding);

            // Clip to image bounds
            cv::Rect roi(bg_tl, bg_br);
            roi &= cv::Rect(0, 0, image.cols, image.rows);

            // Determine text color based on background brightness
            cv::Scalar bg_mean = cv::mean(image(roi));
            float luminance = 0.299f * bg_mean[2] + 0.587f * bg_mean[1] + 0.114f * bg_mean[0];
            cv::Scalar text_color = (luminance > 150.0f) ? cv::Scalar(0, 0, 0) : cv::Scalar(255, 255, 255);

            // Draw label background and text
            cv::rectangle(image, bg_tl, bg_br, box_color, cv::FILLED);
            cv::putText(image, label, label_origin, cv::FONT_HERSHEY_SIMPLEX, font_scale, text_color, font_thickness, cv::LINE_AA);
        }
    }
}

// === Draw Crowd Boxes (resolution-aware thickness) ===
void Renderer::drawCrowdBoxes(cv::Mat& image, const std::vector<CrowdInfo>& crowd_info) {
    // Compute resolution-aware rendering parameters
    const int box_thickness = std::max(3, image.cols / 800);
    const cv::Scalar box_color(0, 255, 0); // Green color for crowd boxes

    for (const auto& c : crowd_info) {
        // Draw bounding box
        cv::rectangle(image, c.bbox, box_color, box_thickness);
    }
}

// === Draw Fall Points ===
void Renderer::drawFall(cv::Mat& image, const std::vector<cv::Point>& people, int radius) {
    int r = std::max(radius, image.cols / 400);

    for (const auto& pt : people) {
        cv::circle(image, pt, r, cv::Scalar(0, 0, 255), cv::FILLED, cv::LINE_AA);
    }
}

// === Draw Crowd Points ===
void Renderer::drawCrowd(cv::Mat& image, const std::vector<cv::Point>& people, int radius) {
    int r = std::max(radius, image.cols / 400);

    for (const auto& pt : people) {
        cv::circle(image, pt, r, cv::Scalar(0, 255, 0), cv::FILLED, cv::LINE_AA);
    }
}

// === Draw Congestion Heatmap ===
void Renderer::drawCongestionHeatmap(cv::Mat& image, const std::vector<std::vector<float>>& congestion_grid_map) {
    try {
        if (congestion_grid_map.empty() || congestion_grid_map[0].empty()) throw std::runtime_error("Congestion map is empty.");

        const int rows = static_cast<int>(congestion_grid_map.size());
        const int cols = static_cast<int>(congestion_grid_map[0].size());

        cv::Mat float_map(rows, cols, CV_32FC1);
        float max_val = 0.0f;

        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < cols; ++x) {
                float val = congestion_grid_map[y][x];
                float_map.at<float>(y, x) = val;
                max_val = std::max(max_val, val);
            }
        }

        // Upscale to match original image resolution
        cv::Mat upscaled;
        cv::resize(float_map, upscaled, image.size(), 0, 0, cv::INTER_CUBIC);

        // Normalize to [0,1]
        if (max_val > 1e-6f) {
            upscaled /= max_val;
        }
        else {
            upscaled.setTo(0);
        }

        // Gamma correction
        cv::pow(upscaled, RENDERER_HEATMAP_GAMMA, upscaled);

        // Blur
        cv::GaussianBlur(upscaled, upscaled, cv::Size(RENDERER_HEATMAP_BLUR, RENDERER_HEATMAP_BLUR), 0);

        // Convert to 8-bit heatmap
        cv::Mat heatmap8U, heatmapColor;
        upscaled.convertTo(heatmap8U, CV_8UC1, 255.0);
        cv::applyColorMap(heatmap8U, heatmapColor, cv::COLORMAP_JET);

        // Blend heatmap with image
        const float alpha = std::clamp(RENDERER_HEATMAP_ALPHA, 0.0f, 1.0f);
        cv::addWeighted(image, alpha, heatmapColor, 1.0f - alpha, 0.0, image); // Blend: image * alpha + heatmap * (1 - alpha)
    }
    catch (const std::exception& e) {
        std::cerr << "[Renderer::drawCongestionHeatmap] Error: " << e.what() << std::endl;
    }
}

void Renderer::drawExits(cv::Mat& image, const std::vector<Exit>& exits) {
    const int circle_radius = std::max(5, image.cols / 300);
    const int thickness = std::max(2, image.cols / 400);
    const float font_scale = image.cols / 800.0f;

    // Linear interpolation between start and end
    auto interpolate_color = [](float t) -> cv::Scalar { t = std::clamp(t, 0.0f, 1.0f); return cv::Scalar(255 * (1 - t), 255 * (1 - t), 255 * t); };

    for (const auto& exit : exits) {
        // Draw exit circle
        cv::circle(image, exit.location, circle_radius, interpolate_color(1.0f), cv::FILLED, cv::LINE_AA);

        const std::string exit_label = "Exit " + std::to_string(exit.index + 1);

        // Determine text size
        int baseline = 0;
        cv::Size label_size = cv::getTextSize(exit_label, cv::FONT_HERSHEY_SIMPLEX, font_scale, thickness, &baseline);

        // Position label above or below the point
        cv::Point exit_text_pos(exit.location.x, exit.location.y - circle_radius - 5);
        exit_text_pos.x = std::clamp(exit_text_pos.x, 0, image.cols - label_size.width - 1);
        exit_text_pos.y = std::clamp(exit_text_pos.y, label_size.height + 1, image.rows - 1);

        // Determine contrast-aware text color
        cv::Rect roi(exit_text_pos, label_size);
        roi &= cv::Rect(0, 0, image.cols, image.rows);
        cv::Scalar mean_color = roi.area() > 0 ? cv::mean(image(roi)) : cv::Scalar(0, 0, 0);
        float luminance = 0.299f * mean_color[2] + 0.587f * mean_color[1] + 0.114f * mean_color[0];
        cv::Scalar exit_text_color = (luminance > 150.0f) ? cv::Scalar(0, 0, 0) : cv::Scalar(255, 255, 255);

        // Draw label
        cv::putText(image, exit_label, exit_text_pos, cv::FONT_HERSHEY_SIMPLEX, font_scale, exit_text_color, thickness, cv::LINE_AA);
    }
}

// === Draw Evacuation Path ===
void Renderer::drawPath(cv::Mat& image, const std::vector<cv::Point>& path, const cv::Point& start_pixel, const Exit& exit) {
    const int circle_radius = std::max(5, image.cols / 300);
    const int thickness = std::max(2, image.cols / 400);
    const float font_scale = image.cols / 800.0f;
    const int label_offset = std::max(20, image.cols / 50);

    // Linear interpolation between start and end
    auto interpolate_color = [](float t) -> cv::Scalar { t = std::clamp(t, 0.0f, 1.0f); return cv::Scalar(255 * (1 - t), 255 * (1 - t), 255 * t); };

    // Draw gradient path
    for (size_t i = 1; i < path.size(); ++i) {
        float t = static_cast<float>(i) / (path.size() - 1);
        cv::line(image, path[i - 1], path[i], interpolate_color(t), thickness, cv::LINE_AA);
    }

    // Draw start and exit
    cv::circle(image, start_pixel, circle_radius, interpolate_color(0.0f), cv::FILLED, cv::LINE_AA);
    cv::circle(image, exit.location, circle_radius, interpolate_color(1.0f), cv::FILLED, cv::LINE_AA);

    // Label strings
    const std::string start_label = "Start";

    // Compute label sizes
    int baseline = 0;
    const cv::Size start_size = cv::getTextSize(start_label, cv::FONT_HERSHEY_SIMPLEX, font_scale, thickness, &baseline);

    // Compute start direction and exit direction to set label
    cv::Point2f start_dir(0, 0);
    if (path.size() >= 2) {
        start_dir = path[1] - path[0];

        float norm_s = std::hypot(start_dir.x, start_dir.y);

        if (norm_s > 1e-3f) start_dir *= (1.0f / norm_s);
    }

    // Compute label positions away from path
    cv::Point start_text_pos = start_pixel - cv::Point(start_dir.x * label_offset, start_dir.y * label_offset);

    // Clamp to image boundaries
    start_text_pos.x = std::clamp(start_text_pos.x, 0, image.cols - start_size.width - 1);
    start_text_pos.y = std::clamp(start_text_pos.y, start_size.height + 1, image.rows - 1);

    // Auto-select text color by averaging local background luminance
    cv::Rect roi(start_text_pos, start_size);
    roi &= cv::Rect(0, 0, image.cols, image.rows);
    cv::Scalar mean_color = roi.area() > 0 ? cv::mean(image(roi)) : cv::Scalar(0, 0, 0);
    float luminance = 0.299f * mean_color[2] + 0.587f * mean_color[1] + 0.114f * mean_color[0];
    cv::Scalar start_text_color = (luminance > 150.0f) ? cv::Scalar(0, 0, 0) : cv::Scalar(255, 255, 255);

    // Draw text
    cv::putText(image, start_label, start_text_pos, cv::FONT_HERSHEY_SIMPLEX, font_scale, start_text_color, thickness, cv::LINE_AA);
}