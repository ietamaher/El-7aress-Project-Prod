#include "autosectorscanmotionmode.h"
#include "controllers/gimbalcontroller.h" // For GimbalController
#include "hardware/devices/servodriverdevice.h" // For ServoDriverDevice
#include <QDebug>
#include <QDateTime>  // For throttling model updates
#include <cmath>    // For std::abs, std::hypot
#include <QtGlobal> // For qBound

AutoSectorScanMotionMode::AutoSectorScanMotionMode(QObject* parent)
    : GimbalMotionModeBase(parent), m_scanZoneSet(false), m_movingToPoint2(true)
{
    // ✅ Load PID gains from runtime config (field-tunable without rebuild)
    const auto& cfg = MotionTuningConfig::instance();
    m_azPid.Kp = cfg.autoScanAz.kp;
    m_azPid.Ki = cfg.autoScanAz.ki;
    m_azPid.Kd = cfg.autoScanAz.kd;
    m_azPid.maxIntegral = cfg.autoScanAz.maxIntegral;

    m_elPid.Kp = cfg.autoScanEl.kp;
    m_elPid.Ki = cfg.autoScanEl.ki;
    m_elPid.Kd = cfg.autoScanEl.kd;
    m_elPid.maxIntegral = cfg.autoScanEl.maxIntegral;
}

void AutoSectorScanMotionMode::enterMode(GimbalController* controller) {
    qDebug() << "[AutoSectorScanMotionMode] Enter";
    if (!m_scanZoneSet || !m_activeScanZone.isEnabled) {
        qWarning() << "AutoSectorScanMotionMode: No active scan zone set or zone disabled. Exiting scan.";
        if (controller) controller->setMotionMode(MotionMode::Idle);
        return;
    }

    // Reset PID controllers to start fresh
    m_azPid.reset();
    m_elPid.reset();

    // Reset rate limiting state
    m_previousDesiredAzVel = 0.0;
    m_previousDesiredElVel = 0.0;

    // Set the initial direction and target
    m_movingToPoint2 = true; // Always start by moving towards point 2
    m_targetAz = m_activeScanZone.az2;
    m_targetEl = m_activeScanZone.el2;

    // ✅ CRITICAL: Start velocity timer for dt measurement
    startVelocityTimer();

    if (controller) {
        // Set a slower, smoother acceleration for scanning motion
        if (auto azServo = controller->azimuthServo()) setAcceleration(azServo, 1000000);
        if (auto elServo = controller->elevationServo()) setAcceleration(elServo, 1000000);
    }
}

void AutoSectorScanMotionMode::exitMode(GimbalController* controller) {
    qDebug() << "[AutoSectorScanMotionMode] Exit";
    stopServos(controller);
    m_scanZoneSet = false; // Reset state for the next time the mode is activated
}

void AutoSectorScanMotionMode::setActiveScanZone(const AutoSectorScanZone& scanZone) {
    m_activeScanZone = scanZone;
    m_scanZoneSet = true;
    qDebug() << "[AutoSectorScanMotionMode] Active scan zone set to ID:" << scanZone.id;
}


