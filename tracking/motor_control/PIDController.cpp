#include "PIDController.h"

#include <cmath>

namespace motor_control {

PIDController::PIDController(const PIDGains& gains, double outputMin, double outputMax)
    : gains_(gains),
      integral_(0.0),
      previousError_(0.0),
      hasPreviousError_(false),
      outputMin_(outputMin),
      outputMax_(outputMax) {}

double PIDController::update(double error, double dt) {
    // Guard against a degenerate/zero dt (e.g. first sample, stalled clock)
    // which would otherwise blow up the integral/derivative terms.
    if (!(dt > 0.0) || !std::isfinite(dt)) {
        dt = 1e-3;
    }

    // Proportional term: reacts to the current offset.
    const double pTerm = gains_.kp * error;

    // Integral term: accumulates offset over time to remove steady-state
    // error (e.g. constant gondola drift).
    integral_ += error * dt;
    double iTerm = gains_.ki * integral_;

    // Derivative term: reacts to how fast the offset is changing, which
    // damps oscillation/overshoot as the Sun approaches the target
    // position - important for stable behaviour during burst capture.
    double dTerm = 0.0;
    if (hasPreviousError_) {
        dTerm = gains_.kd * (error - previousError_) / dt;
    }
    previousError_ = error;
    hasPreviousError_ = true;

    double output = pTerm + iTerm + dTerm;

    // Clamp output to the motor subsystem's valid range and apply
    // integral clamping (anti-windup): if the output is saturated, unwind
    // the integral term so it doesn't keep growing while saturated, which
    // would otherwise cause overshoot once the error shrinks again.
    if (output > outputMax_) {
        output = outputMax_;
        if (gains_.ki != 0.0) {
            integral_ = (outputMax_ - pTerm - dTerm) / gains_.ki;
        }
    } else if (output < outputMin_) {
        output = outputMin_;
        if (gains_.ki != 0.0) {
            integral_ = (outputMin_ - pTerm - dTerm) / gains_.ki;
        }
    }

    return output;
}

void PIDController::reset() {
    integral_ = 0.0;
    previousError_ = 0.0;
    hasPreviousError_ = false;
}

void PIDController::setGains(const PIDGains& gains) {
    gains_ = gains;
}

PIDGains PIDController::gains() const {
    return gains_;
}

void PIDController::setOutputLimits(double outputMin, double outputMax) {
    outputMin_ = outputMin;
    outputMax_ = outputMax;
}

}  // namespace motor_control
