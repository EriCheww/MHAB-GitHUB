#pragma once
#include <opencv2/opencv.hpp>

namespace roi {

cv::Rect getROI(
    const cv::Mat& image,
    const cv::Point2f& center,
    int halfSize
);

}