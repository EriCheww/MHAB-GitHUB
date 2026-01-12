#include <opencv2/opencv.hpp>
#include <iostream>
#include <chrono>
#include <vector>
#include <cmath>
#include <limits>

// PARAMETERS
// binary threshold, for black or white
constexpr int BIN_THRESH = 80;

// sun apparent radius bounds (pixels), can probably be changed to a single fixed size since when viewed from the earth it shouldn;t change much.
constexpr float R_MIN  = 170.0f;
constexpr float R_MAX  = 190.0f;
constexpr float R_STEP = 1.0f;

// alge
constexpr int MIN_ALG_PT_NEEDED = 20;

// curvature filtering
constexpr int CURVATURE_K = 5;
constexpr float CURVATURE_MIN_ANGLE = 0.25f; 
constexpr double MAX_RMS_RADIAL_ERROR_PX = 2.0;
constexpr int MIN_ARC_PTS = 6;
constexpr float MIN_ARC_COVERAGE_DEG = 30.0f;



// extract arc points from curved contour, and split by sign
void extractCurvedArcPointsBySign(
    const std::vector<cv::Point>& contour,
    std::vector<cv::Point2f>& arcPos,
    std::vector<cv::Point2f>& arcNeg
) {
    arcPos.clear();
    arcNeg.clear();

    int n = (int)contour.size();
    const float COS_THRESH = std::cos(CURVATURE_MIN_ANGLE);

    for (int i = CURVATURE_K; i < n - CURVATURE_K; ++i) {
        cv::Point2f p0 = contour[i - CURVATURE_K];
        cv::Point2f p1 = contour[i];
        cv::Point2f p2 = contour[i + CURVATURE_K];

        cv::Point2f v1 = p1 - p0;
        cv::Point2f v2 = p2 - p1;

        float mag = cv::norm(v1) * cv::norm(v2);

        if (mag < 1e-6f) continue;

        float cosAng = v1.dot(v2) / mag;
        cosAng = std::clamp(cosAng, -1.f, 1.f);

        // reject straight edges
        if (cosAng > COS_THRESH) continue;

        // curvature sign (2D cross product)
        float cross = v1.x * v2.y - v1.y * v2.x;

        if (cross > 0) {
            arcPos.push_back(p1);
        } 
        else{
            arcNeg.push_back(p1);
        }
    }
}



// check for min arc points 
bool hasMinimumArcPoints(
    const std::vector<cv::Point2f>& arcPts,
    int minPts
) {
    return (int)arcPts.size() >= minPts;
}



// compute angular arc coverage by arc points
float computeArcCoverageDegrees(
    const std::vector<cv::Point2f>& arcPts,
    const cv::Point2f& center
) {
    if (arcPts.size() < 2) return 0.0f;

    std::vector<float> angles;
    angles.reserve(arcPts.size());

    // convert arc points to angles
    for (const auto& p : arcPts) {
        float a = std::atan2(p.y - center.y, p.x - center.x);
        angles.push_back(a);
    }

    std::sort(angles.begin(), angles.end());

    // find largest gap between consecutive angles
    float maxGap = 0.0f;
    for (size_t i = 1; i < angles.size(); ++i) {
        maxGap = std::max(maxGap, angles[i] - angles[i - 1]);
    }

    // wrap-around gap (last to first)
    float wrapGap = (angles.front() + 2.0f * (float)CV_PI) - angles.back();
    maxGap = std::max(maxGap, wrapGap);

    // coverage = full circle minus largest empty gap
    float coverageRad = 2.0f * (float)CV_PI - maxGap;

    // convert to degrees
    return coverageRad * 180.0f / (float)CV_PI;
}



// algebraic fit, force fit all point in set to a circle
bool algebraicCircleFit(
    const std::vector<cv::Point2f>& pts,
    cv::Point2f& center,
    float& radius
) {
    int n = (int)pts.size();
    if (n < MIN_ALG_PT_NEEDED) return false;

    double sumX = 0, sumY = 0;
    double sumX2 = 0, sumY2 = 0;
    double sumXY = 0;
    double sumX3 = 0, sumY3 = 0;
    double sumX1Y2 = 0, sumX2Y1 = 0;

    for (const auto& p : pts) {
        double x = p.x;
        double y = p.y;
        double x2 = x * x;
        double y2 = y * y;

        sumX += x;
        sumY += y;
        sumX2 += x2;
        sumY2 += y2;
        sumXY += x * y;
        sumX3 += x2 * x;
        sumY3 += y2 * y;
        sumX1Y2 += x * y2;
        sumX2Y1 += x2 * y;
    }

    double C = n * sumX2 - sumX * sumX;
    double D = n * sumXY - sumX * sumY;
    double E = n * sumY2 - sumY * sumY;
    double G = 0.5 * (n * (sumX3 + sumX1Y2) - sumX * (sumX2 + sumY2));
    double H = 0.5 * (n * (sumY3 + sumX2Y1) - sumY * (sumX2 + sumY2));

    double denom = C * E - D * D;

    if (std::abs(denom) < 1e-8) return false;

    double cx = (G * E - D * H) / denom;
    double cy = (C * H - D * G) / denom;

    center = cv::Point2f((float)cx, (float)cy);

    double r = 0;
    for (const auto& p : pts) {
        double dx = p.x - cx;
        double dy = p.y - cy;
        r += std::sqrt(dx * dx + dy * dy);
    }

    radius = float(r / n);
    return true;
}

