#ifndef TRACKINGMOTIONMODE_H
#define TRACKINGMOTIONMODE_H

#include "gimbalmotionmodebase.h"
#include "utils/TimestampLogger.h"

// =============================================================================
// RIGID CRADLE LAC STATE MACHINE
// =============================================================================
// Physical constraint: Camera and Gun are rigidly mounted (no independent motion)
//
// Consequence: During lead angle injection, the target MUST drift off-center
// because the gimbal is aiming ahead of the target's current position.
//
// State Machine:
//   TRACK      - Visual tracking active, target centered (lacBlendFactor = 0.0)
//   FIRE_LEAD  - Lead injection active, target drifts (lacBlendFactor → 1.0)
//   RECENTER   - Fire stopped, transitioning back to tracking (lacBlendFactor → 0.0)
// =============================================================================
enum class TrackingState {
    TRACK,       ///< Normal tracking - P+D loop keeps target centered
    FIRE_LEAD,   ///< LAC active - open-loop lead injection, target drifts off-center
    RECENTER     ///< Transitioning back to tracking after fire
};

class TrackingMotionMode : public GimbalMotionModeBase
{
    Q_OBJECT

public:
    explicit TrackingMotionMode(QObject* parent = nullptr);

    void enterMode(GimbalController* controller) override;
    void exitMode(GimbalController* controller) override;
    void update(GimbalController* controller, double dt) override;

public slots:
    void onTargetPositionUpdated(double az, double el,
                               double velocityAz_dps, double velocityEl_dps,
                               bool isValid);

private:
    // Helper functions for improved control
    double applyRateLimit(double newVelocity, double previousVelocity, double maxChange);
    double applyVelocityScaling(double velocity, double error);

    // Target state
    bool m_targetValid;
    double m_targetAz, m_targetEl;
    double m_targetAzVel_dps, m_targetElVel_dps;

    // Smoothed values
    double m_smoothedTargetAz, m_smoothedTargetEl;
    double m_smoothedAzVel_dps, m_smoothedElVel_dps;

    // Rate limiting
    double m_previousDesiredAzVel, m_previousDesiredElVel;

    // PID controllers
    PIDController m_azPid, m_elPid;

    QElapsedTimer m_velocityTimer; // To measure time between frames

    // ==========================================================================
    // RIGID CRADLE LAC STATE MACHINE (Physical constraint: camera+gun locked)
    // ==========================================================================
    // Per TM 9-1090-225-10-2 (CROWS):
    // "Lead Angle Compensation helps to keep the on screen reticle on target while
    //  the Host Platform, the target, or both are moving at a near constant speed"
    //
    // For RIGID CRADLE systems (camera+gun physically locked):
    // - During TRACK: P+D loop keeps target centered
    // - During FIRE_LEAD: Open-loop lead injection, target intentionally drifts
    // - Blend factor provides smooth 200ms transition to avoid servo jerk
    // ==========================================================================

    /// Current tracking state machine state
    TrackingState m_state = TrackingState::TRACK;

    /// Blend factor: 0.0 = pure tracking, 1.0 = pure lead injection
    /// Ramps IN at 5.0/sec (200ms) and OUT at 3.0/sec (333ms)
    double m_lacBlendFactor = 0.0;

    /// LAC rate bias gain (how much of the latched rate to inject)
    static constexpr double LAC_RATE_BIAS_GAIN = 1.0;   ///< Full rate injection (1.0 = 100%)

    /// Blend ramp rates (per second)
    static constexpr double LAC_BLEND_RAMP_IN  = 5.0;   ///< 200ms ramp to full lead
    static constexpr double LAC_BLEND_RAMP_OUT = 3.0;   ///< 333ms ramp back to tracking

    // Manual nudge (position offset during tracking)
    static constexpr double MANUAL_OVERRIDE_SCALE = 5.0; ///< Manual slew scale (deg/s per unit)
    static constexpr double HALF_SCREEN_LIMIT_DEG = 5.0; ///< Max manual offset from target

    // Gimbal position tracking
    double m_lastGimbalAz   = 0.0;
    double m_lastGimbalEl   = 0.0;
    double m_gimbalVelAz    = 0.0;
    double m_gimbalVelEl    = 0.0;
    double m_prevAzVel      = 0.0;

    // Tracking error (image-based)
    double m_prevErrAz = 0.0;   // previous loop error for azimuth
    double m_prevErrEl = 0.0;   // previous loop error for elevation
    double m_imageErrAz = 0.0;  // current image error azimuth
    double m_imageErrEl = 0.0;  // current image error elevation

    // Filtered derivative-on-error (noise rejection for D-term)
    double m_filteredDErrAz = 0.0;  // low-pass filtered dErr azimuth
    double m_filteredDErrEl = 0.0;  // low-pass filtered dErr elevation

    // Manual nudge offsets (degrees)
    double m_manualAzOffset_deg = 0.0;
    double m_manualElOffset_deg = 0.0;

    // Rate estimation
    double m_gimbalAzRate_dps = 0.0;
    double m_gimbalElRate_dps = 0.0;

    // Debug/logging
    int m_logCycleCounter = 0;
    double m_lastValidAzRate = 0.0;
    double m_lastValidElRate = 0.0;

};

#endif // TRACKINGMOTIONMODE_H