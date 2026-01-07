#ifndef PIDCONTROLLER_H
#define PIDCONTROLLER_H

#include <QtMath>
#include <QDebug>

/**
 * @brief Structure to hold PID controller parameters and state.
 */
struct PIDController {
    double Kp;           ///< Proportional gain
    double Ki;           ///< Integral gain
    double Kd;           ///< Derivative gain
    double integral;     ///< Accumulated integral error
    double prevError;    ///< Previous error for derivative calculation
    double maxIntegral;  ///< Maximum absolute value for integral windup protection

    /**
     * @brief Resets the PID controller's internal state.
     */
    void reset() {
        integral = 0.0;
        prevError = 0.0;
    }
};

/**
 * @brief Computes the PID output for a given error and time step.
 * @param pid Reference to the PIDController struct.
 * @param error Current error value.
 * @param dt Time step (delta time) since the last computation.
 * @return The computed PID output.
 */
inline double pidCompute(PIDController& pid, double error, double dt) {
    // Proportional term
    double proportional = pid.Kp * error;

    // Integral term with windup protection
    pid.integral += error * dt;
    pid.integral = qBound(-pid.maxIntegral, pid.integral, pid.maxIntegral);

    // Derivative term
    double derivative = (error - pid.prevError) / dt;

    // Total PID output
    double output = proportional + (pid.Ki * pid.integral) + (pid.Kd * derivative);

    // Store current error for next iteration's derivative calculation
    pid.prevError = error;

    return output;
}

#endif  // PIDCONTROLLER_H
