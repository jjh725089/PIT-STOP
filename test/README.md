# Test

## Overview

These test codes validate a multi-module vision system by executing either log-based consistency test or real-time visualization test. The system integrates the following key components:

- **Fall Detection**: Identifies fallen individuals.
- **Crowd Detection**: Detects and localizes people in an image.
- **Congestion Analysis**: Generates heatmaps representing crowd density.
- **Pathfinding**: Calculates optimal escape routes based on crowd congestion and exit positions.

Two test applications are provided:

- `test_log`: Verifies the consistency of module outputs over repeated runs.
- `test_visual`: Visualizes all detection and analysis results in a fullscreen OpenCV window.

## Author

Jooho Hwang

## Project Structure

- `test_log.cpp`: Consistency testing and timing for all modules
- `test_visual.cpp`: Visualization of fall/crowd detection and pathfinding
- `Makefile`: Build configuration for compiling test binaries

## Installation & Dependencies

- OpenCV >= 4.6
- C++17 or later
- ONNX Runtime >= 1.17.0

## Testing

- `test_log.cpp`

Executes runs of fall detection, crowd detection, congestion analysis, and pathfinding.
Measures execution time per module.
Compares each run to the baseline to verify consistency using fuzzy matching.

- `test_visual.cpp`

Displays results in a fullscreen OpenCV window:

    - Top-left: Fall detection bounding boxes
    - Top-right: Pathfinding results
    - Bottom-left: Crowd detection dots
    - Bottom-right: Congestion heatmap

## Notes

- Input files must be located in the current working directory.
- Some randomness (e.g., start location) is introduced to simulate varied test conditions.