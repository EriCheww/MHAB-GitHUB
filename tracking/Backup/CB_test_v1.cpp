#include <opencv2/opencv.hpp>
#include <iostream>
#include <chrono>
#include <vector>
#include <cmath>
#include <limits>

// PARAMETERS
// sun apparent radius bounds (pixels), can probably be changed to a single fixed size since when viewed from the earth it shouldn;t change much.
constexpr float R_MIN  = 140.0f;
constexpr float R_MAX  = 180.0f;
constexpr float R_STEP = 1.0f;

// curvature filtering
constexpr int   CURVATURE_K = 6;
constexpr float CURVATURE_MIN_ANGLE = 0.20f; 

// binary threshold, for black or white
constexpr int BIN_THRESH = 80;



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

        // Reject straight edges
        if (cosAng > COS_THRESH) continue;

        // Curvature sign (2D cross product)
        float cross = v1.x * v2.y - v1.y * v2.x;

        if (cross > 0)
            arcPos.push_back(p1);
        else
            arcNeg.push_back(p1);
    }
}



// algebraic fit, force fit all point in set to a circle
bool algebraicCircleFit(
    const std::vector<cv::Point2f>& pts,
    cv::Point2f& center,
    float& radius
) {
    int n = (int)pts.size();
    if (n < 20) return false;

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
    if (std::abs(denom) < 1e-8)
        return false;

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



// geometric fit
double solveCenterFixedRadius(
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

        if (hx < 1e-6 || hy < 1e-6)
            break;

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
        double err = solveCenterFixedRadius(arcPts, R, c);

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

    // choose dominant arc (or merge if full circle)
    constexpr int MIN_ARC_PTS = 12;

    auto isGoodSunArc = [&](const std::vector<cv::Point2f>& pts) -> bool {
        if ((int)pts.size() < MIN_ARC_PTS) return false;

        cv::Point2f c;
        float r;
        if (!algebraicCircleFit(pts, c, r)) return false;
        if (r < R_MIN || r > R_MAX) return false;

        double rms = computeRmsError(pts, c, r);
        return rms < 2.0;
    };

    bool posOk = isGoodSunArc(arcPos);
    bool negOk = isGoodSunArc(arcNeg);

    if (posOk && !negOk)
        arcPts = arcPos;
    else if (negOk && !posOk)
        arcPts = arcNeg;
    else if (posOk && negOk) {
        // both look sun-like (true full circle) → merge
        arcPts = arcPos;
        arcPts.insert(arcPts.end(), arcNeg.begin(), arcNeg.end());
    } else {
        // neither passes quality → fall back to dominant (best effort)
        arcPts = (arcPos.size() > arcNeg.size()) ? arcPos : arcNeg;
    }




    cv::Point2f center;
    float radius = 0.0f;
    bool usedAlgebraic = false;

    // mode 2: debug forced non-arc algebraic, useful to see if arc point data set was the issue 
    if (mode == 2) {
        std::vector<cv::Point2f> contourPts;
        for (const auto& p : contours[idx])
            contourPts.emplace_back((float)p.x, (float)p.y);

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
        // std::vector<cv::Point2f> arcPos, arcNeg;
        // extractCurvedArcPointsBySign(contours[idx], arcPos, arcNeg);

        std::cout << "[DEBUG GEOMETRIC]\n";
        std::cout << "arcPos: " << arcPos.size()
                << " arcNeg: " << arcNeg.size() << "\n";

        // pick dominant arc for debug
        const auto& arcPts =
            (arcPos.size() > arcNeg.size()) ? arcPos : arcNeg;

        fitCircleRadiusRange(arcPts, center, radius);

        std::cout << "[DEBUG GEOMETRIC]\n";
        std::cout << "Arc points used: " << arcPts.size() << "\n";
    }

    // mode 1: auto
    else {
        // std::vector<cv::Point2f> arcPos, arcNeg;
        // extractCurvedArcPointsBySign(contours[idx], arcPos, arcNeg);

        // choose dominant curvature sign (sun arc)
        // const auto& arcPts =
        //     (arcPos.size() > arcNeg.size()) ? arcPos : arcNeg;

        if (algebraicCircleFit(arcPts, center, radius)) {
            double rms = computeRmsError(arcPts, center, radius);
            if (radius >= R_MIN && radius <= R_MAX && rms < 2.0)
                usedAlgebraic = true;
        }

        if (!usedAlgebraic)
            fitCircleRadiusRange(arcPts, center, radius);
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> t = end - start;

    std::cout << "Processing time: " << t.count() << " ms\n";
    std::cout << "Fit method: "
              << (usedAlgebraic ? "Algebraic" : "Geometric") << "\n";
    std::cout << "Center: (" << center.x << ", " << center.y << ")\n";
    std::cout << "Radius: " << radius << "\n";

    // visualisation 
    cv::Mat out;
    cv::cvtColor(bin, out, cv::COLOR_GRAY2BGR);

    cv::drawContours(out, contours, (int)idx,
                     cv::Scalar(0,255,0), 2);

    // std::vector<cv::Point2f> arcPts;
    // std::vector<cv::Point2f> arcPos, arcNeg;
    // extractCurvedArcPointsBySign(contours[idx], arcPos, arcNeg);

    for (const auto& p : arcPos)
        cv::circle(out, p, 2, cv::Scalar(0,0,255), -1);   // red

    for (const auto& p : arcNeg)
        cv::circle(out, p, 2, cv::Scalar(0,255,255), -1); // yellow

    for (const auto& p : arcPts)
        cv::circle(out, p, 2, cv::Scalar(255,0,255), -1); // magenta (selected arc)



    cv::circle(out, center, (int)radius,
               cv::Scalar(255,0,0), 1);   

    cv::drawMarker(out, center, cv::Scalar(255,0,0),
                   cv::MARKER_CROSS, 500, 1);

    cv::imshow(
        mode == 1 ? "FINAL AUTO"
                  : (mode == 2 ? "DEBUG ALGEBRAIC" : "DEBUG GEOMETRIC"),
        out
    );
    cv::waitKey(0);

    return 0;
}
