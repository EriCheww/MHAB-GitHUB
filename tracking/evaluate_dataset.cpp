#include <opencv2/opencv.hpp>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <unordered_map>
#include <chrono>
#include <cmath>
#include <limits>

#include "cb_detect/CB_detect_v5_export.h"

// ================= BLOCK TYPE =================
enum class BlockType {
    NONE,
    VERTICAL_WIRE,
    CIRCLE,
    ELLIPSE,
    SQUARE,
    UNKNOWN
};

BlockType parseBlockType(const std::string& s) {
    if (s == "NONE") return BlockType::NONE;
    if (s == "VERTICAL_WIRE") return BlockType::VERTICAL_WIRE;
    if (s == "CIRCLE") return BlockType::CIRCLE;
    if (s == "ELLIPSE") return BlockType::ELLIPSE;
    if (s == "SQUARE") return BlockType::SQUARE;
    return BlockType::UNKNOWN;
}

std::string blockTypeName(BlockType t) {
    switch (t) {
        case BlockType::NONE: return "NONE";
        case BlockType::VERTICAL_WIRE: return "VERTICAL_WIRE";
        case BlockType::CIRCLE: return "CIRCLE";
        case BlockType::ELLIPSE: return "ELLIPSE";
        case BlockType::SQUARE: return "SQUARE";
        default: return "UNKNOWN";
    }
}

// ================= GROUND TRUTH =================
struct GroundTruth {
    float cx = -1;
    float cy = -1;
    float radius = -1;
};

