# Model Export Guideline

## Overview

This repository provides export scripts for two deep learning models from PyTorch format to ONNX format.

## Author

Jooho Hwang

## Project Structure

- `fall_model_export.ipynb`: TorchScript export for the 3D CNN-based fall detection model
- `crowd_model_export.ipynb`: TorchScript export for the CSRNet-based crowd detection model
- `crowd.pt`: PyTorch-formatted model for fall detection
- `fall.pt`: PyTorch-formatted model for crowd detection

## Installation & Dependencies

- Google Colaboratory
- Jupyter Notebook

## Notes

- Download and place fall.pt and crowd.pt in the correct directory.
- After opening and running all cells in the notebooks, fall.onnx and crowd.onnx will be saved in the current working directory.