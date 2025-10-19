#include <opencv2/opencv.hpp>
#include <chrono>
#include <iostream>

struct CircleDetectionParams {
    // Hough params
    double dp = 1.2;                 // inverse accumulator resolution
    double minDistFrac = 0.25;       // minDist as fraction of min(img.rows, img.cols)
    double cannyHigh = 120;          // param1 (Canny high threshold)
    double accumulator = 25;         // param2 (center vote threshold)
    int    minRadius = 0;            // pixels (0 = let Hough decide)
    int    maxRadius = 0;            // 0 = no upper bound

    // Preprocessing
    int    blurKS = 5;               // Gaussian kernel size (odd)
    double blurSigma = 1.2;
};

/// Returns true if a circle-like object is found; outputs center & radius (pixels)
bool detectCircleLike(const cv::Mat& inputBGRorGray,
                      cv::Point2f& center,
                      float& radius,
                      const CircleDetectionParams& p = {}) {
    CV_Assert(!inputBGRorGray.empty());

    // 1) Grayscale + denoise
    cv::Mat gray;
    if (inputBGRorGray.channels() == 1) gray = inputBGRorGray.clone();
    else cv::cvtColor(inputBGRorGray, gray, cv::COLOR_BGR2GRAY);

    if (p.blurKS >= 3 && (p.blurKS % 2 == 1))
        cv::GaussianBlur(gray, gray, cv::Size(p.blurKS, p.blurKS), p.blurSigma, p.blurSigma);

    // Normalize contrast
    cv::Mat norm;
    cv::normalize(gray, norm, 0, 255, cv::NORM_MINMAX);

    // 2) HoughCircles only
    std::vector<cv::Vec3f> circles;
    const int minSide = std::min(norm.rows, norm.cols);
    const double minDist = std::max(1.0, p.minDistFrac * minSide);

    cv::HoughCircles(norm, circles, cv::HOUGH_GRADIENT, p.dp, minDist,
                     p.cannyHigh, p.accumulator, p.minRadius, p.maxRadius);

    if (!circles.empty()) {
        auto best = *std::max_element(circles.begin(), circles.end(),
                                      [](const cv::Vec3f& a, const cv::Vec3f& b){ return a[2] < b[2]; });
        center = cv::Point2f(best[0], best[1]);
        radius = best[2];
        return true;
    }

    return false;
}

int main() {
    cv::Mat img = cv::imread("random_circle.png");
    if (img.empty()) {
        std::cerr << "Could not read image\n";
        return 1;
    }

    cv::Point2f c; float r;
    CircleDetectionParams params;

    auto start = std::chrono::high_resolution_clock::now();
    bool found = detectCircleLike(img, c, r, params);
    auto end = std::chrono::high_resolution_clock::now();

    double ms = std::chrono::duration<double, std::milli>(end - start).count();
    std::cout << "Detection time: " << ms << " ms\n";

    if (found) {
        cv::circle(img, c, (int)r, {0,255,0}, 2);
        cv::circle(img, c, 2, {0,0,255}, -1);
        std::cout << "Center: (" << c.x << ", " << c.y << "), r=" << r << "\n";
    } else {
        std::cout << "No circle found.\n";
    }

    cv::imshow("Detection", img);
    cv::waitKey(0);
    return 0;
}
