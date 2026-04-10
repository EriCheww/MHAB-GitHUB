#pragma once

#include <opencv2/opencv.hpp>
#include <vector>
#include <string>

namespace mosaic {

    // Load images from file paths
    bool loadImages(const std::vector<std::string>& paths,
                    std::vector<cv::Mat>& images);

    // Stitch images into one result
    bool stitchImages(const std::vector<cv::Mat>& images,
                      cv::Mat& result);

    // Convenience function: load + stitch
    bool createMosaic(const std::vector<std::string>& paths,
                      cv::Mat& result);

}