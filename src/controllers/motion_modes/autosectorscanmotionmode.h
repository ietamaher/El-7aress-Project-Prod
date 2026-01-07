#ifndef AUTOSECTORSCANMOTIONMODE_H
#define AUTOSECTORSCANMOTIONMODE_H

#include "gimbalmotionmodebase.h"
#include "models/domain/systemstatemodel.h"

class AutoSectorScanMotionMode : public GimbalMotionModeBase {
    Q_OBJECT

public:
    explicit AutoSectorScanMotionMode(QObject* parent = nullptr);
    ~AutoSectorScanMotionMode() override = default;

    void enterMode(GimbalController* controller) override;
    void exitMode(GimbalController* controller) override;

    void setActiveScanZone(const AutoSectorScanZone& scanZone);

protected:
    /**
     * @brief Auto sector scan motion mode implementation.
     *
     * Implements automatic sector scanning between defined waypoints.
     * Safety checks are handled by base class updateWithSafety().
     *
     * @param controller Pointer to the GimbalController
     * @param dt Time delta in seconds since last update
     */
    void updateImpl(GimbalController* controller, double dt) override;

private:
    enum ScanState { AlignElevation, ScanAzimuth };

    ScanState m_state = AlignElevation;

    AutoSectorScanZone m_activeScanZone;
    bool m_scanZoneSet = false;

    // Targets
    double m_targetAz = 0.0;
    double m_targetEl = 0.0;

    // Tracking state
    bool m_movingToPoint2 = true;
    double m_previousDesiredAzVel = 0.0;
    double m_timeAtTarget = 0.0;

    // PID (optional, kept for future tuning)
    PIDController m_azPid;
    PIDController m_elPid;

    // Motor direction sign (central inversion)
    int m_azHardwareSign = +1;
};

#endif  // AUTOSECTORSCANMOTIONMODE_H
