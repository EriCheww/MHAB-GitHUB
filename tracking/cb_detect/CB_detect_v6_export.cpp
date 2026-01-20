#define CB_DETECT_EXPORTS
#include "CB_detect_v6_export.h"

#include <vector>
#include <cmath>
#include <limits>
#include <random>
#include <algorithm>

namespace cb_detect {
namespace detail {

// ---------------- HELPERS ----------------

static void extractCurvedArcPointsBySign(
    const std::vector<cv::Point>& contour,
    const CircleDetectParams& params,
    std::vector<cv::Point2f>& arcPos,
    std::vector<cv::Point2f>& arcNeg
) {
    arcPos.clear();
    arcNeg.clear();

    const float COS_THRESH = std::cos(params.curvatureMinAngle);

    for (int i = params.curvatureK;
         i < (int)contour.size() - params.curvatureK;
         ++i) {

        cv::Point2f p0 = contour[i - params.curvatureK];
        cv::Point2f p1 = contour[i];
        cv::Point2f p2 = contour[i + params.curvatureK];

        cv::Point2f v1 = p1 - p0;
        cv::Point2f v2 = p2 - p1;

        float mag = cv::norm(v1) * cv::norm(v2);
        if (mag < 1e-6f) continue;

        float cosAng = std::clamp(v1.dot(v2) / mag, -1.f, 1.f);
        if (cosAng > COS_THRESH) continue;

        float cross = v1.x * v2.y - v1.y * v2.x;
        (cross > 0 ? arcPos : arcNeg).push_back(p1);
    }
}


static bool validateMinimumArcPoints(
    const std::vector<cv::Point2f>& arcPts,
    int minPts
) {
    return (int)arcPts.size() >= minPts;
}


static bool validateSpatialSpread(
    const std::vector<cv::Point2f>& pts,
    float minSpanPx
) {
    if (pts.size() < 2)
        return false;

    float minX = pts[0].x, maxX = pts[0].x;
    float minY = pts[0].y, maxY = pts[0].y;

    for (const auto& p : pts) {
        minX = std::min(minX, p.x);
        maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y);
        maxY = std::max(maxY, p.y);
    }

    float width  = maxX - minX;
    float height = maxY - minY;

    return (width >= minSpanPx) || (height >= minSpanPx);
}


static bool validateRadialConsistency(
    const std::vector<cv::Point2f>& pts,
    const cv::Point2f& center,
    float radius,
    float maxOutlierPercent
) {
    const size_t n = pts.size();
    if (n == 0) return false;

    const int maxBad = std::max(1, int(maxOutlierPercent * n));
    int badCount = 0;

    // Use RMS-derived adaptive threshold
    const float adaptiveErr = std::max(8.0f, 2.5f * radius * 1e-3f);
    const float errSq = adaptiveErr * adaptiveErr;

    for (const auto& p : pts) {
        float e = cv::norm(p - center) - radius;
        if (e * e > errSq) {
            if (++badCount > maxBad)
                return false;
        }
    }
    return true;
}


static double computeRmsError(
    const std::vector<cv::Point2f>& pts,
    const cv::Point2f& center,
    float radius
) {
    double err = 0.0;
    for (const auto& p : pts) {
        double d = cv::norm(p - center);
        double e = d - radius;
        err += e * e;
    }
    return std::sqrt(err / pts.size());
}


static bool algebraicCircleFit(
    const std::vector<cv::Point2f>& pts,
    const CircleDetectParams& params,
    cv::Point2f& center,
    float& radius
) {
    if ((int)pts.size() < params.algMinPts) return false;

    double sumX = 0, sumY = 0, sumX2 = 0, sumY2 = 0;
    double sumXY = 0, sumX3 = 0, sumY3 = 0;
    double sumX1Y2 = 0, sumX2Y1 = 0;

    for (const auto& p : pts) {
        double x = p.x, y = p.y;
        double x2 = x * x, y2 = y * y;

        sumX += x; sumY += y;
        sumX2 += x2; sumY2 += y2;
        sumXY += x * y;
        sumX3 += x2 * x;
        sumY3 += y2 * y;
        sumX1Y2 += x * y2;
        sumX2Y1 += x2 * y;
    }

    int n = (int)pts.size();
    double C = n * sumX2 - sumX * sumX;
    double D = n * sumXY - sumX * sumY;
    double E = n * sumY2 - sumY * sumY;
    double G = 0.5 * (n * (sumX3 + sumX1Y2) - sumX * (sumX2 + sumY2));
    double H = 0.5 * (n * (sumY3 + sumX2Y1) - sumY * (sumX2 + sumY2));

    double denom = C * E - D * D;
    if (std::abs(denom) < 1e-8) return false;

    center.x = float((G * E - D * H) / denom);
    center.y = float((C * H - D * G) / denom);

    double r = 0;
    for (const auto& p : pts)
        r += cv::norm(p - center);

    radius = float(r / n);
    return true;
}

static bool ransacCircleFit(
    const std::vector<cv::Point2f>& pts,
    const CircleDetectParams& params,
    cv::Point2f& bestCenter,
    float& bestRadius,
    float& bestRMS
) {
    if (pts.size() < 3) return false;

    std::mt19937 rng(params.ransacSeed);
    std::uniform_int_distribution<int> dist(0, (int)pts.size() - 1);

    int bestInliers = 0;
    bestRMS = std::numeric_limits<float>::infinity();

    for (int iter = 0; iter < params.ransacMaxIters; ++iter) {
        int i1 = dist(rng), i2 = dist(rng), i3 = dist(rng);
        if (i1 == i2 || i1 == i3 || i2 == i3) continue;

        auto p1 = pts[i1], p2 = pts[i2], p3 = pts[i3];

        float a = p2.x - p1.x, b = p2.y - p1.y;
        float c = p3.x - p1.x, d = p3.y - p1.y;
        float g = 2.0f * (a * (p3.y - p2.y) - b * (p3.x - p2.x));
        if (std::abs(g) < 1e-6f) continue;

        float cx = (d * (a*(p1.x+p2.x)+b*(p1.y+p2.y)) -
                    b * (c*(p1.x+p3.x)+d*(p1.y+p3.y))) / g;
        float cy = (a * (c*(p1.x+p3.x)+d*(p1.y+p3.y)) -
                    c * (a*(p1.x+p2.x)+b*(p1.y+p2.y))) / g;

        cv::Point2f center(cx, cy);
        float R = cv::norm(center - p1);
        if (R < params.rMin || R > params.rMax) continue;

        int inliers = 0;
        double err = 0.0;
        for (const auto& p : pts) {
            float e = std::abs(cv::norm(p - center) - R);
            if (e < params.ransacInlierThreshPx) {
                inliers++;
                err += e * e;
            }
        }

        if (inliers < params.ransacMinInliers) continue;

        float rms = std::sqrt(err / inliers);
        if (inliers > bestInliers || rms < bestRMS) {
            bestInliers = inliers;
            bestCenter = center;
            bestRadius = R;
            bestRMS = rms;

            if (bestRMS <= params.ransacEarlyExitRms &&
                float(bestInliers) / pts.size() >= params.ransacEarlyInlierRatio)
                break;
        }
    }

    return bestInliers >= params.ransacMinInliers &&
           bestRMS <= params.ransacMaxRms;
}

}

