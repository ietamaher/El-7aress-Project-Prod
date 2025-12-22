#ifndef TRACKINGMOTIONMODE_H
#define TRACKINGMOTIONMODE_H

#include "gimbalmotionmodebase.h"
#include "utils/TimestampLogger.h"

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
    // CROWS-COMPLIANT LAC (Rate-bias feed-forward, NOT position steering)
    // ==========================================================================
    // Per TM 9-1090-225-10-2:
    // "Lead Angle Compensation helps to keep the on screen reticle on target while
    //  the Host Platform, the target, or both are moving at a near constant speed"
    //
    // This is achieved through RATE BIAS (velocity feed-forward), not by steering
    // the gimbal to a predicted intercept point.
    static constexpr double LAC_RATE_BIAS_GAIN = 0.7;   ///< LAC feed-forward gain (0.0-1.0)
    static constexpr double MANUAL_OVERRIDE_SCALE = 5.0; ///< Manual slew scale during tracking (deg/s per unit)
    static constexpr double HALF_SCREEN_LIMIT_DEG = 5.0; ///< Max manual offset from target (half FOV approx)
    double m_manualAzOffset = 0.0;
    double m_manualElOffset = 0.0;

    double m_lastGimbalAz   = 0.0;
    double m_lastGimbalEl   = 0.0;
    double m_gimbalVelAz    = 0.0;
    double m_gimbalVelEl    = 0.0;
        double m_prevAzVel = 0.0;

    double m_prevErrAz = 0.0;   // previous loop error for azimuth (for sign-change check)
double m_prevErrEl = 0.0;   // previous loop error for elevation

double m_imageErrAz = 0.0;
double m_imageErrEl = 0.0;


double m_gimbalAzRate_dps = 0.0;
double m_gimbalElRate_dps = 0.0;

};

#endif // TRACKINGMOTIONMODE_H
