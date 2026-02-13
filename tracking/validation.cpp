#include <opencv2/opencv.hpp>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <iomanip>
#include <unordered_map>

#include "cb_detect/CB_detect_v7_export.h"

// ================= CONFIG =================
const std::string IMAGE_DIR = "image_generation/output/images/";
const std::string FAIL_FILE = "failures.txt";
const std::string CSV_FILE  = "results.csv";
const int BINARY_THRESHOLD = 80;
const float CENTER_ERR_THRESHOLD_UPPER = 6.0f;
const float CENTER_ERR_THRESHOLD_LOWER = 4.0f;

// ================= HELPERS =================

std::vector<std::string> loadFailureIDs()
{
    std::vector<std::string> ids;
    std::ifstream f(FAIL_FILE);
    std::string line;
    while (std::getline(f, line)) {
        if (line.size() >= 3)
            ids.push_back(line.substr(0, 3));
    }
    return ids;
}

std::vector<std::string> loadHighCenterErrorIDs(float lower, float upper)
{
    std::vector<std::string> ids;
    std::ifstream f(CSV_FILE);
    std::string line;

    std::getline(f, line); // skip header

    while (std::getline(f, line)) {
        std::stringstream ss(line);
        std::string cell;
        std::vector<std::string> cols;

        while (std::getline(ss, cell, ','))
            cols.push_back(cell);

        if (cols.size() < 7)
            continue;

        float centerErr = std::stof(cols[6]);
        if (centerErr >= lower && centerErr <= upper)
            ids.push_back(cols[0]);
    }
    return ids;
}

// Load full failure reason per ID
std::unordered_map<std::string, std::string> loadFailureReasons()
{
    std::unordered_map<std::string, std::string> map;
    std::ifstream f(FAIL_FILE);
    std::string line;

    while (std::getline(f, line)) {
        if (line.size() < 4)
            continue;

        std::string id = line.substr(0, 3);
        std::string reason = line.substr(3);

        map[id] = reason;
    }

    return map;
}

void drawFailureText(
    cv::Mat& img,
    const std::string& id,
    const std::string& reason
) {
    const int font = cv::FONT_HERSHEY_SIMPLEX;
    const double scale = 1.2;   // 🔼 larger text
    const int thickness = 3;

    std::string line1 = "ID: " + id;
    std::string line2 = "FAIL: " + reason;

    int baseline1 = 0, baseline2 = 0;
    cv::Size size1 = cv::getTextSize(line1, font, scale, thickness, &baseline1);
    cv::Size size2 = cv::getTextSize(line2, font, scale, thickness, &baseline2);

    int width  = std::max(size1.width, size2.width) + 30;
    int height = size1.height + size2.height + baseline1 + baseline2 + 40;

    cv::Rect bg(10, 10, width, height);

    // Background box
    cv::rectangle(img, bg, cv::Scalar(0, 0, 0), cv::FILLED);

    // Text lines
    cv::putText(
        img,
        line1,
        cv::Point(20, 20 + size1.height),
        font,
        scale,
        cv::Scalar(0, 255, 255), // yellow
        thickness
    );

    cv::putText(
        img,
        line2,
        cv::Point(20, 30 + size1.height + size2.height),
        font,
        scale,
        cv::Scalar(0, 0, 255), // red
        thickness
    );
}


void drawResult(
    const cv::Mat& imageGray,
    const cb_detect::CircleDetectResult& res
) {
    cv::Mat out;
    cv::cvtColor(imageGray, out, cv::COLOR_GRAY2BGR);

    // Circle & center
    cv::circle(out, res.center, (int)res.radius, cv::Scalar(255, 0, 255), 2);
    cv::drawMarker(out, res.center, cv::Scalar(255, 0, 255),
                   cv::MARKER_CROSS, 300, 2);

    // Arc points
    for (const auto& p : res.arcPos)
        cv::circle(out, p, 2, cv::Scalar(255, 0, 0), -1);   // blue

    for (const auto& p : res.arcNeg)
        cv::circle(out, p, 2, cv::Scalar(0, 0, 255), -1);   // red

    for (const auto& p : res.arcUsed)
        cv::circle(out, p, 3, cv::Scalar(0, 255, 0), -1);   // green

    cv::imshow("Validation Viewer", out);
}

// ================= MAIN =================

int main()
{
    std::cout << "Select mode:\n";
    std::cout << "  [1] Review failures (failures.txt)\n";
    std::cout << "  [2] Review large center error (> threshold)\n";
    std::cout << "Choice: ";

    int mode;
    std::cin >> mode;

    std::vector<std::string> ids;

    if (mode == 1) {
        ids = loadFailureIDs();
        std::cout << "Loaded " << ids.size() << " failure cases\n";
    }
    else if (mode == 2) {
        ids = loadHighCenterErrorIDs(
            CENTER_ERR_THRESHOLD_LOWER,
            CENTER_ERR_THRESHOLD_UPPER
        );
        std::cout << "Loaded " << ids.size()
                  << " cases with center_error_px between "
                  << CENTER_ERR_THRESHOLD_LOWER << " and "
                  << CENTER_ERR_THRESHOLD_UPPER << "\n";
    }
    else {
        std::cerr << "Invalid mode\n";
        return 1;
    }

    // Load failure reasons once
    auto failureReasons = loadFailureReasons();

    cb_detect::CircleDetectParams params;

    for (size_t i = 0; i < ids.size(); ++i) {

        std::string filename = IMAGE_DIR + ids[i] + ".png";

        cv::Mat image = cv::imread(filename, cv::IMREAD_GRAYSCALE);
        if (image.empty()) {
            std::cerr << "Failed to load " << filename << "\n";
            continue;
        }

        cv::Mat mask;
        cv::threshold(image, mask, BINARY_THRESHOLD, 255, cv::THRESH_BINARY);

        cb_detect::CircleDetectResult res;
        bool found = cb_detect::detectCircle(
            mask, res, params, cb_detect::Mode::Validate
        );

        std::cout << "\n[" << ids[i] << "] ";

        if (!found) {
            std::cout << "Detection FAILED\n";

            cv::Mat out;
            cv::cvtColor(image, out, cv::COLOR_GRAY2BGR);

            auto it = failureReasons.find(ids[i]);
            if (it != failureReasons.end()) {
                drawFailureText(out, ids[i], it->second);
            } else {
                drawFailureText(out, ids[i], "UnknownFailure");
            }

            cv::imshow("Validation Viewer", out);
        }
        else {
            std::cout << "RMS=" << res.rms
                      << " | arcUsed=" << res.arcUsed.size()
                      << "\n";
            drawResult(image, res);
        }

        int key = cv::waitKey(0);
        if (key == 27) break; // ESC
    }

    cv::destroyAllWindows();
    return 0;
}