// ===================================================================================
// =================== FULLY UPDATED AND UNIFORM UPDATE METHOD =====================
// ===================================================================================
/*void AutoSectorScanMotionMode::update(GimbalController* controller) {
    // Top-level guard clauses for mode-specific state
    if (!controller || !m_scanZoneSet || !m_activeScanZone.isEnabled) {
        // If the scan zone is disabled while we are in this mode, stop and let the
        // main controller logic switch to Idle.
        stopServos(controller);
        if (controller && m_scanZoneSet && !m_activeScanZone.isEnabled) {
            controller->setMotionMode(MotionMode::Idle);
        }
        return;
    }

    SystemStateData data = controller->systemStateModel()->data();
    double errAz = m_targetAz - data.gimbalAz;
    double errEl = m_targetEl - data.gimbalEl;

    // --- ROBUSTNESS: Use a 2D distance check for arrival ---
    double distanceToTarget = std::sqrt( errEl * errEl); //errAz * errAz +

    if (distanceToTarget < ARRIVAL_THRESHOLD_DEG) {
        qDebug() << "AutoSectorScan: Reached point" << (m_movingToPoint2 ? "2" : "1");
        
        // Toggle direction
        m_movingToPoint2 = !m_movingToPoint2;
        if (m_movingToPoint2) {
            m_targetAz = m_activeScanZone.az2;
            m_targetEl = m_activeScanZone.el2;
        } else {
            m_targetAz = m_activeScanZone.az1;
            m_targetEl = m_activeScanZone.el1;
        }
        
        // Reset PID controllers to prevent integral windup from any previous steady-state error
        m_azPid.reset();
        m_elPid.reset();
        
        // Recalculate error for the new target for this update cycle
        errAz = m_targetAz - data.gimbalAz;
        errEl = m_targetEl - data.gimbalEl;
    }

    // --- UNIFORMITY: This block is now identical in pattern to other modes ---
    // Outer Loop: PID calculates the DESIRED world velocity to move to the next scan point
    double desiredAzVelocity = 0; //pidCompute(m_azPid, errAz, UPDATE_INTERVAL_S);
    double desiredElVelocity = pidCompute(m_elPid, errEl, UPDATE_INTERVAL_S);
    
    // Limit the velocity vector's magnitude to the defined scan speed
    double desiredSpeedDegS = m_activeScanZone.scanSpeed;
    double totalVelocityMag = std::sqrt(desiredAzVelocity * desiredAzVelocity + desiredElVelocity * desiredElVelocity);
    if (totalVelocityMag > desiredSpeedDegS && desiredSpeedDegS > 0) {
        desiredAzVelocity = (desiredAzVelocity / totalVelocityMag) * desiredSpeedDegS;
        desiredElVelocity = (desiredElVelocity / totalVelocityMag) * desiredSpeedDegS;
    }
     qDebug() << "AreaScan: desiredElVelocity " << desiredElVelocity << " totalVelocityMag " << totalVelocityMag   ;    
    // Pass the final desired world velocity to the base class for stabilization and hardware output.
    sendStabilizedServoCommands(controller, desiredAzVelocity, desiredElVelocity);
}
*/

