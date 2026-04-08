#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>

#include "ROI/ROI.h"
#include "cb_detect/CB_detect_v7_export.h"

enum class TrackingState
{
    SEARCHING,
    TRACKING,
    RECOVERY,
    IDLE
};

int main() 
{   
    // Initilisation 
    cb_detect::CircleDetectResult results;
    cb_detect::CircleDetectParams params; // NEED TUNNING
    
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

    // Distance 
    int targetIndex = 0;
    std::vector<cv::Point2f> targetCenters = {
        {320.0f, 240.0f}, // NEED TUNNING
        {400.0f, 240.0f}, // NEED TUNNING
        {240.0f, 240.0f} // NEED TUNNING
    };  
    
    while (true) {

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
                roiRect = roi::getROI(image, trackedCenter, recoverySizes[recoveryStep]);
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
        }

        switch (state) {
            case TrackingState::SEARCHING: {
                if (found) {
                    recoveryStep = 0;
                    state = TrackingState::TRACKING;
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
                } 
                else {
                    // Move into recovery instead of full search
                    recoveryStep = 0;
                    state = TrackingState::RECOVERY;
                }

                break;
            }

            case TrackingState::RECOVERY: {
                if (found) {
                    recoveryStep = 0;
                    state = TrackingState::TRACKING;
                } 
                else {
                    recoveryStep++;

                    // If failed after all steps,
                    if (recoveryStep >= static_cast<int>(recoverySizes.size())) {
                        recoveryStep = 0;
                        
                        // for now just go back to searching, but we ideally want a new idle State, for when we determine if
                        // tracking and recovery failed becuase the gondola is blocking it (gradual blocking over time), or 
                        // completely lost (suddenly disappeared). We can already do this as cb_detect returns contour area. 
                        // so we probably have too store past 20 or so results and use the area and center to determine gradual 
                        // or sudden lost. If gradual, just power off and wait, if sudden search. 

                        state = TrackingState::SEARCHING;
                    }
                }

                break;
            }

            case TrackingState::IDLE: {
                // idle state for when sun is aligned or gradual blocking is true. Need to cut power to stepper? if thats an option
                // power can be saved.

                break;
            }
        }
    }
}


// state with idle : 
// SEARCHING ---(if sun found)---> TRACKING
// SEARCHING ---(if sun not found)---> SEARCHING
// TRACKING ---(if sun found, large distance)---> TRACKING
// TRACKING ---(if sun found, none distance)---> IDLE
// TRACKING ---(if sun lost)---> RECOVERY
// RECOVERY ---(determined gradual lost)---> IDLE
// RECOVERY ---(determined sudden lost)---> SEARCHING
// IDLE ---(taret and center distance grew too large)---> TRACKING
// IDLE ---(sun graudally unblocked)---> TRACKING
// IDLE ---(Max timeout reached)---> SEARCHING

// state without idle : 
// SEARCHING ---(if sun found)---> TRACKING
// SEARCHING ---(if sun not found)---> SEARCHING
// TRACKING ---(if sun found, large distance)---> TRACKING
// TRACKING ---(if sun lost)---> RECOVERY
// RECOVERY ---(determined sudden lost)---> SEARCHING
