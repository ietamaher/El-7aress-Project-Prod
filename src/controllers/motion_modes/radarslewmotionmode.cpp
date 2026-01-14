#include "radarslewmotionmode.h"
#include "controllers/gimbalcontroller.h" // For GimbalController and SystemStateData
#include "models/domain/systemstatemodel.h" // For SimpleRadarPlot
#include <QDebug>
#include <cmath>
#include <QtGlobal>

RadarSlewMotionMode::RadarSlewMotionMode(QObject* parent)
    : GimbalMotionModeBase(parent), // Call base constructor
    m_currentTargetId(0),
    m_isSlewInProgress(false),
    m_lastErrAz(0.0),
    m_lastErrEl(0.0)
{
    // ✅ Load PID gains from runtime config (field-tunable without rebuild)
    const auto& cfg = MotionTuningConfig::instance();
    m_azPid.Kp = cfg.radarSlewAz.kp;
    m_azPid.Ki = cfg.radarSlewAz.ki;
    m_azPid.Kd = cfg.radarSlewAz.kd;
    m_azPid.maxIntegral = cfg.radarSlewAz.maxIntegral;

    m_elPid.Kp = cfg.radarSlewEl.kp;
    m_elPid.Ki = cfg.radarSlewEl.ki;
    m_elPid.Kd = cfg.radarSlewEl.kd;
    m_elPid.maxIntegral = cfg.radarSlewEl.maxIntegral;
}

void RadarSlewMotionMode::enterMode(GimbalController* controller)
{
    qDebug() << "[RadarSlewMotionMode] Enter. Awaiting slew command.";
    m_isSlewInProgress = false;
    m_currentTargetId = 0;
    m_previousDesiredAzVel = 0.0;
    m_previousDesiredElVel = 0.0;
    m_lastErrAz = 0.0;
    m_lastErrEl = 0.0;

    // The GimbalController::setMotionMode already called exitMode on the previous mode,
    // which should have already stopped the servos. Calling it again here is
    // redundant and is likely triggering the problematic signal feedback loop.
    // Let's remove it and rely on the exitMode of the previous state.
    // if (controller) {
    //     stopServos(controller); // <<< TRY COMMENTING THIS OUT
    // }

}

void RadarSlewMotionMode::exitMode(GimbalController* controller)
{
    qDebug() << "[RadarSlewMotionMode] Exit.";
    stopServos(controller);
}