double computeRmsError(
    const std::vector<cv::Point2f>& pts,
    const cv::Point2f& center,
    float radius
) {
    double err = 0.0;
    for (const auto& p : pts) {
        double dx = p.x - center.x;
        double dy = p.y - center.y;
        double d = std::sqrt(dx * dx + dy * dy);
        double e = d - radius;
        err += e * e;
    }
    return std::sqrt(err / pts.size());
}



double geometricCirclFitFixedRadius(
    const std::vector<cv::Point2f>& pts,
    float R,
    cv::Point2f& center
) {
    double cx = 0, cy = 0;
    for (const auto& p : pts) {
        cx += p.x;
        cy += p.y;
    }
    cx /= pts.size();
    cy /= pts.size();

    for (int iter = 0; iter < 5; ++iter) {
        double gx = 0, gy = 0;
        double hx = 0, hy = 0;

        for (const auto& p : pts) {
            double dx = cx - p.x;
            double dy = cy - p.y;
            double d  = std::sqrt(dx * dx + dy * dy);
            if (d < 1e-6) continue;

            double e = d - R;
            gx += e * dx / d;
            gy += e * dy / d;
            hx += (dx * dx) / (d * d);
            hy += (dy * dy) / (d * d);
        }

        if (hx < 1e-6 || hy < 1e-6) break;

        cx -= gx / hx;
        cy -= gy / hy;
    }

    center = cv::Point2f((float)cx, (float)cy);

    double err = 0.0;
    for (const auto& p : pts) {
        double dx = p.x - cx;
        double dy = p.y - cy;
        double d  = std::sqrt(dx * dx + dy * dy);
        double e = d - R;
        err += e * e;
    }
    return std::sqrt(err / pts.size());
}



// fit range of determined circle radius to find best fit, for now its a range but is possible to be just a single set radius 
bool fitCircleRadiusRange(
    const std::vector<cv::Point2f>& arcPts,
    cv::Point2f& bestCenter,
    float& bestRadius
) {
    double bestErr = std::numeric_limits<double>::max();
    bool found = false;

    for (float R = R_MIN; R <= R_MAX; R += R_STEP) {
        cv::Point2f c;
        double err = geometricCirclFitFixedRadius(arcPts, R, c);

        if (err < bestErr) {
            bestErr = err;
            bestCenter = c;
            bestRadius = R;
            found = true;
        }
    }
    return found;
}



