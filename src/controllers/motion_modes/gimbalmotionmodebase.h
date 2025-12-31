#ifndef GIMBALMOTIONMODEBASE_H
#define GIMBALMOTIONMODEBASE_H

#include <QObject>
#include <QtMath>
#include <QElapsedTimer>
#include <cmath>
#include <cstdint>
#include <limits>
#include "models/domain/systemstatedata.h" // Include for SystemStateData
#include "config/MotionTuningConfig.h"      // Include for runtime-configurable parameters
#include "GimbalStabilizer.h"               // Include for velocity-based stabilization

// Eigen for 3D transformations (rotation matrices)
#include <Eigen/Dense>
#include <Eigen/Geometry>

// Forward declare GimbalController
class GimbalController;

// Low-pass filter class for gyroscope data
class GyroLowPassFilter {
private:
    double cutoffFreq;      // Cutoff frequency in Hz
    double alpha;           // Filter coefficient (0 < alpha < 1) - for fixed sampleRate fallback
    double filteredValue;   // Current filtered value
    bool initialized;       // Whether filter has been initialized

public:
    // Constructor: cutoffFreq in Hz, sampleRate in Hz (optional)
    GyroLowPassFilter(double cutoffFreqHz = 5.0, double sampleRateHz = 100.0)
        : cutoffFreq(cutoffFreqHz), initialized(false)
    {
        // Precompute alpha for fixed sampleRate fallback
        double dt = 1.0 / sampleRateHz;
        double RC = 1.0 / (2.0 * M_PI * cutoffFreq);
        alpha = dt / (RC + dt);
        alpha = qBound(0.0001, alpha, 0.9999);
    }

    // Legacy update (uses precomputed sampleRate alpha)
    double update(double newValue) {
        if (!initialized) {
            filteredValue = newValue;
            initialized = true;
            return filteredValue;
        }
        filteredValue = alpha * newValue + (1.0 - alpha) * filteredValue;
        return filteredValue;
    }

    // New: update using runtime dt (seconds) and cutoffFreq
    double updateWithDt(double newValue, double dt) {
        if (!initialized) {
            filteredValue = newValue;
            initialized = true;
            return filteredValue;
        }
        if (dt <= 0.0) dt = 1e-3;
        double RC = 1.0 / (2.0 * M_PI * cutoffFreq);
        double a = dt / (RC + dt); // computed alpha
        a = qBound(1e-6, a, 0.999999);
        filteredValue = a * newValue + (1.0 - a) * filteredValue;
        return filteredValue;
    }

    void reset() {
        initialized = false;
        filteredValue = 0.0;
    }

    bool isInitialized() const { return initialized; }
};

class GimbalMotionModeBase : public QObject
{
    Q_OBJECT
public:
    explicit GimbalMotionModeBase(QObject* parent = nullptr)
        : QObject(parent)
        , m_gyroXFilter(MotionTuningConfig::instance().filters.gyroCutoffFreqHz, 100.0)
        , m_gyroYFilter(MotionTuningConfig::instance().filters.gyroCutoffFreqHz, 100.0)
        , m_gyroZFilter(MotionTuningConfig::instance().filters.gyroCutoffFreqHz, 100.0)
    {
        // Filters now use runtime-configurable cutoff frequency from motion_tuning.json
    }

    virtual ~GimbalMotionModeBase() = default;

    // Called when we enter this mode
    virtual void enterMode(GimbalController* /*controller*/) {}

    // Called when we exit this mode
    virtual void exitMode(GimbalController* /*controller*/) {}

    // =========================================================================
    // TEMPLATE METHOD PATTERN FOR SAFETY ENFORCEMENT
    // =========================================================================
    // All motion mode updates MUST pass through SafetyInterlock checks.
    // Use updateWithSafety() which is final and cannot be bypassed.
    //
    // USAGE:
    // - GimbalController calls update() or updateWithSafety()
    // - Derived classes implement updateImpl() for their motion logic
    // - Safety checks are automatically enforced before updateImpl() is called
    // =========================================================================

    /**
     * @brief Template method that enforces safety checks before motion updates.
     *
     * This method is FINAL and cannot be overridden. It guarantees that:
     * 1. SafetyInterlock.canMove() is checked before any motion
     * 2. Servos are stopped if safety denies motion
     * 3. updateImpl() is only called when motion is permitted
     *
     * @param controller Pointer to the GimbalController
     * @param dt Time delta in seconds since last update
     */
    void updateWithSafety(GimbalController* controller, double dt) final;

