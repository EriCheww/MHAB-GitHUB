#include <opencv2/opencv.hpp>
#include <iostream>
#include <chrono>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: ./sun_detect <image_path>\n";
        return 1;
    }

    int mode;
    std::cout << "Select mode:\n";
    std::cout << "[1] Detect and exit (no display, for timing)\n";
    std::cout << "[2] Detect and display result\n";
    std::cout << "Choice: ";
    std::cin >> mode;

    if (mode != 1 && mode != 2) {
        std::cerr << "Invalid mode selected\n";
        return 1;
    }

    // load image
    cv::Mat img = cv::imread(argv[1], cv::IMREAD_GRAYSCALE);
    if (img.empty()) {
        std::cerr << "Failed to load image\n";
        return 1;
    }

    // start performance timer
    auto start = std::chrono::high_resolution_clock::now();

    // find contours
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(img, contours, cv::RETR_EXTERNAL,
                    cv::CHAIN_APPROX_SIMPLE);

    // find largest contour
    size_t idx = 0;
    double maxArea = 0.0;
    for (size_t i = 0; i < contours.size(); ++i) {
        double a = cv::contourArea(contours[i]);
        if (a > maxArea) {
            maxArea = a;
            idx = i;
        }
    }

    // centroid via moments
    cv::Moments m = cv::moments(contours[idx]);
    cv::Point2f center(m.m10 / m.m00, m.m01 / m.m00);

    // end performance timer
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> processingTime = end - start;

    std::cout << "Processing time: " << processingTime.count() << " ms\n";
    std::cout << "Sun center: (" << center.x << ", " << center.y << ")\n";

    // mode 1: display summary
    if (mode == 1) {
        return 0;
    }

    // mode 2: display result img
    cv::Mat output;
    cv::cvtColor(img, output, cv::COLOR_GRAY2BGR);

    cv::drawMarker(output, center, cv::Scalar(0, 255, 0), cv::MARKER_CROSS, 1000, 2);
    cv::circle(output, center, 5, cv::Scalar(255, 0, 0), -1);

    cv::imshow("Sun Detection (CBD)", output);
    cv::waitKey(0);

    return 0;
}
