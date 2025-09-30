# Fall Detector

## Overview

This module performs fall detection using a YOLO-based model executed through ONNX Runtime. The detection pipeline includes preprocessing (resizing, padding, normalization), ONNX inference execution, postprocessing (NMS, bounding box adjustment), and fall classification. A simple aspect-ratio-based rule is used to distinguish fall from safe postures.

## Author

Jooho Hwang

## Project Structure

- `fall_info.h`: Data structure representing a single fall detection result.
- `fall_detector.h`: Header file defining the FallDetector class interface.
- `fall_detector.cpp`: Implementation of model loading, inference, and post-processing.

## Installation & Dependencies

- OpenCV >= 4.6
- C++17 or later
- ONNX Runtime >= 1.17.0

## Key Components

### FallDetector class

- `Constructor`: Initializes a shared ONNX session using the provided model path.
- `detect()`: Takes a BGR image and returns a list of FallInfo results.
- `getFallCenters()`: Extracts the center coordinates of fall detections.
- `runWarmUp()`: Performs dummy inference for initialization.
- `preprocessYoloInput()`: Prepares image input by resizing and normalizing to match model input.
- `runYoloInference()`: Executes ONNX inference and returns output tensor.
- `postprocessYoloOutput()`: Filters valid detections, applies NMS, and adjusts bounding boxes.
- `applyNms()`: Applies OpenCV’s NMSBoxes to reduce overlapping detections.

### FallInfo structure

- `cv::Rect bbox`: Bounding box coordinates.
- `float yolo_conf`: Model confidence score.
- `int class_id`: Predicted class ID (e.g., person).
- `int pred`: Fall prediction (0 = FALL, 1 = SAFE).
- `float svm_conf`: Heuristic confidence based on aspect ratio.

## Notes

- Fall detection is based on a fixed aspect ratio threshold (AR > 1.125 → FALL).
- ONNX session is cached by model path and reused to optimize performance.
- detect() performs all steps in a single method call — from preprocessing to fall classification.
- Warm-up is automatically triggered on construction to ensure the session is ready for inference.