    /**
     * @brief Legacy update method - now delegates to updateWithSafety().
     *
     * For backward compatibility, this method is preserved but now
     * calls updateWithSafety() to ensure safety checks are enforced.
     *
     * @deprecated Use updateWithSafety() directly for clarity.
     * @param controller Pointer to the GimbalController
     * @param dt Time delta in seconds since last update
     */
    virtual void update(GimbalController* controller, double dt);

    void stopServos(GimbalController* controller);
    bool checkSafetyConditions(GimbalController* controller);

protected:
    /**
     * @brief Motion mode implementation - override in derived classes.
     *
     * This method contains the actual motion logic for each mode.
     * It is called by updateWithSafety() ONLY after safety checks pass.
     *
     * @note Do NOT call checkSafetyConditions() in your implementation -
     *       it has already been called by updateWithSafety().
     *
     * @param controller Pointer to the GimbalController
     * @param dt Time delta in seconds since last update
     */
    virtual void updateImpl(GimbalController* controller, double dt);

public:
    /**
     * @brief Updates the Z-axis gyro bias if the vehicle is stationary.
     * This function should be called periodically from the main controller loop.
     * @param systemState The current system state data.
     */
    void updateGyroBias(const SystemStateData& systemState);

    /**
     * @brief Converts gimbal angles from platform frame to world frame (old scalar version).
     * @param gimbalAz_platform Current gimbal azimuth in platform frame (degrees)
     * @param gimbalEl_platform Current gimbal elevation in platform frame (degrees)
     * @param platform_roll Platform roll angle from AHRS (degrees)
     * @param platform_pitch Platform pitch angle from AHRS (degrees)
     * @param platform_yaw Platform yaw angle from AHRS (degrees)
     * @param worldAz Output world-frame azimuth (degrees)
     * @param worldEl Output world-frame elevation (degrees)
     */
    void convertGimbalToWorldFrame(double gimbalAz_platform, double gimbalEl_platform,
                                    double platform_roll, double platform_pitch, double platform_yaw,
                                    double& worldAz, double& worldEl);

    /**
     * @brief Converts gimbal velocity vectors from gimbal frame to world frame using rotation matrices.
     * @param linVel_gimbal_mps Linear velocity in gimbal frame (m/s)
     * @param angVel_gimbal_dps Angular velocity in gimbal frame (deg/s)
     * @param azDeg Gimbal azimuth angle (degrees)
     * @param elDeg Gimbal elevation angle (degrees)
     * @param linVel_world_mps Output linear velocity in world frame (m/s)
     * @param angVel_world_dps Output angular velocity in world frame (deg/s)
     */
    void convertGimbalToWorldFrame(
        const Eigen::Vector3d& linVel_gimbal_mps,
        const Eigen::Vector3d& angVel_gimbal_dps,
        double azDeg,
        double elDeg,
        Eigen::Vector3d& linVel_world_mps,
        Eigen::Vector3d& angVel_world_dps);

protected:
    // ========================================================================
    // VELOCITY TIMER & CENTRAL HELPERS (Expert Review - Critical Fixes)
    // ========================================================================

    /**
     * @brief Velocity timer used across modes to compute dt
     */
    QElapsedTimer m_velocityTimer;

    /**
     * @brief Clamp dt to avoid divide-by-zero and unrealistic values
     * @param dt Time delta in seconds
     * @return Clamped dt (minimum 1 ms, maximum implicit)
     */
    static inline double clampDt(double dt) {
        return qMax(dt, 1e-3); // 1 ms minimum
    }

    /**
     * @brief Compute IIR filter alpha from time constant tau and actual dt
     * @param tau Time constant in seconds (e.g., 0.15 for 150ms response)
     * @param dt Measured time step in seconds
     * @return Alpha coefficient for this update
     */
    static inline double alphaFromTauDt(double tau, double dt) {
        if (dt <= 0.0) dt = 1e-3;
        return 1.0 - std::exp(-dt / tau);
    }

    /**
     * @brief Apply time-based rate limiting (prevents acceleration spikes)
     * @param desired Target velocity (deg/s)
     * @param prev Previous velocity (deg/s)
     * @param maxDelta Maximum change allowed this cycle (deg/s) = MAX_ACCEL * dt
     * @return Rate-limited velocity
     */
    static inline double applyRateLimitTimeBased(double desired, double prev, double maxDelta) {
        double delta = desired - prev;
        delta = qBound(-maxDelta, delta, maxDelta);
        return prev + delta;
    }