// ---------------- MAIN FUNCTION ----------------
bool detectCircle(
    const cv::Mat& image,
    CircleDetectResult& result,
    const CircleDetectParams& params,
    Mode mode
) {
    if (image.empty()) {
        result.rejectReason = CircleRejectReason::EmptyImage;
        return false;
    } 

    // CONTOUR EXTRACTION
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(image, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    
    if (contours.empty()) {
        result.rejectReason = CircleRejectReason::NoContours;
        return false;
    } 

    auto& contour = *std::max_element(
        contours.begin(), contours.end(),
        [](const auto& a, const auto& b) {
            return cv::contourArea(a) < cv::contourArea(b);
        }
    );

    // ARC POINT EXTRACTION
    std::vector<cv::Point2f> arcPos, arcNeg, arcPts;
    detail::extractCurvedArcPointsBySign(contour, params, arcPos, arcNeg);
    arcPts = (arcPos.size() > arcNeg.size()) ? arcPos : arcNeg;

    if (mode == Mode::Validate) {
        result.arcPos  = arcPos;
        result.arcNeg  = arcNeg;
        result.arcUsed = arcPts;
    }

    // ARC POINT VALIDATION
    if (!detail::validateMinimumArcPoints(arcPts, params.minArcPts)) {
        result.rejectReason = CircleRejectReason::InsufficientArcPoints;
        return false;
    }
    
    if (!detail::validateSpatialSpread(arcPts, params.minSpatialSpanPx)) {
        result.rejectReason = CircleRejectReason::InsufficientSpatialSpread;
        return false;
    }
    
    // CIRCLE FIT
    bool fitOk = false;
    result.usedAlgebraic = false;

    if (detail::algebraicCircleFit(arcPts, params, result.center, result.radius)) {
        result.rms = static_cast<float>(
            detail::computeRmsError(arcPts, result.center, result.radius)
        );

        if (result.rms < params.algMaxRms) {
            result.usedAlgebraic = true;
            fitOk = true;
        }
    }

    if (!fitOk) {
        fitOk = detail::ransacCircleFit(arcPts, params, result.center, result.radius, result.rms);
    }
    
    if (!fitOk) {
        result.rejectReason = CircleRejectReason::FitFailed;
        return false;
    }

    // FIT VALIDATION
    if (!detail::validateRadialConsistency(arcPts, result.center, result.radius, params.maxOutlierPercent)) {
        result.rejectReason = CircleRejectReason::RadialInconsistency;
        return false;
    }

    return true;
}
}