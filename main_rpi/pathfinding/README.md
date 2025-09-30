# Pathfinder

## Overview

This module implements a congestion-aware A* pathfinding system designed for indoor evacuation and crowd-aware navigation. It calculates the optimal path from an incident location to a specified exit by evaluating multiple factors, including internal congestion near exits, external crowd density, and path traversal cost. The final score for each path reflects its overall suitability based on these weighted components.

## Author

Jooho Hwang

## Project Structure

- `path_finder.h`: Header file defining the PathFinder class interface.
- `path_finder.cpp`: Implementation of congestion-aware A* pathfinding, congestion calculations, and scoring logic.

## Installation & Dependencies

- OpenCV >= 4.6
- C++17 or later

## Key Components

### Pathfinder class

- `Constructor`: Initializes the pathfinder with a given image size (used for coordinate conversions).
- `setCongestionMap()`: Sets the internal congestion grid (2D float matrix).
- `getFallCenters()`: Extracts the center coordinates of fall detections.
- `generatePathInfo()`: Main method to compute the best path and its corresponding score using congestion data and path metrics.
- `calculateExitInnerCongestion()`: Calculates average congestion around the exit using the CongestionAnalyzer.
- `calculateExitOuterCongestion()`: Converts external crowd count to a normalized congestion score.
- `calculatePath()`: Uses the A* algorithm with congestion-weighted cost to compute a path between two points.
- `calculatePathCost()`: Computes total cost of a path based on congestion and distance.
- `calculateScore()`: Combines all factors (path cost, inner/outer congestion) into a final score using configurable weights.

### PathInfo structure

Stores the computed path, path cost, exit-related congestion, and overall score.

### Exit structure

Represents an exit point with position (cv::Point) and index.

## Notes

- A valid congestion map must be provided before calling generatePathInfo().
- CongestionAnalyzer must provide the method calculateAverageCongestionAround(...) for computing local congestion values.
- The system assumes a grid-aligned 2D space and does not include visualization or front-end integration.