// ================= MAIN =================
int main()
{
    const std::string IMAGE_DIR = "image_generation/output/images/";
    const std::string META_FILE = "image_generation/output/metadata.json";

    const int NUM_IMAGES = 1000;
    const int BINARY_THRESHOLD = 80;
    const float RMS_THRESHOLD = 2.0f;

    // ---------------- LOAD METADATA ----------------
    std::unordered_map<std::string, GroundTruth> gtMap;
    std::unordered_map<std::string, BlockType> blockMap;

    std::ifstream meta(META_FILE);
    if (!meta) {
        std::cerr << "Failed to open metadata.json\n";
        return 1;
    }

    std::string line, currentId;
    while (std::getline(meta, line)) {

        if (line.find("\"id\"") != std::string::npos) {
            auto p1 = line.find("\"", line.find(":") + 1);
            auto p2 = line.find("\"", p1 + 1);
            currentId = line.substr(p1 + 1, p2 - p1 - 1);
        }

        if (line.find("\"center\"") != std::string::npos) {
            auto l = line.find("[");
            auto c = line.find(",");
            auto r = line.find("]");
            gtMap[currentId].cx = std::stof(line.substr(l + 1, c - l - 1));
            gtMap[currentId].cy = std::stof(line.substr(c + 1, r - c - 1));
        }

        if (line.find("\"radius\"") != std::string::npos &&
            line.find("block") == std::string::npos) {
            gtMap[currentId].radius =
                std::stof(line.substr(line.find(":") + 1));
        }

        if (line.find("\"type\"") != std::string::npos) {
            auto p1 = line.find("\"", line.find(":") + 1);
            auto p2 = line.find("\"", p1 + 1);
            blockMap[currentId] =
                parseBlockType(line.substr(p1 + 1, p2 - p1 - 1));
        }
    }

    // ---------------- OUTPUT FILES ----------------
    std::ofstream csv("results.csv");
    std::ofstream failures("failures.txt");

    csv << "id,found,center_x,center_y,radius,"
           "rms,center_error_px,method,block,time_ms\n";

    // ---------------- STATS ----------------
    cb_detect::CircleDetectParams params;

    double rmsSum = 0.0;
    int rmsCount = 0;

    double centerErrSum = 0.0;
    double centerErrMin = std::numeric_limits<double>::max();
    double centerErrMax = 0.0;
    int centerErrCount = 0;

    // Center error histogram bins
    // [0–2), [2–4), [4–6), [6–8), [8–10), [>=10)
    int centerErrBins[6] = {0, 0, 0, 0, 0, 0};

    double timeSum = 0.0;
    double timeMin = std::numeric_limits<double>::max();
    double timeMax = 0.0;
    int timeCount = 0;

    int totalFailures = 0;

    std::unordered_map<BlockType, int> blockCount;
    std::unordered_map<BlockType, int> blockFailures;

    // -------- METHOD STATS --------
    int algCount = 0, ranCount = 0;
    double algRmsSum = 0, ranRmsSum = 0;
    double algCenterErrSum = 0, ranCenterErrSum = 0;
    double algTimeSum = 0, ranTimeSum = 0;

    // ---------------- PROCESS DATASET ----------------
    for (int i = 1; i <= NUM_IMAGES; ++i) {

        std::ostringstream id;
        id << std::setw(3) << std::setfill('0') << i;
        std::string sid = id.str();

        std::string filename = IMAGE_DIR + sid + ".png";

        cb_detect::CircleDetectResult res;

        // Load image COLOR      
        // cv::Scalar hsvLower = {5, 30, 120};
        // cv::Scalar hsvUpper = {30, 255, 255};

        // cv::Mat image = cv::imread(filename, cv::IMREAD_COLOR);
        // if (image.empty()) {
        //     failures << sid << " (load_failed)\n";
        //     totalFailures++;
        //     continue;
        // }
        
        // auto t0 = std::chrono::high_resolution_clock::now();
        // cv::Mat hsv;
        // cv::cvtColor(image, hsv, cv::COLOR_BGR2HSV);
        // cv::Mat mask;
        // cv::inRange(hsv, hsvLower, hsvUpper, mask);

        // Load Image GREYSCALE 
        cv::Mat image = cv::imread(filename, cv::IMREAD_GRAYSCALE);
        if (image.empty()) {
            failures << sid << " (load_failed)\n";
            totalFailures++;
            continue;
        }
        auto t0 = std::chrono::high_resolution_clock::now();

        cv::Mat mask;
        cv::threshold(image, mask, BINARY_THRESHOLD, 255, cv::THRESH_BINARY);

        bool found = cb_detect::detectCircle(mask, res, params);

        auto t1 = std::chrono::high_resolution_clock::now();

        double timeMs =
            std::chrono::duration<double, std::milli>(t1 - t0).count();

        timeSum += timeMs;
        timeMin = std::min(timeMin, timeMs);
        timeMax = std::max(timeMax, timeMs);
        timeCount++;

        BlockType bt = BlockType::UNKNOWN;
        if (blockMap.count(sid))
            bt = blockMap[sid];

        blockCount[bt]++;

        double centerErrorPx = -1.0;
        bool bad = false;

        if (!found) {
            bad = true;
            failures << sid << " (detect_failed)\n";
        }
        else {
            if (gtMap.count(sid)) {
                const auto& gt = gtMap[sid];
                double dx = res.center.x - gt.cx;
                double dy = res.center.y - gt.cy;
                centerErrorPx = std::sqrt(dx * dx + dy * dy);

                centerErrSum += centerErrorPx;
                centerErrMin = std::min(centerErrMin, centerErrorPx);
                centerErrMax = std::max(centerErrMax, centerErrorPx);
                centerErrCount++;

                // ---- BINNING ----
                if (centerErrorPx < 2.0)       centerErrBins[0]++;
                else if (centerErrorPx < 4.0)  centerErrBins[1]++;
                else if (centerErrorPx < 6.0)  centerErrBins[2]++;
                else if (centerErrorPx < 8.0)  centerErrBins[3]++;
                else if (centerErrorPx < 10.0) centerErrBins[4]++;
                else                           centerErrBins[5]++;
            }

            rmsSum += res.rms;
            rmsCount++;

            if (res.usedAlgebraic) {
                algCount++;
                algRmsSum += res.rms;
                algCenterErrSum += centerErrorPx;
                algTimeSum += timeMs;
            } else {
                ranCount++;
                ranRmsSum += res.rms;
                ranCenterErrSum += centerErrorPx;
                ranTimeSum += timeMs;
            }

            if (res.rms > RMS_THRESHOLD) {
                bad = true;
                failures << sid << " (high_rms=" << res.rms << ")\n";
            }
        }

        if (bad) {
            totalFailures++;
            blockFailures[bt]++;
        }

        csv << sid << ","
            << found << ","
            << (found ? res.center.x : -1) << ","
            << (found ? res.center.y : -1) << ","
            << (found ? res.radius : -1) << ","
            << (found ? res.rms : -1) << ","
            << centerErrorPx << ","
            << (found ? (res.usedAlgebraic ? "algebraic" : "ransac") : "none")
            << ","
            << blockTypeName(bt) << ","
            << timeMs
            << "\n";
    }

    csv.close();
    failures.close();

    // ---------------- SUMMARY ----------------
    std::cout << "\n=== DATASET EVALUATION SUMMARY ===\n";
    std::cout << "Total images: " << NUM_IMAGES << "\n";
    std::cout << "Total failures: " << totalFailures << "\n";

    if (rmsCount > 0)
        std::cout << "Average RMS: " << (rmsSum / rmsCount) << "\n";

    if (centerErrCount > 0) {
        std::cout << "Center error (px):\n";
        std::cout << "  Avg: " << (centerErrSum / centerErrCount) << "\n";
        std::cout << "  Min: " << centerErrMin << "\n";
        std::cout << "  Max: " << centerErrMax << "\n";

        std::cout << "\nCenter error distribution (px):\n";
        std::cout << "  [0-2):   " << centerErrBins[0] << "\n";
        std::cout << "  [2-4):   " << centerErrBins[1] << "\n";
        std::cout << "  [4-6):   " << centerErrBins[2] << "\n";
        std::cout << "  [6-8):   " << centerErrBins[3] << "\n";
        std::cout << "  [8-10):  " << centerErrBins[4] << "\n";
        std::cout << "  [>=10):  " << centerErrBins[5] << "\n";
    }

    if (timeCount > 0) {
        std::cout << "\nDetection time (ms):\n";
        std::cout << "  Avg: " << (timeSum / timeCount) << "\n";
        std::cout << "  Min: " << timeMin << "\n";
        std::cout << "  Max: " << timeMax << "\n";
    }

    if (algCount > 0) {
        std::cout << "\nAlgebraic:\n";
        std::cout << "  Count: " << algCount << "\n";
        std::cout << "  Avg RMS: " << algRmsSum / algCount << "\n";
        std::cout << "  Avg center err: " << algCenterErrSum / algCount << " px\n";
        std::cout << "  Avg time: " << algTimeSum / algCount << " ms\n";
    }

    if (ranCount > 0) {
        std::cout << "\nRANSAC:\n";
        std::cout << "  Count: " << ranCount << "\n";
        std::cout << "  Avg RMS: " << ranRmsSum / ranCount << "\n";
        std::cout << "  Avg center err: " << ranCenterErrSum / ranCount << " px\n";
        std::cout << "  Avg time: " << ranTimeSum / ranCount << " ms\n";
    }

    std::cout << "\nFailures by block type:\n";
    for (auto& [bt, count] : blockCount) {
        std::cout << "  " << blockTypeName(bt)
                  << ": " << count
                  << " samples, "
                  << blockFailures[bt]
                  << " failures\n";
    }

    std::cout << "\nSaved results.csv and failures.txt\n";
    return 0;
}