    /**
     * @brief Split signed 32-bit integer into two 16-bit registers (bit-preserving)
     * @param value Signed 32-bit value (e.g., motor speed in Hz)
     * @return Vector of two 16-bit registers [upper, lower]
     */
    static inline QVector<quint16> splitInt32ToRegs(int32_t value) {
        uint32_t u = static_cast<uint32_t>(static_cast<int32_t>(value));
        return { static_cast<quint16>((u >> 16) & 0xFFFF),
                 static_cast<quint16>(u & 0xFFFF) };
    }

public:
    /**
     * @brief Start velocity timer - call in enterMode() of derived classes
     */
    void startVelocityTimer() { m_velocityTimer.start(); }

protected:

    /**
     * @brief Calculates and sends final servo commands, incorporating full kinematic stabilization.
     * @param controller Pointer to the GimbalController to access system state (IMU, angles).
     * @param desiredAzVelocity The desired azimuth velocity relative to a stable world frame (deg/s).
     * @param desiredElVelocity The desired elevation velocity relative to a stable world frame (deg/s).
     * @param enableStabilization True to apply stabilization corrections, false to send raw commands.
     * @param dt Time delta in seconds since last update (Expert Review Fix)
     */
    void sendStabilizedServoCommands(GimbalController* controller,
                                 double desiredAzVelocity,
                                 double desiredElVelocity,
                                 bool enableStabilization,
                                 double dt);
    // --- UNIFIED PID CONTROLLER ---
    struct PIDController {
        double Kp = 0.0;
        double Ki = 0.0;
        double Kd = 0.0;
        double integral = 0.0;
        double maxIntegral = 1.0;
        double previousError = 0.0;
        double previousMeasurement = 0.0;

        void reset() {
            integral = 0.0;
            previousError = 0.0;
        }
    };
    struct PIDOutput {
        double p_term = 0.0;
        double i_term = 0.0;
        double d_term = 0.0;
        double total = 0.0;
    };
    double pidCompute(PIDController& pid, double error, double setpoint, double measurement, bool derivativeOnMeasurement, double dt);

    // We can provide a convenient overload for the old "derivative on error" method
    // This way, you don't have to change your existing code in the scanning modes.
    double pidCompute(PIDController& pid, double error, double dt);
    // Helper methods for common operations
    //       double joystickInput, quint16 angularVelocity);

    void writeServoCommands(class ServoDriverDevice* driverInterface, double finalVelocity, float scalingFactor = 250.0f);
    void writeTargetPosition(ServoDriverDevice* driverInterface,  long targetPositionInSteps);
    void setAcceleration(class ServoDriverDevice* driverInterface, quint32 acceleration = DEFAULT_ACCELERATION);
    bool checkElevationLimits(double currentEl, double targetVelocity, bool upperLimit, bool lowerLimit);
    /**
     * @brief Configures the AZD-KX driver for continuous velocity control mode.
     *        This should be called once when a motion mode is entered.
     */
    void configureVelocityMode(class ServoDriverDevice* driverInterface);

    /**
     * @brief Writes a new speed command to the driver in real-time.
     * @param finalVelocity The calculated velocity in degrees/second.
     * @param scalingFactor Converts deg/s to Hz for the driver.
     */
    void writeVelocityCommand(class ServoDriverDevice* driverInterface,
                              double finalVelocity,
                              double scalingFactor,
                              qint32& lastSpeedHz);
    // --- COMMON CONSTANTS ---
    // Servo Control
    static constexpr quint32 DEFAULT_ACCELERATION = 100000;
    static constexpr quint32 MAX_ACCELERATION = 1000000000;
    static constexpr quint32 MAX_SPEED = 30000;

    // Servo register addresses
    static constexpr quint16 SPEED_REGISTER = 0x0480;
    static constexpr quint16 DIRECTION_REGISTER = 0x007D;
    static constexpr quint16 ACCEL_REGISTERS[] = {0x2A4, 0x282, 0x600, 0x680};

    // Direction commands
    static constexpr quint16 DIRECTION_FORWARD = 0x4000;
    static constexpr quint16 DIRECTION_REVERSE = 0x8000;
    static constexpr quint16 DIRECTION_STOP = 0x0000;

