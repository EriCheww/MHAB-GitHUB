#pragma once

// Generic single-axis PID controller.
//
// Implements SRS-SOF-TRK-SH-010: a PID control strategy for fine tracking
// and motor actuation, chosen for its low computational cost and
// suitability for real-time closed-loop control on the Raspberry Pi 5.
//
// This class is deliberately generic (no dependency on OpenCV or the
// tracking pipeline) so it can be unit tested and tuned in isolation.
// MotorControls.h wraps two instances of this class to turn a 2D pixel
// offset (Sun center error) into pan/tilt correction commands.

namespace motor_control {

struct PIDGains {
    double kp;
    double ki;
    double kd;
};

// Preliminary gains proposed in SRS-SOF-TRK-SH-010. These are a stable
// starting point only - final values depend on mechanical response,
// stepper motor behaviour, camera processing latency, control loop timing,
// and gondola disturbances/vibration, and are expected to change during
// experimental tuning.
inline constexpr PIDGains kDefaultPIDGains{/*kp=*/1.0, /*ki=*/0.05, /*kd=*/0.2};

class PIDController {
public:
    // outputMin/outputMax clamp the controller output (and, via anti-windup,
    // the integral term) to keep overshoot/oscillation in check - important
    // for not degrading image quality during scientific burst capture.
    // Defaults are effectively "unclamped" until the motor subsystem's
    // real limits (e.g. max stepper speed) are known.
    explicit PIDController(const PIDGains& gains = kDefaultPIDGains,
                            double outputMin = -1.0e9,
                            double outputMax = 1.0e9);

    // Computes the PID correction for the given error sample.
    //   error - positional offset for this axis (desired - measured).
    //   dt    - seconds elapsed since the previous call to update().
    // Returns the correction command for this axis.
    double update(double error, double dt);

    // Clears accumulated integral and derivative history. Call this
    // whenever the control loop is interrupted (e.g. Sun lost/reacquired)
    // so stale error history doesn't cause a jump or overshoot once
    // control resumes.
    void reset();

    void setGains(const PIDGains& gains);
    PIDGains gains() const;

    void setOutputLimits(double outputMin, double outputMax);

private:
    PIDGains gains_;
    double integral_;
    double previousError_;
    bool hasPreviousError_;
    double outputMin_;
    double outputMax_;
};

}  // namespace motor_control
