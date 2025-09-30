# Congestion Analyzer

## Overview

This module analyzes congestion in a given image based on detected crowd positions. It divides the image into grid cells and calculates congestion intensity using an exponential decay function centered around each crowd point.

## Author

Jooho Hwang

## Project Structure

- `congestion_analyzer.h`: Header file defining the CongestionAnalyzer class interface.
- `congestion_analyzer.cpp`: Implementation of congestion analysis logic.

## Installation & Dependencies

- OpenCV >= 4.6
- C++17 or later

## Key Components

### CongestionAnalyzer class

- `Constructor`: Initializes the analyzer with the dimensions of the input image.
- `analyzeCongestionGrid()`: Generates a 2D float grid where each cell represents congestion weight based on proximity to crowd positions.
- `calculateAverageCongestionAround()`: Returns the average congestion score in a square area centered around the given pixel coordinate.

## Notes

- Congestion decays with distance using an exponential function: weight = exp(-alpha * distance).
- The module does not include visualization or I/O functionality; it focuses solely on congestion metric computation.