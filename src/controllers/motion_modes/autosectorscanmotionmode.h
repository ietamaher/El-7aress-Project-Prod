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

    // GimbalMotionModeBase interface
    void enterMode(GimbalController* controller) override;
    void exitMode(GimbalController* controller) override;
    void update(GimbalController* controller, double dt) override;
    //MotionMode::Type type() const override { return MotionMode::AutoSectorScan; }

    // Configuration
    void setActiveScanZone(const AutoSectorScanZone& scanZone);

private:
    AutoSectorScanZone m_activeScanZone;
    bool m_scanZoneSet;
    
    // State variables
    bool m_movingToPoint2;       // true = moving to az2, false = moving to az1
    double m_targetAz;           // Current target angle
    double m_previousDesiredAzVel;
    
    // Fix for "Short Range": Timer to hold at endpoint
    double m_timeAtTarget;

    // PIDs (Used for config loading, though logic is profile-based)
    PIDController m_azPid;
    PIDController m_elPid;
};

#endif // AUTOSECTORSCANMOTIONMODE_H