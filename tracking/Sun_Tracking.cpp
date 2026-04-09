#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <chrono>

#include "ROI/ROI.h"
#include "cb_detect/CB_detect_v7_export.h"
#include "recovery_classification/recovery_classification.h"

enum class TrackingState
{
    SEARCHING,
    TRACKING,
    RECOVERY
};

int main() 
{   
    // Initilisation 
    cb_detect::CircleDetectResult results;
    cb_detect::CircleDetectParams params; // NEED TUNNING
    recovery_classify::RecoveryClassifyParams classifyParams;
    
    cv::Mat image;
    int binaryThreshold = 80; // NEED TUNNING
    
    // ROI
    cv::Rect roiRect;
    int baseHalfSize = 100; // NEED TUNNING
    cv::Point2f trackedCenter{};

    TrackingState state = TrackingState::SEARCHING;

    // Recovery
    std::vector<int> recoverySizes = {150, 200, 300}; // NEED TUNNING
    int recoveryStep = 0;
    std::vector<recovery_classify::DetectionHistoryEntry> detectionHistory;
    const int maxHistory = 15; // NEED TUNNING

    recovery_classify::RecoveryMode recoveryMode = recovery_classify::RecoveryMode::UNKNOWN;
    int gradualWaitCounter = 0;
    const int gradualWaitLimit = 10; // NEED TUNNING

    using Clock = std::chrono::steady_clock;
    const auto startTime = Clock::now();

    int frameNumber = 0;

    // Distance 
    int targetIndex = 0;
    std::vector<cv::Point2f> targetCenters = {
        {320.0f, 240.0f}, // NEED TUNNING
        {400.0f, 240.0f}, // NEED TUNNING
        {240.0f, 240.0f} // NEED TUNNING
    };  
    
    while (true) {
        frameNumber++;

        // CODE FOR GETTING IMAGE FROM CAMERA HERE
        
        if (image.empty()) {
            continue;
        }

        // Decide ROI size based on state
        switch (state)
        {
            case TrackingState::SEARCHING: {
                roiRect = cv::Rect(0, 0, image.cols, image.rows);
                break;
            }

            case TrackingState::TRACKING: {
                roiRect = roi::getROI(image, trackedCenter, baseHalfSize);
                break;
            }

            case TrackingState::RECOVERY: {
                if (recoveryMode == recovery_classify::RecoveryMode::GRADUAL_WAIT) {
                    roiRect = roi::getROI(image, trackedCenter, recoverySizes.back());
                } else {
                    roiRect = roi::getROI(image, trackedCenter, recoverySizes[recoveryStep]);
                }
                break;
            }
        }

        // ROI image step 1
        cv::Mat image_roi = image(roiRect);

        // Binary image step 2
        cv::Mat image_roi_mask;
        cv::threshold(image_roi, image_roi_mask, binaryThreshold, 255, cv::THRESH_BINARY);

        // Detect sun and find center step 3
        bool found = cb_detect::detectCircle(image_roi_mask, results, params);

        if (found) {
            results.center.x += roiRect.x;
            results.center.y += roiRect.y;
            trackedCenter = results.center;

            // add only successful detection into histroy 
            if (state == TrackingState::TRACKING) {
                double timestampSec =
                    std::chrono::duration<double>(Clock::now() - startTime).count();

                detectionHistory.push_back({
                    trackedCenter,
                    static_cast<float>(results.contourArea),
                    timestampSec,
                    frameNumber
                });

                // keep histroy rolling
                if (static_cast<int>(detectionHistory.size()) > maxHistory) {
                    detectionHistory.erase(detectionHistory.begin());
                }
            }
        }

        switch (state) {
            case TrackingState::SEARCHING: {
                if (found) {
                    recoveryStep = 0;
                    state = TrackingState::TRACKING;
                } else { 
                    // MOTOR CONTROLL TO ROTATE EVERYTHING 45 OR SOMETHING DEGREES TO SEARCH NEW AREA
                    break;
                }

                break;
            }

            case TrackingState::TRACKING: {
                if (found) {
                    // CODE FOR EXTERNAL SOURCE TO SELECT targetIndex (Most likely from Data Compression) HERE

                    cv::Point2f desiredCenter = targetCenters[targetIndex];
                    cv::Point2f distance = desiredCenter - trackedCenter;

                    // condtions needed to determine if alread locked and can idle

                    // CODE FOR MOTOR CONTROLLS (distance as input) HERE
                } else {
                    recovery_classify::LossType lossType = recovery_classify::classifyLoss(detectionHistory, classifyParams);

                    if (lossType == recovery_classify::LossType::GRADUAL) {
                        recoveryMode = recovery_classify::RecoveryMode::GRADUAL_WAIT;
                        recoveryStep = static_cast<int>(recoverySizes.size()) - 1; // jump to largest ROI
                        gradualWaitCounter = 0;
                    }
                    else {
                        recoveryMode = recovery_classify::RecoveryMode::SUDDEN_SCAN;
                        recoveryStep = 0; // normal stepped recovery
                    }

                    state = TrackingState::RECOVERY;
                }

                break;
            }

            case TrackingState::RECOVERY: {
                if (found) {
                    recoveryStep = 0;
                    gradualWaitCounter = 0;
                    recoveryMode = recovery_classify::RecoveryMode::UNKNOWN;
                    state = TrackingState::TRACKING;
                }
                else {
                    if (recoveryMode == recovery_classify::RecoveryMode::GRADUAL_WAIT) {
                        gradualWaitCounter++;

                        if (gradualWaitCounter >= gradualWaitLimit) {
                            gradualWaitCounter = 0;
                            recoveryStep = 0;
                            recoveryMode = recovery_classify::RecoveryMode::UNKNOWN;
                            state = TrackingState::SEARCHING;
                        }
                    }
                    else {
                        recoveryStep++;

                        if (recoveryStep >= static_cast<int>(recoverySizes.size())) {
                            recoveryStep = 0;
                            recoveryMode = recovery_classify::RecoveryMode::UNKNOWN;
                            state = TrackingState::SEARCHING;
                        }
                    }
                }

                break;
            }
        }
    }
}



// state without idle : 
// SEARCHING ---(if sun found)---> TRACKING
// SEARCHING ---(if sun not found)---> SEARCHING
// TRACKING ---(if sun found, large distance)---> TRACKING
// TRACKING ---(if sun lost, determine recovery classification)---> RECOVERY
// RECOVERY ---(gradual loss)---> RECOVERY (wait)
// RECOVERY ---(sudden loss)---> SEARCHING
