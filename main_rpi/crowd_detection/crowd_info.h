#ifndef CROWD_INFO_H
#define CROWD_INFO_H

// OpenCV
#include <opencv2/core.hpp>

/**
 * @brief Represents information of a detected crowd instance.
 */
struct CrowdInfo {
	cv::Rect bbox;     ///< Bounding box of the detected object
	float conf;  ///< YOLO detection confidence score
};

#endif  // CROWD_INFO_H