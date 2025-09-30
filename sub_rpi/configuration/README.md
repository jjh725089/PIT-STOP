# Configuration

## Overview

This module defines global configuration constants and utility functions used throughout a vision-based detection system. It includes input preprocessing parameters, ONNX model thresholds, grid-based congestion settings, and rendering options. The configurations are intended to support modules such as fall detection, crowd density estimation, and congestion-aware pathfinding.

## Author

Jooho Hwang

## Installation & Dependencies

- OpenCV >= 4.6
- C++17 or later

## Key Components

### YOLO Input Settings

- `YOLO_INPUT_WIDTH`, `YOLO_INPUT_HEIGHT`, `INPUT_LONG_SIDE_TARGET`  
  Define the target input dimensions for YOLO-based object detection models.

### Model Configuration

- `FALL_MODEL_PATH`, `CROWD_MODEL_PATH`  
  Specify the ONNX model file paths.

- `FALL_CONF_THRESHOLD`, `CROWD_CONF_THRESHOLD`, `NMS_THRESHOLD`  
  Set confidence and NMS thresholds for inference.

### Grid & Congestion Parameters

- `GRID_CELL_SIZE`, `CONGESTION_DECAY_ALPHA`, `CONGESTION_INFLUENCE_RADIUS`  
  Used for discretizing the image space and modeling localized congestion levels.

### Pathfinding Parameters

- `PATH_ALPHA_HIGH`, `PATH_ALPHA_LOW`  
  Weights for different routing conditions.

- `PATH_EXIT_OUTER_WEIGHT`, `PATH_INNER_CONGESTION_THRESHOLD`  
  Influence pathfinding toward exits while considering internal congestion.

### Rendering

- `RENDERER_HEATMAP_ALPHA`, `RENDERER_HEATMAP_GAMMA`, `RENDERER_HEATMAP_BLUR`  
  Control heatmap blending, tone mapping, and smoothing.

## Utility Functions

- `toGrid(const cv::Point&)`  
  Converts pixel coordinates to grid coordinates.

- `toPixelCenter(const cv::Point&)`, `toPixelOrigin(const cv::Point&)`  
  Convert grid coordinates to pixel center or top-left origin positions.

- `letterbox(const cv::Mat&, float&, int&, int&)`  
  Resizes and pads an image to fit YOLO input requirements while preserving aspect ratio.

- `convertXywhToXyxy(float x, float y, float w, float h)`  
  Converts center-based YOLO coordinates to OpenCV rectangle format.

## Notes

- This file is intended to be included in any component that depends on standardized input/output scaling, detection thresholds, or spatial reasoning logic.
- See inline comments in the code for detailed descriptions of each parameter or function.