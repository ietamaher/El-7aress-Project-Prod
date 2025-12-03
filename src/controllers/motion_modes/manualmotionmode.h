#ifndef MANUALMOTIONMODE_H
#define MANUALMOTIONMODE_H

#include "gimbalmotionmodebase.h"

class ManualMotionMode : public GimbalMotionModeBase
{
    Q_OBJECT
public:
    explicit ManualMotionMode(QObject* parent = nullptr);

    void enterMode(GimbalController* controller) override;
    void exitMode(GimbalController* controller) override;
    void update(GimbalController* controller, double dt) override;
    double processJoystickInput(double filteredInput);


private:
    double m_currentAzVelocityCmd = 0.0;
    double m_currentElVelocityCmd = 0.0;

    // --- Tuning Parameter for smoothness ---
    // Max acceleration in degrees per second squared.
    // This value controls how quickly the gimbal ramps up to speed.
    // A lower value gives a smoother, gentler response.
    // A higher value gives a more responsive, "tighter" feel.
    static constexpr double MAX_MANUAL_ACCEL_DEGS2 = 100.0; // Tune this value!
    static constexpr float SPEED_MULTIPLIER = 1.0f;
    // NOTE: UPDATE_INTERVAL_S() is now a function in base class (runtime-configurable)
    
    // Command tracking variables
    double m_currentAzSpeedCmd_Hz = 0.0;
    double m_currentElSpeedCmd_Hz = 0.0;
    
    // Filter state variables
    double m_filteredAzJoystick = 0.0;
    double m_filteredElJoystick = 0.0;
 
};

#endif // MANUALMOTIONMODE_H