// ===================================================================================
// =================== REFACTORED UPDATE METHOD WITH MOTION PROFILING ================
// ===================================================================================
void AutoSectorScanMotionMode::update(GimbalController* controller) {
    // Top-level guard clauses
    if (!controller || !m_scanZoneSet || !m_activeScanZone.isEnabled) {
        stopServos(controller);
        if (controller && m_scanZoneSet && !m_activeScanZone.isEnabled) {
            controller->setMotionMode(MotionMode::Idle);
        }
        return;
    }

    SystemStateData data = controller->systemStateModel()->data();

    // ✅ CRITICAL FIX: Measure dt using timer (not fixed UPDATE_INTERVAL_S!)
    const auto& cfg = MotionTuningConfig::instance();
    double dt_s = UPDATE_INTERVAL_S();
    if (m_velocityTimer.isValid()) {
        dt_s = clampDt(m_velocityTimer.restart() / 1000.0);
    } else {
        m_velocityTimer.start();
    }

    // Calculate errors - use encoder for Az, IMU pitch for El
    double errAz = m_targetAz - data.gimbalAz;
    double errEl = m_targetEl - data.imuPitchDeg;

    // ✅ ROBUSTNESS: Use hypot for proper 2D distance
    double distanceToTarget = std::hypot(errAz, errEl);

    // --- 1. ENDPOINT HANDLING LOGIC ---
    if (distanceToTarget < cfg.autoScanParams.arrivalThresholdDeg) {
        qDebug() << "AutoSectorScan: Reached point" << (m_movingToPoint2 ? "2" : "1");

        // Toggle direction for next sweep
        m_movingToPoint2 = !m_movingToPoint2;
        if (m_movingToPoint2) {
            m_targetAz = m_activeScanZone.az2;
            m_targetEl = m_activeScanZone.el2;
        } else {
            m_targetAz = m_activeScanZone.az1;
            m_targetEl = m_activeScanZone.el1;
        }

        // Reset PIDs for new sweep
        m_azPid.reset();
        m_elPid.reset();

        // Recalculate for new target
        errAz = m_targetAz - data.gimbalAz;
        errEl = m_targetEl - data.imuPitchDeg;
        distanceToTarget = std::hypot(errAz, errEl);
    }

    // --- 2. MOTION PROFILE LOGIC ---
    double desiredAzVelocity = 0.0;
    double desiredElVelocity = 0.0;

    if (m_activeScanZone.scanSpeed <= 0) {
        // Scan speed zero - use PID to hold position
        desiredAzVelocity = pidCompute(m_azPid, errAz, dt_s);
        desiredElVelocity = pidCompute(m_elPid, errEl, dt_s);
    } else {
        // ✅ CRITICAL FIX: Compute deceleration distance from kinematics (from config)
        const double a = cfg.motion.scanMaxAccelDegS2;
        const double v = m_activeScanZone.scanSpeed;
        double decelDist = (v * v) / (2.0 * std::max(a, 1e-3));

        // Allow overriding decelDist from config if specified
        if (cfg.autoScanParams.decelerationDistanceDeg > 0) {
            decelDist = cfg.autoScanParams.decelerationDistanceDeg;
        }

        if (distanceToTarget < decelDist) {
            // DECELERATION ZONE: Use PID to slow down smoothly
            qDebug() << "AreaScan: Decelerating. Distance:" << distanceToTarget;
            desiredAzVelocity = pidCompute(m_azPid, errAz, dt_s);
            desiredElVelocity = pidCompute(m_elPid, errEl, dt_s);
        } else {
            // CRUISING ZONE: Move at constant scan speed
            double dirAz = errAz / distanceToTarget;
            double dirEl = errEl / distanceToTarget;

            // ✅ CRITICAL BUG FIX: REMOVE MAGIC * 0.1 MULTIPLIER!
            // Use actual scanSpeed, not 10% of it!
            desiredAzVelocity = dirAz * v;
            desiredElVelocity = dirEl * v;

            // ✅ SOFT PID RESET: Reset integrator only (keep derivative history)
            m_azPid.integral = 0.0;
            m_elPid.integral = 0.0;
        }
    }

    // ✅ CRITICAL FIX: Apply time-based rate limiting (from config)
    double maxDelta = cfg.motion.scanMaxAccelDegS2 * dt_s;  // deg/s^2 * s = deg/s
    desiredAzVelocity = applyRateLimitTimeBased(desiredAzVelocity, m_previousDesiredAzVel, maxDelta);
    desiredElVelocity = applyRateLimitTimeBased(desiredElVelocity, m_previousDesiredElVel, maxDelta);

    // Update previous velocities for next cycle
    m_previousDesiredAzVel = desiredAzVelocity;
    m_previousDesiredElVel = desiredElVelocity;

    // Throttle world-target publish to 10 Hz
    static qint64 lastPublishMs = 0;
    qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (nowMs - lastPublishMs >= 100 && data.imuConnected) {
        double worldAz, worldEl;
        convertGimbalToWorldFrame(m_targetAz, m_targetEl,
                                  data.imuRollDeg, data.imuPitchDeg, data.imuYawDeg,
                                  worldAz, worldEl);

        auto stateModel = controller->systemStateModel();
        SystemStateData updatedState = stateModel->data();
        updatedState.targetAzimuth_world = worldAz;
        updatedState.targetElevation_world = worldEl;
        updatedState.useWorldFrameTarget = true;
        stateModel->updateData(updatedState);
        lastPublishMs = nowMs;
    }

    // Send stabilized commands
    sendStabilizedServoCommands(controller, desiredAzVelocity, desiredElVelocity, true);
}
