#pragma once

#include <opencv2/opencv.hpp>

#include "PIDController.h"

// Motor control module (SRS-SOF-TRK-SH-010).
//
// Turns the positional offset between the detected Sun center and the
// desired target position into correction commands for the motor control
// subsystem, using a PID controller per axis (pan/tilt).

namespace motor_control {

// Correction command produced for the motor control subsystem. Units
// mirror the input error (pixels of positional offset) until the motor
// subsystem's own units (e.g. stepper steps/degrees per second) are known -
// see SRS-SOF-TRK-SH-010: "Final gain values will depend on ... Stepper
// motor behaviour".
struct MotorCommand {
    double panCorrection;   // horizontal (x-axis / azimuth) correction
    double tiltCorrection;  // vertical   (y-axis / elevation) correction
};

class MotorControls {
public:
    explicit MotorControls(const PIDGains& gains = kDefaultPIDGains,
                            double outputMin = -1.0e9,
                            double outputMax = 1.0e9);

    // error - positional offset between the detected Sun center and the
    //         desired target position (desiredCenter - trackedCenter), in
    //         pixels. This is the PID error input required by
    //         SRS-SOF-TRK-SH-010.
    // dt    - seconds elapsed since the previous call.
    // Returns the pan/tilt correction commands for the motor control
    // subsystem to maintain payload alignment.
    MotorCommand computeCorrection(const cv::Point2f& error, double dt);

    // Clears accumulated PID state on both axes. Call this whenever
    // tracking is interrupted (Sun lost, state transitions out of/into
    // TRACKING) so stale error history doesn't cause a jump/overshoot when
    // control resumes.
    void reset();

    void setGains(const PIDGains& gains);
    void setOutputLimits(double outputMin, double outputMax);

private:
    PIDController panController_;
    PIDController tiltController_;
};

}  // namespace motor_control
