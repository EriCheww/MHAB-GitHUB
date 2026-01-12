#include <opencv2/opencv.hpp>
#include <iostream>
#include <chrono>
#include <vector>
#include <cmath>
#include <limits>
#include <random>

// PARAMETERS
// binary threshold, for black or white
constexpr int BIN_THRESH = 80;

// sun apparent radius bounds (pixels), can probably be changed to a single fixed size since when viewed from the earth it shouldn;t change much.
constexpr float R_MIN  = 100.0f;
constexpr float R_MAX  = 300.0f;
constexpr float R_STEP = 1.0f;

// alge
constexpr int MIN_ALG_PT_NEEDED = 20;

// curvature filtering
constexpr int CURVATURE_K = 5;
constexpr float CURVATURE_MIN_ANGLE = 0.25f; 
constexpr double MAX_RMS_RADIAL_ERROR_PX = 2.0;
constexpr int MIN_ARC_PTS = 6;
constexpr float MIN_ARC_COVERAGE_DEG = 30.0f;

// ================= RANSAC CIRCLE FIT PARAMETERS =================

// Maximum RANSAC iterations
constexpr int RANSAC_MAX_ITERS = 300;

// Inlier distance threshold (pixels)
constexpr float RANSAC_INLIER_THRESH_PX = 2.0f;

// Minimum number of inliers to accept a circle
constexpr int RANSAC_MIN_INLIERS = 10;

// Maximum allowed RMS radial error after RANSAC
constexpr float RANSAC_MAX_RMS_ERROR_PX = 2.0f;

// Random seed for reproducibility
constexpr unsigned int RANSAC_SEED = 42;

constexpr float RANSAC_EARLY_EXIT_RMS = 10.0f;   // px (start with 0.5–1.0)
constexpr float RANSAC_EARLY_INLIER_RATIO = 0.5f; // 70% of points


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



