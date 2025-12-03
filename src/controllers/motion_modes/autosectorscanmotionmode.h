#ifndef AUTOSECTORSCANMOTIONMODE_H
#define AUTOSECTORSCANMOTIONMODE_H

#include "gimbalmotionmodebase.h"
#include "models/domain/systemstatemodel.h" // For AutoSectorScanZone struct

class AutoSectorScanMotionMode : public GimbalMotionModeBase
{
    Q_OBJECT
public:
    explicit AutoSectorScanMotionMode(QObject* parent = nullptr);
    ~AutoSectorScanMotionMode() override = default;

    void enterMode(GimbalController* controller) override;
    void exitMode(GimbalController* controller) override;
    void update(GimbalController* controller, double dt) override;

    // Method for GimbalController to set the active scan parameters
    void setActiveScanZone(const AutoSectorScanZone& scanZone);

private:
    AutoSectorScanZone m_activeScanZone;
    bool m_scanZoneSet;
    bool m_movingToPoint2; // True if current direction is towards point 2, false if towards point 1
    double m_targetAz, m_targetEl; // Current intermediate target for PID
    
    // PID controllers for smooth movement to target points
    // You might reuse or adapt the PID logic from TrackingMotionMode
    // Or implement simpler "move at speed towards target" if PID is overkill for scans
   // struct PIDController { /* ... as in TrackingMotionMode ... */ 
   // void reset(){ integral = 0; previousError = 0;} double Kp = 0.8, Ki=0.05, Kd=0.1, integral=0, previousError=0, maxIntegral=50;};
    PIDController m_azPid;
    PIDController m_elPid;

    // Rate limiting - track previous velocities for time-based acceleration control
    double m_previousDesiredAzVel = 0.0;
    double m_previousDesiredElVel = 0.0;

    static constexpr double ARRIVAL_THRESHOLD_DEG = 0.2; // How close to consider a point "reached"
    static constexpr double DECELERATION_DISTANCE_DEG = 2.0;
};

#endif // AUTOSECTORSCANMOTIONMODE_H
