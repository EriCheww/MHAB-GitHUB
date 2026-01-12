#include <opencv2/opencv.hpp>
#include <iostream>

#include "cb_detect/CB_detect_v5_export.h"


int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "Usage: detect_test <image_path>\n";
        return 1;
    }

    // get cb_detect structures ready
    cb_detect::CircleDetectResult res;
    cb_detect::CircleDetectParams params;

    // Load image COLOR       speed: 25 - 40ms 
    // cv::Scalar hsvLower = {5, 30, 120};
    // cv::Scalar hsvUpper = {30, 255, 255};

    // cv::Mat image = cv::imread(argv[1], cv::IMREAD_COLOR);
    // if (image.empty()) return 1;  

    // auto start = std::chrono::high_resolution_clock::now();

    // cv::Mat hsv;
    // cv::cvtColor(image, hsv, cv::COLOR_BGR2HSV);
    // cv::Mat mask;
    // cv::inRange(hsv, hsvLower, hsvUpper, mask);

    // Load image Grey Scale    speed: 10 to 20 ms
    int binaryThreshold = 80;
    cv::Mat image = cv::imread(argv[1], cv::IMREAD_GRAYSCALE);
    if (image.empty()) {
        std::cerr << "Failed to load image: " << argv[1] << "\n";
        return 1;
    }

    auto start = std::chrono::high_resolution_clock::now();

    cv::Mat mask;
    cv::threshold(image, mask, binaryThreshold, 255, cv::THRESH_BINARY);

    bool found = cb_detect::detectCircle(mask, res, params);

    auto end = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(end - start).count();


    if (!found) {
        std::cout << "No circle detected.\n";
        cv::imshow("Result", image);
        cv::waitKey(0);
        return 0;
    }

    // Draw result for binary image mask
    cv::Mat out;
    cv::cvtColor(image, out, cv::COLOR_GRAY2BGR);

    // Draw result for color image mask
    // cv::Mat out = image.clone();

    // Draw circle (green)
    cv::circle(out, res.center, static_cast<int>(res.radius), cv::Scalar(255, 0, 255), 2);

    // Draw center cross (red)
    cv::drawMarker(out, res.center, cv::Scalar(255, 0, 255), cv::MARKER_CROSS, 300, 2);


    // Print info
    std::cout << "Circle detected\n";
    std::cout << "Center: (" << res.center.x << ", " << res.center.y << ")\n";
    std::cout << "Radius: " << res.radius << "\n";
    std::cout << "Detection time: " << ms << " ms\n";
    std::cout << "RMS: " << res.rms << "\n";
    std::cout << "Method: " << (res.usedAlgebraic ? "Algebraic" : "RANSAC") << "\n";
   

    // Show result
    cv::imshow("Detected Circle", out);
    cv::waitKey(0);
    cv::destroyAllWindows();

    return 0;
}