    // Motion limits (hardware-specific, not tunable)
    static constexpr double MIN_ELEVATION_ANGLE = -10.0;
    static constexpr double MAX_ELEVATION_ANGLE = 50.0;

    // Scaling factors
    static constexpr float SPEED_SCALING_FACTOR_SCAN = 250.0f;
    static constexpr float SPEED_SCALING_FACTOR_TRP_SCAN = 250.0f;

    // ========================================================================
    // RUNTIME-CONFIGURABLE PARAMETERS (loaded from motion_tuning.json)
    // ========================================================================
    // These inline functions provide access to tunable parameters without rebuild

    // Common PID/Scan constants
    static inline double ARRIVAL_THRESHOLD_DEG() {
        return MotionTuningConfig::instance().motion.arrivalThresholdDeg;
    }
    static inline double UPDATE_INTERVAL_S() {
        return MotionTuningConfig::instance().motion.updateIntervalS;
    }

    // Motion acceleration limits (deg/s²)
    static inline double MAX_ACCELERATION_DEG_S2() {
        return MotionTuningConfig::instance().motion.maxAccelerationDegS2;
    }
    static inline double SCAN_MAX_ACCEL_DEG_S2() {
        return MotionTuningConfig::instance().motion.scanMaxAccelDegS2;
    }
    static inline double TRP_MAX_ACCEL_DEG_S2() {
        return MotionTuningConfig::instance().motion.trpMaxAccelDegS2;
    }

    // Default travel speeds (deg/s)
    static inline double TRP_DEFAULT_TRAVEL_SPEED() {
        return MotionTuningConfig::instance().motion.trpDefaultTravelSpeed;
    }
    static inline double MAX_VELOCITY() {
        return MotionTuningConfig::instance().motion.maxVelocityDegS;
    }

    // Unit conversion constants - centralized
    static inline double AZ_STEPS_PER_DEGREE() {
        return MotionTuningConfig::instance().servo.azStepsPerDegree;
    }
    static inline double EL_STEPS_PER_DEGREE() {
        return MotionTuningConfig::instance().servo.elStepsPerDegree;
    }

    /**
     * @brief Normalize angle to [-180, 180] range
     * @param angle Angle in degrees
     * @return Normalized angle in degrees
     */
    static inline double normalizeAngle180(double angle) {
        while (angle > 180.0) angle -= 360.0;
        while (angle < -180.0) angle += 360.0;
        return angle;
    }

private:
    // Helper for angle conversions (scalar)
    static inline double degToRad(double deg) { return deg * (M_PI / 180.0); }
    static inline double radToDeg(double rad) { return rad * (180.0 / M_PI); }

    // Helper for angle conversions (Eigen vectors - element-wise)
    static inline Eigen::Vector3d degToRad(const Eigen::Vector3d& deg) {
        return deg * (M_PI / 180.0);
    }
    static inline Eigen::Vector3d radToDeg(const Eigen::Vector3d& rad) {
        return rad * (180.0 / M_PI);
    }

    // ========================================================================
    // STABILIZATION
    // ========================================================================

    // Velocity-based stabilizer (stateless, shared across all modes)
    static GimbalStabilizer s_stabilizer;

    // Gyro filters for stabilization (legacy code)
    GyroLowPassFilter m_gyroXFilter;
    GyroLowPassFilter m_gyroYFilter;
    GyroLowPassFilter m_gyroZFilter;

    // Gyro bias
    double m_gyroBiasX = 0.0;
    double m_gyroBiasY = 0.0;
    double m_gyroBiasZ = 0.0;

    // Last servo command tracking for change detection (prevents redundant Modbus writes)
    qint32 m_lastAzSpeedHz = std::numeric_limits<qint32>::max(); // Initialize to invalid value
    qint32 m_lastElSpeedHz = std::numeric_limits<qint32>::max();

    // ========================================================================
    // LEGACY FUNCTIONS (Deprecated - use GimbalStabilizer instead)
    // ========================================================================

    // ========================================================================
    // DEPRECATED FUNCTIONS REMOVED (2025-12-27)
    // The following functions were removed - use GimbalStabilizer instead:
    //   - calculateRequiredGimbalAngles()
    //   - calculateHybridStabilizationCorrection()
    //   - calculateStabilizationCorrection()
    // ========================================================================
};

#endif // GIMBALMOTIONMO


