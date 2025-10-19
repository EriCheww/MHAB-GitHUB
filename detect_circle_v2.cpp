#include <opencv2/opencv.hpp>
#include <iostream>

struct FastCircleParams {
    int   maxDim = 320;         // downscale the longer side to this
    bool  brightOnDark = true;  // Sun/spot is brighter than background
    double minFracArea = 0.0005;// ignore blobs < 0.05% of downscaled image
    int   refineROI = 64;       // full-res ROI half-size (pixels) for subpixel refine; 0 = skip refine
};

struct FastCircleScratch {
    cv::Mat gray, small, bin, labels, stats, centroids;
    cv::Mat roiGray, roiBin;
};

/// Returns true if found. center/radius are in **original image pixels**.
/// Super fast: O(N) threshold + connectedComponents on downscaled image.
inline bool detectCircleFast(const cv::Mat& bgrOrGray,
                             cv::Point2f& center,
                             float& radius,
                             FastCircleScratch& S,
                             const FastCircleParams& P = {}) {
    CV_Assert(!bgrOrGray.empty());

    // 1) Grayscale (no alloc if already 1ch)
    if (bgrOrGray.channels()==1) S.gray = bgrOrGray;
    else cv::cvtColor(bgrOrGray, S.gray, cv::COLOR_BGR2GRAY);

    // 2) Downscale aggressively with area filter (preserves photometry)
    int w = S.gray.cols, h = S.gray.rows;
    double scale = 1.0;
    if (std::max(w,h) > P.maxDim) {
        scale = static_cast<double>(P.maxDim) / std::max(w,h);
        cv::resize(S.gray, S.small, cv::Size(), scale, scale, cv::INTER_AREA);
    } else {
        S.small = S.gray;
        scale = 1.0;
    }

    // 3) Light blur + Otsu threshold (choose polarity)
    cv::GaussianBlur(S.small, S.small, cv::Size(3,3), 0, 0, cv::BORDER_REPLICATE);
    int thrType = P.brightOnDark ? (cv::THRESH_BINARY|cv::THRESH_OTSU)
                                 : (cv::THRESH_BINARY_INV|cv::THRESH_OTSU);
    cv::threshold(S.small, S.bin, 0, 255, thrType);

    // 4) Connected components -> pick largest blob
    int n = cv::connectedComponentsWithStats(S.bin, S.labels, S.stats, S.centroids, 8, CV_32S);
    if (n <= 1) return false; // only background

    const int W = S.small.cols, H = S.small.rows;
    const double minArea = P.minFracArea * (W * 1.0 * H);
    int bestIdx = -1; int bestArea = -1;
    for (int i = 1; i < n; ++i) {
        int area = S.stats.at<int>(i, cv::CC_STAT_AREA);
        if (area < minArea) continue;
        if (area > bestArea) { bestArea = area; bestIdx = i; }
    }
    if (bestIdx < 0) return false;

    // 5) Centroid & radius estimate on downscaled image
    cv::Point2d cSmall(S.centroids.at<double>(bestIdx,0),
                       S.centroids.at<double>(bestIdx,1));
    double rSmall = std::sqrt(bestArea / CV_PI);

    // 6) Map to full-res
    cv::Point2f cFull(static_cast<float>(cSmall.x / scale),
                      static_cast<float>(cSmall.y / scale));
    float rFull = static_cast<float>(rSmall / scale);

    // 7) Optional refine on tiny ROI at full-res (subpixel)
    if (P.refineROI > 0) {
        int hs = P.refineROI;
        int x0 = std::clamp(static_cast<int>(std::round(cFull.x)) - hs, 0, w-1);
        int y0 = std::clamp(static_cast<int>(std::round(cFull.y)) - hs, 0, h-1);
        int x1 = std::min(x0 + 2*hs + 1, w);
        int y1 = std::min(y0 + 2*hs + 1, h);
        cv::Rect roi(x0, y0, x1-x0, y1-y0);

        S.roiGray = S.gray(roi);
        int thrTypeROI = P.brightOnDark ? (cv::THRESH_BINARY|cv::THRESH_OTSU)
                                        : (cv::THRESH_BINARY_INV|cv::THRESH_OTSU);
        cv::threshold(S.roiGray, S.roiBin, 0, 255, thrTypeROI);

        // Moments for centroid; radius via area again
        cv::Moments m = cv::moments(S.roiBin, true);
        if (m.m00 > 1.0) {
            cv::Point2f local(static_cast<float>(m.m10/m.m00), static_cast<float>(m.m01/m.m00));
            cFull.x = x0 + local.x;
            cFull.y = y0 + local.y;
            float area = static_cast<float>(m.m00);
            rFull = std::sqrt(area / CV_PI);
            // Small tweak: approximate radius bias from threshold shrink
            rFull *= 1.03f; // empirically ~3% helps on soft edges; adjust as needed
        }
    }

    center = cFull;
    radius = rFull;
    return true;
}


int main() {
    cv::setUseOptimized(true);            // enable OpenCV SIMD
    cv::setNumThreads(cv::getNumberOfCPUs());

    cv::Mat img = cv::imread("random_circle.png", cv::IMREAD_COLOR);
    if (img.empty()) { std::cerr << "no image\n"; return 1; }

    cv::Point2f c; float r;
    FastCircleParams P;
    P.maxDim = 320;        // speed/robustness sweet spot
    P.refineROI = 64;      // 0 for maximum speed
    FastCircleScratch S;

    auto start = std::chrono::high_resolution_clock::now();
    bool found = detectCircleFast(img, c, r, S, P);
    auto end = std::chrono::high_resolution_clock::now();

    double ms = std::chrono::duration<double, std::milli>(end - start).count();
    std::cout << "Detection time: " << ms << " ms\n";
    
    if (found) {
        cv::circle(img, c, (int)r, {0,255,0}, 2, cv::LINE_AA);
        cv::circle(img, c, 2, {0,0,255}, -1, cv::LINE_AA);
        std::cout << "Center: (" << c.x << ", " << c.y << "), r=" << r << "\n";
    } else {
        std::cout << "No circle found.\n";
    }

    cv::imshow("Detection", img);
    cv::waitKey(0);
    return 0;
}