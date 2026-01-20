#pragma once
#include <opencv2/opencv.hpp>

namespace cb_detect {

enum class Mode {
    Detect,    
    Validate 
};

enum class CircleRejectReason {
    None,
    EmptyImage,
    NoContours,
    InsufficientArcPoints,
    InsufficientSpatialSpread,
    FitFailed,
    RadialInconsistency
};

struct CircleDetectParams {
    float rMin = 100.0f;
    float rMax = 300.0f;

    int curvatureK = 5;
    float curvatureMinAngle = 0.25f;

    int minArcPts = 6;
    float minSpatialSpanPx = 12.0f;
    float maxOutlierPercent = 0.5f;

    int algMinPts = 20;
    float algMaxRms = 2.0f;

    int ransacMaxIters = 300;
    float ransacInlierThreshPx = 2.0f;
    int ransacMinInliers = 10;
    float ransacMaxRms = 2.0f;
    unsigned int ransacSeed = 42;
    float ransacEarlyExitRms = 2.0f;
    float ransacEarlyInlierRatio = 0.5f;
};

struct CircleDetectResult {
    cv::Point2f center{};
    float radius = 0.0f;
    float rms = 0.0f;
    bool usedAlgebraic = false;

    CircleRejectReason rejectReason = CircleRejectReason::None;

    std::vector<cv::Point2f> arcPos;
    std::vector<cv::Point2f> arcNeg;
    std::vector<cv::Point2f> arcUsed;
};

bool detectCircle(
    const cv::Mat& imageBGR,
    CircleDetectResult& result,
    const CircleDetectParams& params = CircleDetectParams(),
    Mode mode = Mode::Detect
);

}
