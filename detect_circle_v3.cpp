#include <opencv2/opencv.hpp>
#include <iostream>

struct FixedSunParams {
    int   maxDim = 320;     // downscale longest side for speed
    int   spotDiam = 50;    // expected sun diameter (pixels in FULL-RES)
    float bgScale = 3.0f;   // background window ≈ bgScale * spotDiam
    int   refineROI = 32;   // optional full-res refine ROI half-size (0 = off)
};

bool detectFixedSunFast(const cv::Mat& bgrOrGray,
                        cv::Point2f& center,
                        float& radius,
                        const FixedSunParams& P = {}) {
    CV_Assert(!bgrOrGray.empty());

    // 1) Grayscale
    cv::Mat gray;
    if (bgrOrGray.channels() == 1) gray = bgrOrGray;
    else cv::cvtColor(bgrOrGray, gray, cv::COLOR_BGR2GRAY);

    // 2) Downscale for speed
    int W = gray.cols, H = gray.rows;
    double scale = 1.0;
    cv::Mat small;
    if (std::max(W,H) > P.maxDim) {
        scale = static_cast<double>(P.maxDim) / std::max(W,H);
        cv::resize(gray, small, cv::Size(), scale, scale, cv::INTER_AREA);
    } else {
        small = gray;
    }

    // Spot/background kernel sizes in DOWNSCALED pixels
    int spotK = std::max(3, int(std::round(P.spotDiam * scale)));
    if (spotK % 2 == 0) ++spotK;
    int bgK = std::max(spotK+2, int(std::round(P.bgScale * P.spotDiam * scale)));
    if (bgK % 2 == 0) ++bgK;

    // 3) Fast local mean filters (box) in float
    cv::Mat small32f, spotMean, bgMean, resp;
    small.convertTo(small32f, CV_32F);
    cv::blur(small32f, spotMean, cv::Size(spotK, spotK), cv::Point(-1,-1), cv::BORDER_REPLICATE);
    cv::blur(small32f, bgMean,   cv::Size(bgK,   bgK),   cv::Point(-1,-1), cv::BORDER_REPLICATE);

    // 4) Response = spot mean - background mean; find global max
    cv::subtract(spotMean, bgMean, resp);
    double maxVal; cv::Point maxLoc;
    cv::minMaxLoc(resp, nullptr, &maxVal, nullptr, &maxLoc);

    // 5) Map back to full-res; radius is fixed
    center = cv::Point2f(float(maxLoc.x / scale), float(maxLoc.y / scale));
    radius = P.spotDiam * 0.5f;

    // 6) Optional tiny refine at full-res (centroid in a small ROI)
    if (P.refineROI > 0) {
        int x0 = std::clamp(int(std::round(center.x)) - P.refineROI, 0, W-1);
        int y0 = std::clamp(int(std::round(center.y)) - P.refineROI, 0, H-1);
        int x1 = std::min(x0 + 2*P.refineROI + 1, W);
        int y1 = std::min(y0 + 2*P.refineROI + 1, H);
        cv::Rect roi(x0, y0, x1-x0, y1-y0);

        cv::Mat roiG = gray(roi);
        // simple local threshold (Otsu) to pull the bright spot
        cv::Mat roiBin;
        cv::threshold(roiG, roiBin, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

        cv::Moments m = cv::moments(roiBin, true);
        if (m.m00 > 1.0) {
            center.x = x0 + float(m.m10 / m.m00);
            center.y = y0 + float(m.m01 / m.m00);
        }
    }
    return true;
}

int main() {
    cv::setUseOptimized(true);
    cv::setNumThreads(cv::getNumberOfCPUs());

    cv::Mat img = cv::imread("random_circle.png", cv::IMREAD_COLOR);
    if (img.empty()) { std::cerr << "no image\n"; return 1; }

    FixedSunParams P;
    P.spotDiam = 102;   // <-- set to your known spot size in pixels (full-res)
    P.bgScale  = 3.0f;  // 2.5–4 works well; larger smooths more background
    P.maxDim   = 320;   // downscale for speed
    P.refineROI = 32;   // set 0 to squeeze even more speed

    cv::Point2f c; float r;
    auto t0 = std::chrono::high_resolution_clock::now();
    bool ok = detectFixedSunFast(img, c, r, P);
    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    std::cout << "Detection time: " << ms << " ms\n";
    if (ok) {
        std::cout << "Center: (" << c.x << ", " << c.y << "), r=" << r << "\n";
        cv::circle(img, c, (int)r, {0,255,0}, 2, cv::LINE_AA);
        cv::circle(img, c, 2, {0,0,255}, -1, cv::LINE_AA);
    }
    cv::imshow("Detection", img);
    cv::waitKey(0);
    return 0;
}
