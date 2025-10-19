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
    bool   assumeBrightOnDark = true;

    // Contour fallback
    double minCircularity = 0.70;    // 4πA/P²
    double minAreaFrac = 0.001;      // ignore tiny blobs (< 0.1% image area)
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

    // Normalize contrast a bit (helps on flat images)
    cv::Mat norm;
    cv::normalize(gray, norm, 0, 255, cv::NORM_MINMAX);

    // 2) Try HoughCircles first
    std::vector<cv::Vec3f> circles;
    const int minSide = std::min(norm.rows, norm.cols);
    const double minDist = std::max(1.0, p.minDistFrac * minSide);

    cv::HoughCircles(norm, circles, cv::HOUGH_GRADIENT, p.dp, minDist,
                     p.cannyHigh, p.accumulator, p.minRadius, p.maxRadius);

    if (!circles.empty()) {
        // pick the largest radius (often the target disk) — or just take circles[0]
        auto best = *std::max_element(circles.begin(), circles.end(),
                                      [](const cv::Vec3f& a, const cv::Vec3f& b){ return a[2] < b[2]; });
        center = cv::Point2f(best[0], best[1]);
        radius = best[2];
        return true;
    }

    // 3) Fallback: threshold -> contours -> circularity filter -> minEnclosingCircle
    cv::Mat thr;
    if (p.assumeBrightOnDark) {
        // Otsu on original; bright objects remain white
        cv::threshold(norm, thr, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
    } else {
        cv::threshold(norm, thr, 0, 255, cv::THRESH_BINARY_INV | cv::THRESH_OTSU);
    }

    // Clean specks
    cv::morphologyEx(thr, thr, cv::MORPH_OPEN, cv::getStructuringElement(cv::MORPH_ELLIPSE, {3,3}));

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(thr, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    const double minArea = p.minAreaFrac * norm.total();
    double bestScore = -1.0;
    std::vector<cv::Point> bestContour;

    for (auto& c : contours) {
        double area = cv::contourArea(c);
        if (area < minArea) continue;
        double perim = cv::arcLength(c, true);
        if (perim <= 0) continue;

        double circularity = 4.0 * CV_PI * area / (perim * perim); // 1.0 is a perfect circle
        if (circularity < p.minCircularity) continue;

        // prefer larger & more circular
        double score = circularity * area;
        if (score > bestScore) {
            bestScore = score;
            bestContour = std::move(c);
        }
    }

    if (!bestContour.empty()) {
        cv::minEnclosingCircle(bestContour, center, radius);
        return true;
    }

    return false; // nothing convincing found
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

