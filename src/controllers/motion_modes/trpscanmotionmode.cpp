#include "trpscanmotionmode.h"
#include "controllers/gimbalcontroller.h" // For GimbalController and SystemStateData
#include <QDebug>
#include <QDateTime>  // For throttling model updates
#include <cmath> // For std::sqrt, std::hypot

TRPScanMotionMode::TRPScanMotionMode()
    : m_currentState(State::Idle)
    , m_currentTrpIndex(0)
{
    // Configure PID gains for responsive and smooth stopping at waypoints.
    // These might need tuning based on your gimbal\"s physical characteristics.
    // ✅ Load PID gains from runtime config (field-tunable without rebuild)
    const auto& cfg = MotionTuningConfig::instance();
    m_azPid.Kp = cfg.trpScanAz.kp;
    m_azPid.Ki = cfg.trpScanAz.ki;
    m_azPid.Kd = cfg.trpScanAz.kd;
    m_azPid.maxIntegral = cfg.trpScanAz.maxIntegral;

    m_elPid.Kp = cfg.trpScanEl.kp;
    m_elPid.Ki = cfg.trpScanEl.ki;
    m_elPid.Kd = cfg.trpScanEl.kd;
    m_elPid.maxIntegral = cfg.trpScanEl.maxIntegral;
}

void TRPScanMotionMode::setActiveTRPPage(const std::vector<TargetReferencePoint>& trpPage)
{
    qDebug() << "[TRPScanMotionMode] Active TRP page set with" << trpPage.size() << "points.";
    m_trpPage = trpPage;
    // Reset progress, ready to start when `enterMode` is called.
    m_currentTrpIndex = 0;
    m_currentState = m_trpPage.empty() ? State::Idle : State::Moving;
}

void TRPScanMotionMode::enterMode(GimbalController* controller)
{
    qDebug() << "[TRPScanMotionMode] Enter";
    if (m_trpPage.empty()) {
        qWarning() << "TRPScanMotionMode: No TRP page set. Exiting scan.";
        // The GimbalController will set mode to Idle, so we just stop.
        stopServos(controller);
        m_currentState = State::Idle;
        return;
    }

    // Reset state to start the path from the beginning
    m_currentTrpIndex = 0;
    m_currentState = State::Moving;
    m_azPid.reset();
    m_elPid.reset();

    // Reset rate limiting state
    m_previousDesiredAzVel = 0.0;
    m_previousDesiredElVel = 0.0;

    // ✅ CRITICAL: Start velocity timer for dt measurement
    startVelocityTimer();

    if (controller) {
        // Set an aggressive acceleration for point-to-point moves
        if (auto azServo = controller->azimuthServo()) setAcceleration(azServo, 200000);
        if (auto elServo = controller->elevationServo()) setAcceleration(elServo, 200000);
    }
    qDebug() << "[TRPScanMotionMode] Starting path, moving to point 0.";
}

void TRPScanMotionMode::exitMode(GimbalController* controller)
{
    qDebug() << "[TRPScanMotionMode] Exit";
    stopServos(controller);
    m_currentState = State::Idle;
}