// greyscale -> binary(black or white) -> get contour -> get arc points -> select alge or geo mode -> output results.
int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: ./CB_detect <image>\n";
        return 1;
    }

    int mode;
    std::cout << "Select mode:\n";
    std::cout << "[1] Auto (numbers + final image)\n";
    std::cout << "[2] DEBUG Algebraic (forced)\n";
    std::cout << "[3] DEBUG Geometric (forced)\n";
    std::cout << "Choice: ";
    std::cin >> mode;

    cv::Mat img = cv::imread(argv[1], cv::IMREAD_GRAYSCALE);
    if (img.empty()) return 1;

    auto start = std::chrono::high_resolution_clock::now();

    cv::Mat bin;
    cv::threshold(img, bin, BIN_THRESH, 255, cv::THRESH_BINARY);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(bin, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    
    if (contours.empty()) return 1;

    size_t idx = 0;
    double maxArea = 0;
    for (size_t i = 0; i < contours.size(); ++i) {
        double a = cv::contourArea(contours[i]);
        if (a > maxArea) {
            maxArea = a;
            idx = i;
        }
    }

    // arc point containers (used for ALL modes + visualisation)
    std::vector<cv::Point2f> arcPos, arcNeg, arcPts;
    extractCurvedArcPointsBySign(contours[idx], arcPos, arcNeg);

    // choose dominant arc
    arcPts = (arcPos.size() > arcNeg.size()) ? arcPos : arcNeg;

    cv::Point2f center;
    float radius = 0.0f;
    bool usedAlgebraic = false;
    double rms_alg = -1.0; 
    double rms_geo = -1.0;

    // mode 2: debug forced non-arc algebraic, useful to see if arc point data set was the issue 
    if (mode == 2) {
        std::vector<cv::Point2f> contourPts;
        for (const auto& p : contours[idx]) {
            contourPts.emplace_back((float)p.x, (float)p.y);
        }

        usedAlgebraic = algebraicCircleFit(contourPts, center, radius);

        std::cout << "[DEBUG ALGEBRAIC]\n";
        if (usedAlgebraic) {
            std::cout << "RMS error: "
                      << computeRmsError(contourPts, center, radius) << "\n";
        } else {
            std::cout << "Fit failed\n";
        }
    }

    // mode 3: debug forced geometric, compare algebraic and geometric performance.
    else if (mode == 3) {

        std::cout << "[DEBUG GEOMETRIC]\n";
        std::cout << "arcPos: " << arcPos.size() << " arcNeg: " << arcNeg.size() << "\n";

        // pick dominant arc for debug
        const auto& arcPts = (arcPos.size() > arcNeg.size()) ? arcPos : arcNeg;

        fitCircleRadiusRange(arcPts, center, radius);

        rms_geo = computeRmsError(arcPts, center, radius);

        std::cout << "RMS (geometric): " << rms_geo << " px\n";
        std::cout << "Arc points used: " << arcPts.size() << "\n";
    }

    // mode 1: auto
    else {

        if (algebraicCircleFit(arcPts, center, radius)) {
            rms_alg = computeRmsError(arcPts, center, radius);
            if (radius >= R_MIN && radius <= R_MAX && rms_alg < MAX_RMS_RADIAL_ERROR_PX) {
                usedAlgebraic = true; 
            }  
        }

        if (!usedAlgebraic) {
            fitCircleRadiusRange(arcPts, center, radius);
            rms_geo = computeRmsError(arcPts, center, radius);
        }

        // validation

        // minimum arc count
        constexpr int MIN_ARC_PTS = 6;
        if (!hasMinimumArcPoints(arcPts, MIN_ARC_PTS)) {
            std::cout << "Rejected: insufficient arc points\n";
            return 0;  // or mark detection invalid
        }

        // arc angular coverage
        constexpr float MIN_ARC_COVERAGE_DEG = 30.0f;
        float arcCoverageDeg = computeArcCoverageDegrees(arcPts, center);

        if (arcCoverageDeg < MIN_ARC_COVERAGE_DEG) {
            std::cout << "Rejected: insufficient arc coverage ("
                    << arcCoverageDeg << " deg)\n";
            return 0;  // or mark detection invalid
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> t = end - start;

    std::cout << "Processing time: " << t.count() << " ms\n";
    std::cout << "Fit method: " << (usedAlgebraic ? "Algebraic" : "Geometric") << "\n";
    std::cout << "arcPos: " << arcPos.size() << "\n";
    std::cout << "arcNeg: " << arcNeg.size() << "\n";
    std::cout << "arcPts: " << arcPts.size() << "\n";
    std::cout << "RMS (" << (usedAlgebraic ? "Algebraic" : "Geometric") << "): " << (usedAlgebraic ? rms_alg : rms_geo) << " px\n";
    std::cout << "Center: (" << center.x << ", " << center.y << ")\n";
    std::cout << "Radius: " << radius << "\n";

    // visualisation 
    cv::Mat out;
    cv::cvtColor(bin, out, cv::COLOR_GRAY2BGR);

    cv::drawContours(out, contours, (int)idx, cv::Scalar(0,255,0), 2);

    for (const auto& p : arcPos) {
        cv::circle(out, p, 3, cv::Scalar(0,0,255), -1);   // red
    }

    for (const auto& p : arcNeg) {
        cv::circle(out, p, 3, cv::Scalar(0,255,255), -1); // yellow
    } 

    for (const auto& p : arcPts) {
        cv::circle(out, p, 5, cv::Scalar(255,0,255), -1); // magenta (selected arc)
    } 

    cv::circle(out, center, (int)radius, cv::Scalar(255,0,0), 1);   

    cv::drawMarker(out, center, cv::Scalar(255,0,0), cv::MARKER_CROSS, 500, 1);

    cv::imshow(mode == 1 ? "FINAL AUTO": (mode == 2 ? "DEBUG ALGEBRAIC" : "DEBUG GEOMETRIC"), out);

    cv::waitKey(0);
    cv::destroyAllWindows();
    return 0;
}
