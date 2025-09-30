#ifndef FALL_INFO_H
#define FALL_INFO_H

// OpenCV
#include <opencv2/core.hpp>

/**
 * @brief Structure representing a single fall detection result
 */
struct FallInfo {
	cv::Rect bbox;          ///< Bounding box in image coordinates
	float yolo_conf = 0.0f; ///< YOLO model confidence
	int class_id = -1;      ///< Class ID (usually person)
	int pred = -1;          ///< Prediction (0 = FALL, 1 = SAFE)
	float svm_conf = 0.0f;  ///< Confidence score of simple fall classifier
};

#endif  // FALL_INFO_H