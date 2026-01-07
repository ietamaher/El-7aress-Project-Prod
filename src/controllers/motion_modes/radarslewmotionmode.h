#ifndef RADARSLEWMOTIONMODE_H
#define RADARSLEWMOTIONMODE_H

#include "gimbalmotionmodebase.h"  // This now includes PIDController, etc.
#include <QtGlobal>                // For quint32

class RadarSlewMotionMode : public GimbalMotionModeBase {
    Q_OBJECT

public:
    explicit RadarSlewMotionMode(QObject* parent = nullptr);
    ~RadarSlewMotionMode() override = default;

    void enterMode(GimbalController* controller) override;
    void exitMode(GimbalController* controller) override;

protected:
    /**
     * @brief Radar slew motion mode implementation.
     *
     * Implements slewing to radar-designated targets.
     * Safety checks are handled by base class updateWithSafety().
     *
     * @param controller Pointer to the GimbalController
     * @param dt Time delta in seconds since last update
     */
    void updateImpl(GimbalController* controller, double dt) override;

private:
    // Use the PIDController struct defined in the base class
    PIDController m_azPid;
    PIDController m_elPid;

    // Target state for the current slew operation
    double m_targetAz = 0.0;
    double m_targetEl = 0.0;
    quint32 m_currentTargetId = 0;    // The ID we are currently trying to reach
    bool m_isSlewInProgress = false;  // Flag to indicate if we are actively moving
    // Velocity smoothing variables (similar to TrackingMotionMode)
    double m_previousDesiredAzVel = 0.0;
    double m_previousDesiredElVel = 0.0;
    // Motion parameters
    static constexpr double MAX_SLEW_SPEED_DEGS = 25.0;  // Max speed for slewing to cue
    static constexpr float SYSTEM_HEIGHT_METERS =
        15.0f;  // Example height of the system for El calculation
};

#endif  // RADARSLEWMOTIONMODE_H
