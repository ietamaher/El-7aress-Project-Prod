#ifndef TRPSCANMOTIONMODE_H
#define TRPSCANMOTIONMODE_H

#include "gimbalmotionmodebase.h"
#include "models/domain/systemstatemodel.h" // For TargetReferencePoint struct
#include <vector>
#include <QElapsedTimer> // Include for the halt timer

class TRPScanMotionMode : public GimbalMotionModeBase
{
public:
    TRPScanMotionMode();

    // Overridden methods from the base class
    void enterMode(GimbalController* controller) override;
    void exitMode(GimbalController* controller) override;
    void update(GimbalController* controller, double dt) override;

    // This method is called by GimbalController to give us the path
    void setActiveTRPPage(const std::vector<TargetReferencePoint>& trpPage);

private:
    // --- State Machine for managing the path execution ---
    enum class State {
        Idle,       // Not running, path finished, or no path set.
        Moving,     // Moving towards the current waypoint.
        Halted      // Paused at a waypoint for the specified halt time.
    };
    State m_currentState;
    double m_targetAz, m_targetEl; // Current intermediate target for PID

    // --- Path Data & Progress ---
    std::vector<TargetReferencePoint> m_trpPage;
    int m_currentTrpIndex;
    QElapsedTimer m_haltTimer; // Timer to manage the halt duration

    // --- PID Controllers for smooth stopping ---
    PIDController m_azPid;
    PIDController m_elPid;

    // Rate limiting - track previous velocities for time-based acceleration control
    double m_previousDesiredAzVel = 0.0;
    double m_previousDesiredElVel = 0.0;

    // --- Motion Tuning Parameters ---
    // Distance from target to switch from cruising to PID-controlled deceleration.
    static constexpr double DECELERATION_DISTANCE_DEG = 3.0;
    // Position tolerance to consider the gimbal "arrived" at a waypoint.
    static constexpr double ARRIVAL_THRESHOLD_DEG = 0.1;
};

#endif // TRPSCANMOTIONMODE_H
