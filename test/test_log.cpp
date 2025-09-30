// Standard Library
#include <iostream>
#include <random>
#include <limits>
#include <iomanip>
#include <chrono>

// Project Headers
#include "fall_detector.h"
#include "crowd_detector.h"
#include "congestion_analyzer.h"
#include "path_finder.h"
#include "renderer.h"
#include "config.h"

// Tolerances for fuzzy comparisons
constexpr float COORD_TOLERANCE = 0.0f;
constexpr float GRID_TOLERANCE = 0.0f;
constexpr float PATH_TOLERANCE = 0.0f;

// Configuration constants
const int repeat_count = 10;

// Helper comparison functions
bool pointNear(const cv::Point& point_a, const cv::Point& point_b, float coord_tolerance = COORD_TOLERANCE);
bool compareDetectFuzzy(const std::vector<cv::Point>& detect_a, const std::vector<cv::Point>& detect_b, float detect_tolerance = COORD_TOLERANCE);
bool compareGridFuzzy(const std::vector<std::vector<float>>& grid_a, const std::vector<std::vector<float>>& grid_b, float grid_tolerance = GRID_TOLERANCE);
bool comparePathFuzzy(const std::vector<cv::Point>& path_a, const std::vector<cv::Point>& path_b, float path_tolerance = PATH_TOLERANCE);

int main()
{
	// Load the input image
	cv::Mat fall_image = cv::imread("fall00.png");
	if (fall_image.empty())
	{
		std::cerr << "Unable to read fall image" << std::endl;

		return -1;
	}

	cv::Mat crowd_image = cv::imread("crowd00.jpg");
	if (crowd_image.empty())
	{
		std::cerr << "Unable to read crowd image" << std::endl;

		return -1;
	}

	std::vector<int> exit_outer_crowd_counts = { 14, 7, 23, 10 };

	// Exit positions
	// Must be converted as ( pixel -> grid -> pixel center )
	int max_cols = crowd_image.cols / GRID_CELL_SIZE;
	int max_rows = crowd_image.rows / GRID_CELL_SIZE;

	std::vector<Exit> exits =
	{
		{ toPixelCenter(cv::Point(0, 0)), 0 },
		{ toPixelCenter(cv::Point(max_cols - 1, 0)), 1 },
		{ toPixelCenter(cv::Point(0, max_rows - 1)), 2 },
		{ toPixelCenter(cv::Point(max_cols - 1, max_rows - 1)), 3 }
	};

	std::vector<PathInfo> paths_info = {};

	// Random start location
	// Must be converted as ( pixel -> grid -> pixel center )
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> rand_x(0, max_cols - 1);
	std::uniform_int_distribution<> rand_y(0, max_rows - 1);
	cv::Point start_grid(rand_x(gen), rand_y(gen));
	cv::Point start_pixel = toPixelCenter(start_grid);

	// Reference values from the first run
	std::vector<cv::Point> reference_fall_detect;
	std::vector<cv::Point> reference_crowd_detect;
	std::vector<std::vector<float>> reference_congestion_grid;
	std::vector<cv::Point> reference_path_finding;

	// Initialize modules
	FallDetector fall_detector(FALL_MODEL_PATH);
	CrowdDetector crowd_detector(CROWD_MODEL_PATH);
	CongestionAnalyzer analyzer(crowd_image.cols, crowd_image.rows);
	Pathfinder pathfinder(crowd_image.cols, crowd_image.rows);

	for (int i = 0; i < repeat_count; ++i)
	{
		auto t0 = std::chrono::high_resolution_clock::now();

		// Fall Detection
		auto result_fall_detect_info = fall_detector.detect(fall_image);
		auto result_fall_detect_centers = fall_detector.getFallCenters(result_fall_detect_info);

		auto t1 = std::chrono::high_resolution_clock::now();

		// Crowd Detection
		auto result_crowd_detect_info = crowd_detector.detect(crowd_image);
		auto result_crowd_detect_centers = crowd_detector.getCrowdCenters(result_crowd_detect_info);

		auto t2 = std::chrono::high_resolution_clock::now();

		// Congestion Analysis
		auto result_congestion_grid = analyzer.analyzeCongestionGrid(result_crowd_detect_centers);
		if (result_congestion_grid.empty())
		{
			std::cerr << "[Warning] Empty congestion map" << std::endl;

			continue;
		}
		
		auto t3 = std::chrono::high_resolution_clock::now();

		// Pathfinding
		pathfinder.setCongestionMap(result_congestion_grid);

		PathInfo result_path_finding = {};
		result_path_finding.score = std::numeric_limits<float>::max();

		for (size_t i = 0; i < exits.size(); ++i)
		{
			PathInfo path_info = pathfinder.generatePathInfo(start_pixel, exits.at(i), analyzer, exit_outer_crowd_counts.at(i));
			paths_info.push_back(path_info);

			if (result_path_finding.score > path_info.score) result_path_finding = path_info;
		}

		if (result_path_finding.path.empty())
		{
			std::cerr << "[Warning] No viable path found" << std::endl;

			continue;
		}

		auto t4 = std::chrono::high_resolution_clock::now();

		auto elapsed_fall_detect = std::chrono::duration<float, std::milli>(t1 - t0).count();
		auto elapsed_crowd_detect = std::chrono::duration<float, std::milli>(t2 - t1).count();
		auto elapsed_congestion_grid = std::chrono::duration<float, std::milli>(t3 - t2).count();
		auto elapsed_path_finding = std::chrono::duration<float, std::milli>(t4 - t3).count();

		if (i == 0)
		{
			// Store baseline result for later comparisons
			reference_fall_detect = result_fall_detect_centers;
			reference_crowd_detect = result_crowd_detect_centers;
			reference_congestion_grid = result_congestion_grid;
			reference_path_finding = result_path_finding.path;

			std::cout << "[Run " << (i + 1) << "]" << std::endl;
			std::cout << "Fall detected: " << result_fall_detect_centers.size() << " (" << elapsed_fall_detect << " ms)" << std::endl;
			std::cout << "Crowd detected: " << result_crowd_detect_centers.size() << " (" << elapsed_crowd_detect << " ms)" << std::endl;
			std::cout << "Congestion analyzed (" << elapsed_congestion_grid << " ms)" << std::endl;
			std::cout << "Pathfinding done (" << elapsed_path_finding << " ms)" << std::endl;
		}
		else
		{
			// Compare with reference results
			bool same_fall_detect = compareDetectFuzzy(reference_fall_detect, result_fall_detect_centers);
			bool same_crowd_detect = compareDetectFuzzy(reference_crowd_detect, result_crowd_detect_centers);
			bool same_congestion_grid = compareGridFuzzy(reference_congestion_grid, result_congestion_grid);
			bool same_path_finding = comparePathFuzzy(reference_path_finding, result_path_finding.path);

			std::cout << "[Run " << (i + 1) << "]" << std::endl;
			std::cout << "Fall detected: " << result_fall_detect_centers.size() << " (" << elapsed_fall_detect << " ms)" << std::endl;
			std::cout << "Crowd detected: " << result_crowd_detect_centers.size() << " (" << elapsed_crowd_detect << " ms)" << std::endl;
			std::cout << "Congestion analyzed (" << elapsed_congestion_grid << " ms)" << std::endl;
			std::cout << "Pathfinding done (" << elapsed_path_finding << " ms)" << std::endl;

			if (!same_fall_detect || !same_crowd_detect || !same_congestion_grid || !same_path_finding)
			{
				std::cerr << std::endl;
				std::cerr << "[Error] Mismatch occured!" << std::endl;

				return -1;
			}
		}

		paths_info.clear();
		std::cout << std::endl;
	}

	std::cout << "All " << repeat_count << " runs produced consistent fall detection, crowd detection, congestion analysis, and path finding results." << std::endl;

	return 0;
}