// ===================================================================================
// =================== REFACTORED UPDATE METHOD WITH MOTION PROFILING ================
// ===================================================================================
void TRPScanMotionMode::update(GimbalController* controller)
{
    if (!controller) return;

    // --- Main State Machine Logic ---
    switch (m_currentState)
    {
        case State::Idle: {
            stopServos(controller);
            return; // Do nothing
        }

        case State::Halted: {
            // We are waiting at a TRP. Check if the halt timer has expired.
            const auto& currentTrp = m_trpPage[m_currentTrpIndex];
            
            // Assuming `haltTime` is in seconds.
            if (m_haltTimer.isValid() && m_haltTimer.elapsed() >= static_cast<qint64>(currentTrp.haltTime * 1000.0)) {
                qDebug() << "[TRPScanMotionMode] Halt time finished at point" << m_currentTrpIndex;
                
                // Advance to the next point in the path
                m_currentTrpIndex++;

                // --- MODIFIED LOGIC FOR LOOPING ---
                if (m_currentTrpIndex >= m_trpPage.size()) {
                    // Reached the end of the path. Loop back to the beginning.
                    qDebug() << "[TRPScanMotionMode] Path loop finished. Returning to point 0.";
                    m_currentTrpIndex = 0; 
                }
                // --- END OF MODIFIED LOGIC ---

                // This part now executes for both advancing to the next point and looping back.
                qDebug() << "[TRPScanMotionMode] Moving to point" << m_currentTrpIndex;
                m_currentState = State::Moving;
                // Reset PIDs for the next deceleration phase.
                m_azPid.reset();
                m_elPid.reset();
            }
            // While halted, the servos should remain stopped.
            return;
        }

        case State::Moving: {
            // This is the core motion logic.
            if (m_currentTrpIndex >= m_trpPage.size()) {
                m_currentState = State::Idle; // Safety guard
                return;
            }

            const auto& targetTrp = m_trpPage[m_currentTrpIndex];
            SystemStateData data = controller->systemStateModel()->data();

            // ✅ CRITICAL FIX: Measure dt using timer (not fixed UPDATE_INTERVAL_S!)
            const auto& cfg = MotionTuningConfig::instance();
            double dt_s = UPDATE_INTERVAL_S();
            if (m_velocityTimer.isValid()) {
                dt_s = clampDt(m_velocityTimer.restart() / 1000.0);
            } else {
                m_velocityTimer.start();
            }

            // Calculate errors
            double errAz = targetTrp.azimuth - data.gimbalAz;
            double errEl = targetTrp.elevation - data.imuPitchDeg;

            // Normalize Azimuth error for shortest path
            while (errAz > 180.0)  errAz -= 360.0;
            while (errAz < -180.0) errAz += 360.0;

            // ✅ ROBUSTNESS: Use hypot for proper 2D distance
            double distanceToTarget = std::hypot(errAz, errEl);

            // --- 1. ARRIVAL CHECK ---
            if (distanceToTarget < cfg.trpScanParams.arrivalThresholdDeg) {
                qDebug() << "[TRPScanMotionMode] Arrived at point" << m_currentTrpIndex;
                stopServos(controller);
                m_currentState = State::Halted;
                m_haltTimer.start();
                return;
            }

            // --- 2. MOTION PROFILE LOGIC ---
            double desiredAzVelocity = 0.0;
            double desiredElVelocity = 0.0;

            // ✅ Use default TRP travel speed from config (TargetReferencePoint struct doesn't have per-point speed)
            double travelSpeed = cfg.motion.trpDefaultTravelSpeed;

            if (travelSpeed <= 0.0) {
                // Speed zero - use PID to hold position
                desiredAzVelocity = pidCompute(m_azPid, errAz, dt_s);
                desiredElVelocity = pidCompute(m_elPid, errEl, dt_s);
            } else {
                // ✅ CRITICAL FIX: Compute deceleration distance from kinematics (from config)
                const double a = cfg.motion.trpMaxAccelDegS2;
                double decelDist = (travelSpeed * travelSpeed) / (2.0 * std::max(a, 1e-3));

                // Allow overriding decelDist from config if specified
                if (cfg.trpScanParams.decelerationDistanceDeg > 0) {
                    decelDist = cfg.trpScanParams.decelerationDistanceDeg;
                }

                if (distanceToTarget < decelDist) {
                    // DECELERATION ZONE: Use PID to slow down smoothly
                    qDebug() << "TRP: Decelerating. Dist:" << distanceToTarget;
                    desiredAzVelocity = pidCompute(m_azPid, errAz, dt_s);
                    desiredElVelocity = pidCompute(m_elPid, errEl, dt_s);
                } else {
                    // CRUISING ZONE: Move at constant speed
                    double dirAz = errAz / distanceToTarget;
                    double dirEl = errEl / distanceToTarget;
                    desiredAzVelocity = dirAz * travelSpeed;
                    desiredElVelocity = dirEl * travelSpeed;

                    // ✅ SOFT PID RESET: Reset integrator only (keep derivative history)
                    m_azPid.integral = 0.0;
                    m_elPid.integral = 0.0;
                }
            }

            // ✅ CRITICAL FIX: Apply time-based rate limiting (from config)
            double maxDelta = cfg.motion.trpMaxAccelDegS2 * dt_s;
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
                convertGimbalToWorldFrame(targetTrp.azimuth, targetTrp.elevation,
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

            sendStabilizedServoCommands(controller, desiredAzVelocity, desiredElVelocity, true);
            break;
        }
    }
}
