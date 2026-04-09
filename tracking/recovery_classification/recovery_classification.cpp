#include "recovery_classification.h"

#include <cmath>

namespace recovery_classify {

    
std::string lossTypeToString(LossType type)
{
    switch (type)
    {
        case LossType::GRADUAL: return "GRADUAL";
        case LossType::SUDDEN:  return "SUDDEN";
        default: return "UNKNOWN";
    }
}


std::string recoveryModeToString(RecoveryMode mode)
{
    switch (mode)
    {
        case RecoveryMode::UNKNOWN:      return "UNKNOWN";
        case RecoveryMode::GRADUAL_WAIT: return "GRADUAL_WAIT";
        case RecoveryMode::SUDDEN_SCAN:  return "SUDDEN_SCAN";
        default: return "UNKNOWN";
    }
}


LossType classifyLoss(
    const std::vector<DetectionHistoryEntry>& history,
    const RecoveryClassifyParams& params
)
{
    if (history.size() < static_cast<size_t>(params.minHistorySize)) {
        return LossType::SUDDEN;
    }

    const double totalTime = history.back().timestampSec - history.front().timestampSec;

    if (totalTime <= 0.0 || totalTime < params.minHistoryDurationSec) {
        return LossType::SUDDEN;
    }

    const float totalAreaDrop = history.front().area - history.back().area;

    std::vector<float> speeds;
    speeds.reserve(history.size() - 1);

    for (size_t i = 1; i < history.size(); ++i) {
        const double dt = history[i].timestampSec - history[i - 1].timestampSec;
        if (dt <= 1e-4) continue;

        const float distance = cv::norm(history[i].center - history[i - 1].center);

        const float speed = static_cast<float>(distance / dt);
        speeds.push_back(speed);
    }

    if (speeds.empty()) {
        return LossType::SUDDEN;
    }

    float avgSpeed = 0.0f;
    for (float s : speeds) avgSpeed += s;
    avgSpeed /= static_cast<float>(speeds.size());

    float speedVariation = 0.0f;
    for (float s : speeds) {
        speedVariation += std::abs(s - avgSpeed);
    }

    speedVariation /= static_cast<float>(speeds.size());

    const bool areaDroppedEnough = totalAreaDrop > params.areaDropThreshold;

    const bool motionSmooth = speedVariation < params.motionSmoothSpeedThreshold;

    if (areaDroppedEnough && motionSmooth) {
        return LossType::GRADUAL;
    }

    return LossType::SUDDEN;
}

}