bool ransacCircleFit(
    const std::vector<cv::Point2f>& pts,
    cv::Point2f& bestCenter,
    float& bestRadius,
    float& bestRMS
) {
    if (pts.size() < 3) return false;

    std::mt19937 rng(RANSAC_SEED);
    std::uniform_int_distribution<int> dist(0, (int)pts.size() - 1);

    int rejectedRadius = 0;
    int rejectedInliers = 0;
    int validHypotheses = 0;


    int bestInliers = 0;
    bestRMS = std::numeric_limits<float>::infinity();

    for (int iter = 0; iter < RANSAC_MAX_ITERS; ++iter) {
        int i1 = dist(rng);
        int i2 = dist(rng);
        int i3 = dist(rng);

        if (i1 == i2 || i1 == i3 || i2 == i3)
            continue;

        const auto& p1 = pts[i1];
        const auto& p2 = pts[i2];
        const auto& p3 = pts[i3];


        // ---- compute circle from 3 points ----
        float a = p2.x - p1.x;
        float b = p2.y - p1.y;
        float c = p3.x - p1.x;
        float d = p3.y - p1.y;

        float e = a * (p1.x + p2.x) + b * (p1.y + p2.y);
        float f = c * (p1.x + p3.x) + d * (p1.y + p3.y);
        float g = 2.0f * (a * (p3.y - p2.y) - b * (p3.x - p2.x));

        if (std::abs(g) < 1e-6f) continue; // collinear

        float cx = (d * e - b * f) / g;
        float cy = (a * f - c * e) / g;
        cv::Point2f center(cx, cy);

        float R = cv::norm(center - p1);
        if (R < R_MIN || R > R_MAX) {
            rejectedRadius++;
            continue;
        }

        // ---- count inliers ----
        int inliers = 0;
        double errSum = 0.0;

        for (const auto& p : pts) {
            float d = cv::norm(p - center);
            float err = std::abs(d - R);
            if (err < RANSAC_INLIER_THRESH_PX) {
                inliers++;
                errSum += err * err;
            }
        }

        if (inliers < RANSAC_MIN_INLIERS) {
            rejectedInliers++;
            continue;
        }


        float rms = std::sqrt(errSum / inliers);

        if (inliers > bestInliers ||
            (inliers == bestInliers && rms < bestRMS)) {

            bestInliers = inliers;
            bestCenter = center;
            bestRadius = R;
            bestRMS = rms;
            validHypotheses++;

            // ---------- EARLY EXIT ----------
            float inlierRatio = float(bestInliers) / float(pts.size());

            if (bestRMS <= RANSAC_EARLY_EXIT_RMS &&
                inlierRatio >= RANSAC_EARLY_INLIER_RATIO) {

                std::cout << "[RANSAC] Early exit at iter "
                        << iter
                        << " | RMS=" << bestRMS
                        << " | inliers=" << bestInliers
                        << "\n";
                break;
            }
        }
    }

    std::cout << "\n--- RANSAC INTERNAL DEBUG ---\n";
    std::cout << "Total iterations: " << RANSAC_MAX_ITERS << "\n";
    std::cout << "Valid hypotheses: " << validHypotheses << "\n";
    std::cout << "Rejected by radius: " << rejectedRadius << "\n";
    std::cout << "Rejected by inliers: " << rejectedInliers << "\n";
    std::cout << "Best inliers found: " << bestInliers << "\n";
    std::cout << "Best RMS found: " << bestRMS << "\n";


    return (bestInliers >= RANSAC_MIN_INLIERS &&
            bestRMS <= RANSAC_MAX_RMS_ERROR_PX);
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

    cv::Scalar hsvLower = cv::Scalar(5,  30, 120);
    cv::Scalar hsvUpper = cv::Scalar(30, 255, 255);

    cv::Mat img = cv::imread(argv[1], cv::IMREAD_COLOR);
    if (img.empty()) return 1;  

    auto start = std::chrono::high_resolution_clock::now();

    cv::Mat hsv;
    cv::cvtColor(img, hsv, cv::COLOR_BGR2HSV);
    cv::Mat mask;
    cv::inRange(hsv, hsvLower, hsvUpper, mask);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    
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
        for (const auto& p : contours[idx])
            contourPts.emplace_back(p.x, p.y);

        usedAlgebraic = algebraicCircleFit(contourPts, center, radius);

        std::cout << "[DEBUG ALGEBRAIC – FULL CONTOUR]\n";
        if (usedAlgebraic) {
            rms_alg = computeRmsError(contourPts, center, radius);
            std::cout << "RMS: " << rms_alg << "\n";
        } else {
            std::cout << "Fit failed\n";
        }
    }


    // mode 3: debug forced geometric, compare algebraic and geometric performance.
    else if (mode == 3) {
        std::cout << "[DEBUG GEOMETRIC – ARC RANSAC]\n";
        std::cout << "arcPos: " << arcPos.size()
                << " arcNeg: " << arcNeg.size() << "\n";

        float rms_ransac;
        bool ok = ransacCircleFit(arcPts, center, radius, rms_ransac);

        if (!ok) {
            std::cout << "RANSAC failed on arc points\n";
            return 0;
        }

        rms_geo = rms_ransac;

        std::cout << "RMS (RANSAC): " << rms_geo << " px\n";
        std::cout << "Arc points used: " << arcPts.size() << "\n";
    }


    // mode 1: auto
    else {
        bool success = false;

        // 1. Algebraic on arc
        if (algebraicCircleFit(arcPts, center, radius)) {
            rms_alg = computeRmsError(arcPts, center, radius);
            if (radius >= R_MIN && radius <= R_MAX &&
                rms_alg < MAX_RMS_RADIAL_ERROR_PX) {
                usedAlgebraic = true;
                success = true;
            }
        }

        // 2. RANSAC on arc
        if (!success) {
            float rms_ransac;
            if (ransacCircleFit(arcPts, center, radius, rms_ransac)) {
                rms_geo = rms_ransac;
                success = true;
            }
        }

        // 3. RANSAC on full contour (last resort)
        if (!success) {
            std::vector<cv::Point2f> contourPts;
            for (const auto& p : contours[idx])
                contourPts.emplace_back(p.x, p.y);

            float rms_ransac;
            if (ransacCircleFit(contourPts, center, radius, rms_ransac)) {
                rms_geo = rms_ransac;
                arcPts = contourPts;  // for coverage tests
                success = true;
            }
        }

        if (!success) {
            std::cout << "Rejected: no valid circle found\n";
            return 0;
        }

        // --- validation ---
        if (!hasMinimumArcPoints(arcPts, MIN_ARC_PTS)) {
            std::cout << "Rejected: insufficient arc points\n";
            return 0;
        }

        float arcCoverageDeg = computeArcCoverageDegrees(arcPts, center);
        if (arcCoverageDeg < MIN_ARC_COVERAGE_DEG) {
            std::cout << "Rejected: insufficient arc coverage ("
                    << arcCoverageDeg << " deg)\n";
            return 0;
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
    cv::cvtColor(mask, out, cv::COLOR_GRAY2BGR);

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
