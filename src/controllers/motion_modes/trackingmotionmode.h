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

};

#endif // TRACKINGMOTIONMODE_H