void RadarSlewMotionMode::updateImpl(GimbalController* controller, double dt)
{
    // NOTE: Safety checks are handled by base class updateWithSafety()
    // This method is only called after SafetyInterlock.canMove() returns true

    // --- 1. Pre-condition Checks ---
    if (!controller || !controller->systemStateModel()) { return; }
    if (dt <= 0) dt = 1e-3;

    SystemStateData data = controller->systemStateModel()->data();

    // --- 2. Check for a New Slew Command from the Model ---
    bool isNewTarget = (data.selectedRadarTrackId != 0 && data.selectedRadarTrackId != m_currentTargetId);
    
    if (isNewTarget) {
        qInfo() << "[RadarSlewMotionMode] New slew command received for Target ID:" << data.selectedRadarTrackId;
        m_currentTargetId = data.selectedRadarTrackId;
        m_azPid.reset();
        m_elPid.reset();
        // Reset velocity smoothing for new target
        m_previousDesiredAzVel = 0.0;
        m_previousDesiredElVel = 0.0;
    }

    // --- 2b. CONTINUOUS TARGET TRACKING: Update target position every cycle ---
    if (m_currentTargetId != 0) {
        auto it = std::find_if(data.radarPlots.begin(), data.radarPlots.end(),
                               [&](const SimpleRadarPlot& p){ return p.id == m_currentTargetId; });

        if (it != data.radarPlots.end()) {
            // Update target position from latest radar data
            m_targetAz = it->azimuth;
            m_targetEl = atan2(-SYSTEM_HEIGHT_METERS, it->range) * (180.0 / M_PI);
            m_isSlewInProgress = true;

            if (isNewTarget) {
                qDebug() << "[RadarSlewMotionMode] Target set to Az:" << m_targetAz << "| Calculated El:" << m_targetEl;
            }
        } else {
            // Target lost from radar - stop tracking
            qWarning() << "[RadarSlewMotionMode] Target ID" << m_currentTargetId << "lost from radar. Stopping.";
            m_isSlewInProgress = false;
            m_currentTargetId = 0;
            stopServos(controller);
            return;
        }
    }

    // --- 3. Execute Movement if a Slew is in Progress ---
    if (!m_isSlewInProgress) {
        stopServos(controller);
        return;
    }

    // Convert radar target to world-frame for AHRS stabilization
    if (data.imuConnected) {
        double worldAz, worldEl;
        convertGimbalToWorldFrame(m_targetAz, m_targetEl,
                                  data.imuRollDeg, data.imuPitchDeg, data.imuYawDeg,
                                  worldAz, worldEl);

        auto stateModel = controller->systemStateModel();
        SystemStateData updatedState = stateModel->data();
        updatedState.targetAzimuth_world = worldAz;
        updatedState.targetElevation_world = worldEl;
        updatedState.useWorldFrameTarget = true; // Enable stabilized radar slewing
        stateModel->updateData(updatedState);
    }

    // --- 4. Calculate Errors (FIXED: elevation sign like TRP) ---
    double errAz = normalizeAngle180(m_targetAz - data.gimbalAz);
    double errEl = -(m_targetEl - data.gimbalEl);  // FIXED: Negation + use encoder like TRP

    // Note: No "arrival stop" - for continuous tracking we just naturally slow down
    // when close to target. If target moves, we follow it.

    // --- 5. MOTION PROFILING (with realistic deceleration) ---
    
    // Motion profile constants
    static constexpr double ACCEL_DEG_S2 = 50.0;            // For rate limiting only
    static constexpr double DECEL_EFFECTIVE_DEG_S2 = 15.0;  // REALISTIC decel for stopping calc (servo can't do 50!)
    static constexpr double CRUISE_SPEED_DEG_S = 12.0;      // Max cruise speed
    static constexpr double SMOOTHING_TAU_S = 0.05;         // Exponential filter time constant
    static constexpr double FINE_THRESHOLD_DEG = 1.0;       // Switch to PID below this for fine control

    //------------------------------------------------------------
    // AZIMUTH: Trapezoidal profile with REALISTIC deceleration
    //------------------------------------------------------------
    double distAz = std::abs(errAz);
    double dirAz = (errAz > 0) ? 1.0 : -1.0;
    double desiredAzVel;
    
    if (distAz < FINE_THRESHOLD_DEG) {
        // FINE ZONE: Use PID with Kd for damping (prevents oscillation!)
        double pTerm = m_azPid.Kp * errAz;
        double dTerm = -m_azPid.Kd * (errAz - m_lastErrAz) / dt;  // Derivative on error
        m_lastErrAz = errAz;
        desiredAzVel = pTerm + dTerm;
        desiredAzVel = std::clamp(desiredAzVel, -3.0, 3.0);  // Limit fine adjustment speed
    } else {
        // CRUISE/DECEL: Kinematic with CONSERVATIVE deceleration
        // v = sqrt(2 * a * d) but using realistic 'a'
        double v_stop_az = std::sqrt(2.0 * DECEL_EFFECTIVE_DEG_S2 * distAz);
        desiredAzVel = dirAz * std::min(CRUISE_SPEED_DEG_S, v_stop_az);
        m_lastErrAz = errAz;  // Keep tracking for derivative
    }

    // Time-based rate limiting
    double maxDeltaAz = ACCEL_DEG_S2 * dt;
    desiredAzVel = std::clamp(desiredAzVel, m_previousDesiredAzVel - maxDeltaAz, m_previousDesiredAzVel + maxDeltaAz);

    // Exponential smoothing filter
    double alpha = dt / (SMOOTHING_TAU_S + dt);
    double smoothedAzVel = alpha * desiredAzVel + (1.0 - alpha) * m_previousDesiredAzVel;
    m_previousDesiredAzVel = smoothedAzVel;

    //------------------------------------------------------------
    // ELEVATION: Same profile with damping
    //------------------------------------------------------------
    double distEl = std::abs(errEl);
    double dirEl = (errEl > 0) ? 1.0 : -1.0;
    double desiredElVel;
    
    if (distEl < FINE_THRESHOLD_DEG) {
        // FINE ZONE: PID with Kd damping
        double pTerm = m_elPid.Kp * errEl;
        double dTerm = -m_elPid.Kd * (errEl - m_lastErrEl) / dt;
        m_lastErrEl = errEl;
        desiredElVel = pTerm + dTerm;
        desiredElVel = std::clamp(desiredElVel, -3.0, 3.0);
    } else {
        // CRUISE/DECEL
        double v_stop_el = std::sqrt(2.0 * DECEL_EFFECTIVE_DEG_S2 * distEl);
        desiredElVel = dirEl * std::min(CRUISE_SPEED_DEG_S, v_stop_el);
        m_lastErrEl = errEl;
    }

    // Time-based rate limiting
    double maxDeltaEl = ACCEL_DEG_S2 * dt;
    desiredElVel = std::clamp(desiredElVel, m_previousDesiredElVel - maxDeltaEl, m_previousDesiredElVel + maxDeltaEl);

    // Exponential smoothing filter
    double smoothedElVel = alpha * desiredElVel + (1.0 - alpha) * m_previousDesiredElVel;
    m_previousDesiredElVel = smoothedElVel;

    // Debug output (throttled)
    static int debugCounter = 0;
    if (++debugCounter % 25 == 0) {
        qDebug() << "[RadarSlewMotionMode] Err(Az,El):" << errAz << "," << errEl
                 << "| Vel(Az,El):" << smoothedAzVel << "," << smoothedElVel
                 << "| Dist(Az,El):" << distAz << "," << distEl;
    }

    // Let the base class handle stabilization and sending the final command
    sendStabilizedServoCommands(controller, smoothedAzVel, smoothedElVel, true, dt);
}