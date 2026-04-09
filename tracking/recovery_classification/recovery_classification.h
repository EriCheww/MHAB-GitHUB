#pragma once

#include <opencv2/core.hpp>
#include <string>
#include <vector>

namespace recovery_classify {

struct DetectionHistoryEntry
{
    cv::Point2f center;
    float area = 0.0f;
    double timestampSec = 0.0;
    int frameNumber = 0;
};

enum class LossType
{
    GRADUAL,
    SUDDEN
};

enum class RecoveryMode
{
    UNKNOWN,
    GRADUAL_WAIT,
    SUDDEN_SCAN
};

struct RecoveryClassifyParams
{
    int minHistorySize = 6;
    double minHistoryDurationSec = 0.3;

    float areaDropThreshold = 2000.0f;
    float areaDropRateThreshold = 1500.0f;

    float motionSmoothSpeedThreshold = 20.0f;
    float motionMinSpeedThreshold = 5.0f;

    int areaDecreaseTolerance = 2;
};

std::string lossTypeToString(LossType type);
std::string recoveryModeToString(RecoveryMode mode);

LossType classifyLoss(
    const std::vector<DetectionHistoryEntry>& history,
    const RecoveryClassifyParams& params
);

}