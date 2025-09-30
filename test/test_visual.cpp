// Standard Library
#include <iostream>
#include <random>
#include <limits>

// Project Headers
#include "fall_detector.h"
#include "crowd_detector.h"
#include "congestion_analyzer.h"
#include "path_finder.h"
#include "renderer.h"
#include "config.h"

// Configuration constants
const int repeat_count = 10;
const int delay_ms = 500;

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

	// Create window
	const std::string window_title = "Test";
	cv::namedWindow(window_title, cv::WINDOW_NORMAL);
	cv::setWindowProperty(window_title, cv::WND_PROP_FULLSCREEN, cv::WINDOW_FULLSCREEN);
	cv::waitKey(100);

	// Configure Image Size
	int single_image_width = 640;
	int single_image_height = 640;
	cv::Size target_size(single_image_width, single_image_height);

	// Initialize modules
	FallDetector fall_detector(FALL_MODEL_PATH);
	CrowdDetector crowd_detector(CROWD_MODEL_PATH);
	CongestionAnalyzer analyzer(crowd_image.cols, crowd_image.rows);
	Pathfinder pathfinder(crowd_image.cols, crowd_image.rows);
	Renderer renderer;

	for (int i = 0; i < repeat_count; ++i)
	{
		cv::Mat bbox_image = fall_image.clone();
		cv::Mat dot_image = crowd_image.clone();
		cv::Mat heatmap_image = crowd_image.clone();
		cv::Mat path_image = crowd_image.clone();

		// Fall Detection
		auto fall_info = fall_detector.detect(bbox_image);

		// Crowd Detection
		auto crowd_info = crowd_detector.detect(dot_image);
		auto crowd_location = crowd_detector.getCrowdCenters(crowd_info);

		// Congestion Analysis
		auto congestion_grid = analyzer.analyzeCongestionGrid(crowd_location);
		if (congestion_grid.empty())
		{
			std::cerr << "[Warning] Empty congestion grid map" << std::endl;

			continue;
		}

		// Pathfinding
		pathfinder.setCongestionMap(congestion_grid);

		PathInfo best_path_info = {};
		best_path_info.score = std::numeric_limits<float>::max();

		for (size_t i = 0; i < exits.size(); ++i)
		{
			PathInfo path_info = pathfinder.generatePathInfo(start_pixel, exits.at(i), analyzer, exit_outer_crowd_counts.at(i));
			paths_info.push_back(path_info);

			if (best_path_info.score > path_info.score) best_path_info = path_info;
		}

		if (best_path_info.path.empty())
		{
			std::cerr << "[Warning] No viable path found" << std::endl;
			continue;
		}

		// Rendering
		renderer.drawFallBoxes(bbox_image, fall_info);
		renderer.drawCrowd(dot_image, crowd_location);
		renderer.drawCongestionHeatmap(heatmap_image, congestion_grid);
		renderer.drawExits(path_image, exits);
		renderer.drawPath(path_image, best_path_info.path, start_pixel, best_path_info.exit);

		cv::resize(bbox_image, bbox_image, target_size);
		cv::resize(dot_image, dot_image, target_size);
		cv::resize(path_image, path_image, target_size);
		cv::resize(heatmap_image, heatmap_image, target_size);

		// Visualization
		cv::Mat top, bottom, display;
		cv::hconcat(std::vector<cv::Mat>{bbox_image, path_image}, top);
		cv::hconcat(std::vector<cv::Mat>{dot_image, heatmap_image}, bottom);
		cv::vconcat(std::vector<cv::Mat>{top, bottom}, display);

		cv::imshow(window_title, display);

		paths_info.clear();

		// Wait or quit
		char key = (char)cv::waitKey(delay_ms);
		if (key == 'q') break;
	}

	cv::destroyWindow(window_title);
	cv::waitKey(1);

	return 0;
}