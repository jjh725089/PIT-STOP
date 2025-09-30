# Renderer

## Overview

This module provides a set of rendering utilities for overlaying analytical results onto video frames. It supports drawing visual indicators for:

- Fall detection (bounding boxes with confidence scores)
- Crowd detection (bounding boxes and points)
- Congestion heatmaps (color-coded density maps)
- Evacuation paths (gradient-based route visualization)

All rendering functions are resolution-aware, dynamically adjusting thickness, font scale, and radius to ensure visual clarity on various image sizes.

## Author

Jooho Hwang

## Project Structure

- `renderer.h`: Header file defining the Renderer class interface.
- `renderer.cpp`: Implementation of rendering logic.

## Installation & Dependencies

- OpenCV >= 4.6
- C++17 or later
- The module is intended to be integrated into a larger system; no standalone executable is provided.

## Key Components

### Pathfinder class

- `drawFallBoxes()`: Draws red bounding boxes with confidence values for predicted fall events.
- `drawCrowdBoxes()`: Draws green bounding boxes indicating detected crowd regions.
- `drawFall()`: Renders red circles at positions where falls were detected.
- `drawCrowd()`: Renders green circles for people within detected crowd regions.
- `drawCongestionHeatmap()`: Converts a grid of congestion levels into a heatmap using gamma correction, blurring, and color mapping.
- `drawExits()`: Visualizes exits including labeled markers.
- `drawPath()`: Visualizes an evacuation path with a gradient line from the start point to an exit, including labeled markers.

## Notes

- Text color is dynamically chosen for readability based on the brightness of the background region.
- All drawing operations are intended to be called on OpenCV cv::Mat frames in BGR format.