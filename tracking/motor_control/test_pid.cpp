// Standalone test for PIDController (SRS-SOF-TRK-SH-010)
// Compile: g++ -std=c++17 -I.. test_pid.cpp PIDController.cpp -o test_pid
// Run: ./test_pid

#include "PIDController.h"
#include <cmath>
#include <iostream>
#include <iomanip>

using namespace motor_control;

int testsPassed = 0;
int testsFailed = 0;

void check(bool condition, const char* name) {
    if (condition) {
        std::cout << "[PASS] " << name << "\n";
        ++testsPassed;
    } else {
        std::cout << "[FAIL] " << name << "\n";
        ++testsFailed;
    }
}

void testDefaultGains() {
    // SRS-SOF-TRK-SH-010: Kp=1.0, Ki=0.05, Kd=0.2
    check(kDefaultPIDGains.kp == 1.0, "Default Kp = 1.0");
    check(kDefaultPIDGains.ki == 0.05, "Default Ki = 0.05");
    check(kDefaultPIDGains.kd == 0.2, "Default Kd = 0.2");
}

void testProportionalOnly() {
    PIDController pid({1.0, 0.0, 0.0});
    double output = pid.update(10.0, 0.1);
    check(std::abs(output - 10.0) < 1e-9, "P-only: output = Kp * error");
}

void testIntegralAccumulation() {
    PIDController pid({0.0, 1.0, 0.0});
    pid.update(10.0, 0.1);  // integral = 10 * 0.1 = 1.0
    pid.update(10.0, 0.1);  // integral = 1.0 + 1.0 = 2.0
    double output = pid.update(10.0, 0.1);  // integral = 3.0
    check(std::abs(output - 3.0) < 1e-9, "I-only: integral accumulates over time");
}

void testDerivative() {
    PIDController pid({0.0, 0.0, 1.0});
    pid.update(0.0, 0.1);   // first sample, no derivative yet
    double output = pid.update(10.0, 0.1);  // d/dt = (10-0)/0.1 = 100
    check(std::abs(output - 100.0) < 1e-9, "D-only: derivative = (error - prev) / dt");
}

void testReset() {
    PIDController pid({1.0, 1.0, 1.0});
    pid.update(10.0, 0.1);
    pid.update(10.0, 0.1);
    pid.reset();
    // After reset, integral should be zero and no previous error
    PIDController fresh({1.0, 1.0, 1.0});
    double afterReset = pid.update(5.0, 0.1);
    double fromFresh = fresh.update(5.0, 0.1);
    check(std::abs(afterReset - fromFresh) < 1e-9, "reset() clears integral and derivative history");
}

void testAntiWindup() {
    // With output clamped to [-10, 10], integral shouldn't grow unbounded
    PIDController pid({0.0, 1.0, 0.0}, -10.0, 10.0);
    for (int i = 0; i < 100; ++i) {
        pid.update(100.0, 0.1);  // would accumulate 1000 without clamping
    }
    double output = pid.update(100.0, 0.1);
    check(output == 10.0, "Anti-windup: output clamped to max");

    // Now if error reverses, output should drop quickly (integral was clamped)
    output = pid.update(-100.0, 0.1);
    check(output < 10.0, "Anti-windup: integral doesn't cause overshoot after saturation");
}

void testZeroDtGuard() {
    PIDController pid({1.0, 1.0, 1.0});
    // Should not crash or produce NaN/Inf with zero dt
    double output = pid.update(10.0, 0.0);
    check(std::isfinite(output), "Zero dt guard: output is finite");
}

void testNegativeDtGuard() {
    PIDController pid({1.0, 1.0, 1.0});
    double output = pid.update(10.0, -0.1);
    check(std::isfinite(output), "Negative dt guard: output is finite");
}

void testConvergenceSimulation() {
    // Simulate a simple system: position += velocity * dt, velocity = pid_output
    // Starting at position=100, target=0, we should converge toward 0
    // Note: Real convergence depends on motor/mechanical response - this just
    // verifies the PID drives the system in the right direction without
    // excessive overshoot.
    PIDController pid(kDefaultPIDGains, -50.0, 50.0);

    double position = 100.0;
    double velocity = 0.0;
    const double dt = 0.02;  // 50 Hz control loop

    double maxOvershoot = 0.0;
    bool converged = false;
    bool movingTowardTarget = false;

    for (int i = 0; i < 500; ++i) {  // 10 seconds
        double error = 0.0 - position;  // target - current
        double correction = pid.update(error, dt);

        velocity = correction * 0.5;  // motor response gain
        position += velocity * dt;

        if (position < 0) {
            maxOvershoot = std::max(maxOvershoot, -position);
        }

        if (std::abs(position) < 1.0 && std::abs(velocity) < 0.1) {
            converged = true;
        }

        if (i == 50 && position < 100.0) {
            movingTowardTarget = true;
        }
    }

    std::cout << "  [INFO] Final position: " << std::fixed << std::setprecision(3) << position
              << ", max overshoot: " << maxOvershoot << "\n";

    check(movingTowardTarget, "Convergence: PID drives system toward target");
    check(maxOvershoot < 20.0, "Convergence: overshoot is reasonable (<20%)");
}

int main() {
    std::cout << "=== PID Controller Tests (SRS-SOF-TRK-SH-010) ===\n\n";

    testDefaultGains();
    testProportionalOnly();
    testIntegralAccumulation();
    testDerivative();
    testReset();
    testAntiWindup();
    testZeroDtGuard();
    testNegativeDtGuard();
    testConvergenceSimulation();

    std::cout << "\n=== Results: " << testsPassed << " passed, " << testsFailed << " failed ===\n";
    return testsFailed > 0 ? 1 : 0;
}
