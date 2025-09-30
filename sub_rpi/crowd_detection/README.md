# Crowd Detector

## Overview

This module performs crowd detection using a YOLO-based model executed through ONNX Runtime. The detection pipeline includes preprocessing (resizing, padding, normalization), ONNX inference execution, and postprocessing (NMS, bounding box adjustment).

## Author

Jooho Hwang

## Project Structure

- `crowd_info.h`: Data structure representing a single crowd detection result.
- `crowd_detector.h`: Header file defining the CrowdDetector class interface.
- `crowd_detector.cpp`: Implementation of model loading, inference, and post-processing.

## Installation & Dependencies

- OpenCV >= 4.6
- C++17 or later
- ONNX Runtime >= 1.17.0

## Key Components

### CrowdDetector class

- `Constructor`: Initializes a shared ONNX session using the provided model path.
- `detect()`: Takes a BGR image and returns a list of CrowdInfo results.
- `getCrowdCenters()`: Extracts the center coordinates of crwod detections.
- `runWarmUp()`: Performs dummy inference for initialization.
- `preprocessYoloInput()`: Prepares image input by resizing and normalizing to match model input.
- `runYoloInference()`: Executes ONNX inference and returns output tensor.
- `postprocessYoloOutput()`: Filters valid detections, applies NMS, and adjusts bounding boxes.
- `applyNms()`: Applies OpenCV’s NMSBoxes to reduce overlapping detections.
- `computeIoU()`: Calculates intersection of union

### CrowdInfo structure

- `cv::Rect bbox`: Bounding box coordinates.
- `float conf`: Model confidence score.

## Notes

- ONNX session is cached by model path and reused to optimize performance.
- detect() performs all steps in a single method call — from preprocessing to postprocessing.
- Warm-up is automatically triggered on construction to ensure the session is ready for inference.