// Euclidean distance-based point comparison
bool pointNear(const cv::Point& point_a, const cv::Point& point_b, float coord_tolerance)
{
	float dx = static_cast<float>(point_a.x - point_b.x);
	float dy = static_cast<float>(point_a.y - point_b.y);

	return std::sqrt(dx * dx + dy * dy) <= coord_tolerance;
}

// One-to-one matching of points
bool compareDetectFuzzy(const std::vector<cv::Point>& detect_a, const std::vector<cv::Point>& detect_b, float detect_tolerance)
{
	if (detect_a.size() != detect_b.size()) return false;

	std::vector<bool> matched(detect_b.size(), false);
	for (const auto& p : detect_a)
	{
		bool found = false;
		for (size_t i = 0; i < detect_b.size(); ++i)
		{
			if (!matched[i] && pointNear(p, detect_b[i], detect_tolerance))
			{
				matched[i] = true;
				found = true;

				break;
			}
		}
		if (!found) return false;
	}

	return true;
}

// Compare two congestion grids with tolerance
bool compareGridFuzzy(const std::vector<std::vector<float>>& grid_a, const std::vector<std::vector<float>>& grid_b, float grid_tolerance)
{
	if (grid_a.size() != grid_b.size()) return false;
	if (grid_a[0].size() != grid_b[0].size()) return false;

	for (size_t y = 0; y < grid_a.size(); ++y)
	{
		for (size_t x = 0; x < grid_a[0].size(); ++x)
		{
			if (std::abs(grid_a[y][x] - grid_b[y][x]) > grid_tolerance) return false;
		}
	}

	return true;
}

// Compare paths point-by-point
bool comparePathFuzzy(const std::vector<cv::Point>& path_a, const std::vector<cv::Point>& path_b, float path_tolerance)
{
	if (path_a.size() != path_b.size()) return false;

	for (size_t i = 0; i < path_a.size(); ++i)
	{
		if (!pointNear(path_a[i], path_b[i], path_tolerance)) return false;
	}

	return true;
}