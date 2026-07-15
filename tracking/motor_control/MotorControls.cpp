#include "MotorControls.h"

namespace motor_control {

MotorControls::MotorControls(const PIDGains& gains, double outputMin, double outputMax)
    : panController_(gains, outputMin, outputMax),
      tiltController_(gains, outputMin, outputMax) {}

MotorCommand MotorControls::computeCorrection(const cv::Point2f& error, double dt) {
    MotorCommand command{};
    command.panCorrection = panController_.update(static_cast<double>(error.x), dt);
    command.tiltCorrection = tiltController_.update(static_cast<double>(error.y), dt);
    return command;
}

void MotorControls::reset() {
    panController_.reset();
    tiltController_.reset();
}

void MotorControls::setGains(const PIDGains& gains) {
    panController_.setGains(gains);
    tiltController_.setGains(gains);
}

void MotorControls::setOutputLimits(double outputMin, double outputMax) {
    panController_.setOutputLimits(outputMin, outputMax);
    tiltController_.setOutputLimits(outputMin, outputMax);
}

}  // namespace motor_control
