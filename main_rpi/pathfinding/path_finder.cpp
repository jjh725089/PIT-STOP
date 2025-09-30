// Standard Library
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <algorithm>
#include <limits>
#include <memory>
#include <queue>

// Project headers
#include "path_finder.h"
#include "config.h"

// === Constructor ===
Pathfinder::Pathfinder(int image_width, int image_height)
	: image_size(image_width, image_height) {
}

// === Sets the Internal Congestion Map ===
void Pathfinder::setCongestionMap(const std::vector<std::vector<float>>& congestion_grid_map) {
	this->congestion_grid_map = congestion_grid_map;
}

// === Generate Path Information ===
PathInfo Pathfinder::generatePathInfo(const cv::Point& incident_location_pixel, const Exit& exit, CongestionAnalyzer& analyzer, const int& exit_outer_crowd_counts) {
	PathInfo path_info;
	path_info.exit = exit;
	path_info.exit_inner_congestion = calculateExitInnerCongestion(exit.location, analyzer);
	path_info.exit_outer_congestion = calculateExitOuterCongestion(exit_outer_crowd_counts);
	path_info.path = calculatePath(incident_location_pixel, exit.location);
	path_info.path_cost = calculatePathCost(path_info.path);
	path_info.score = calculateScore(path_info.path_cost, path_info.exit_inner_congestion, path_info.exit_outer_congestion);

	return path_info;
}

// === Calculate Inner Congestion around Exit ===
float Pathfinder::calculateExitInnerCongestion(const cv::Point& exit_location_pixel, CongestionAnalyzer& analyzer) {
	float max_inner_congestion = 1e-6f;
	float inner_congestion = analyzer.calculateAverageCongestionAround(exit_location_pixel, 2);

	inner_congestion = std::max(max_inner_congestion, inner_congestion);

	return inner_congestion;
}

// === Calculate Outer Congestion around Exit ===
float Pathfinder::calculateExitOuterCongestion(const int& exit_outer_crowd_counts) {
	float max_outer_congestion = 1e-6f;
	float outer_congestion = static_cast<float>(exit_outer_crowd_counts);

	outer_congestion = std::max(max_outer_congestion, outer_congestion);

	return outer_congestion / 10.0f;
}

// === A* Pathfinding from Start to Goal ===
std::vector<cv::Point> Pathfinder::calculatePath(const cv::Point& start_pixel, const cv::Point& goal_pixel) {
	std::vector<cv::Point> empty;

	try {
		if (congestion_grid_map.empty() || congestion_grid_map[0].empty()) throw std::runtime_error("Congestion map is empty.");

		int rows = static_cast<int>(congestion_grid_map.size());
		int cols = static_cast<int>(congestion_grid_map[0].size());

		struct Node {
			int x, y;
			float g, h;
			std::shared_ptr<Node> parent;
			float f() const { return g + h; }
		};

		auto cmp = [](const std::shared_ptr<Node>& a, const std::shared_ptr<Node>& b) { return a->f() > b->f(); };
		std::priority_queue<std::shared_ptr<Node>, std::vector<std::shared_ptr<Node>>, decltype(cmp)> open(cmp);
		std::vector<std::vector<bool>> visited(rows, std::vector<bool>(cols, false));

		cv::Point grid_start = toGrid(start_pixel);
		cv::Point grid_goal = toGrid(goal_pixel);

		float initial_h = static_cast<float>(cv::norm(grid_start - grid_goal));
		open.push(std::make_shared<Node>(Node{ grid_start.x, grid_start.y, 0.0f, initial_h, nullptr }));

		const std::vector<cv::Point> directions = { {1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {-1, 1}, {-1, -1}, {1, -1} };

		while (!open.empty()) {
			auto current = open.top();
			open.pop();

			if (current->x == grid_goal.x && current->y == grid_goal.y) {
				std::vector<cv::Point> path;
				for (auto n = current; n; n = n->parent) {
					path.push_back(toPixelCenter(cv::Point(n->x, n->y)));
				}
				std::reverse(path.begin(), path.end());
				return path;
			}

			if (current->x < 0 || current->y < 0 || current->x >= cols || current->y >= rows) continue;
			if (visited[current->y][current->x]) continue;
			visited[current->y][current->x] = true;

			for (const auto& d : directions) {
				int nx = current->x + d.x;
				int ny = current->y + d.y;

				if (nx < 0 || ny < 0 || nx >= cols || ny >= rows) continue;
				if (visited[ny][nx]) continue;

				float congestion = congestion_grid_map[ny][nx];
				float dist = std::hypot(static_cast<float>(d.x), static_cast<float>(d.y));
				float cost = dist * (1.0f + congestion);

				float g = current->g + cost;
				float h = static_cast<float>(cv::norm(cv::Point(nx, ny) - grid_goal));

				open.push(std::make_shared<Node>(Node{ nx, ny, g, h, current }));
			}
		}
	}
	catch (const std::exception& e) {
		std::cerr << "[Pathfinder::findPath] Error: " << e.what() << std::endl;
	}

	return empty;
}

// === Calculates Cost of a Path based on Congestion and Distance ===
float Pathfinder::calculatePathCost(const std::vector<cv::Point>& path) {
	float max_path_cost = 1e-6f;
	float path_cost = 0.0f;

	for (size_t i = 1; i < path.size(); ++i) {
		float dist = cv::norm(path[i] - path[i - 1]);

		cv::Point grid_pt = toGrid(path[i]);
		int gx = grid_pt.x;
		int gy = grid_pt.y;

		float congestion = (gy >= 0 && gy < static_cast<int>(congestion_grid_map.size()) && gx >= 0 && gx < static_cast<int>(congestion_grid_map[0].size())) ? congestion_grid_map[gy][gx] : 0.0f;
		path_cost += dist * (1.0f + congestion);
	}

	path_cost = std::max(max_path_cost, path_cost);

	return path_cost / 100.0f;
}

// === Calculates score ===
float Pathfinder::calculateScore(const float& path_cost, const float& exit_inner_congestion, const float& exit_outer_congestion) {
	float alpha = (exit_inner_congestion > PATH_INNER_CONGESTION_THRESHOLD) ? PATH_ALPHA_LOW : PATH_ALPHA_HIGH;
	float score = alpha * path_cost + (1.0f - alpha) * exit_inner_congestion + PATH_EXIT_OUTER_WEIGHT * exit_outer_congestion;

	return score;
}