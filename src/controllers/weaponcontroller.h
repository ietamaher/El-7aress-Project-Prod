#ifndef WEAPONCONTROLLER_H
#define WEAPONCONTROLLER_H

#include <QObject>


class Plc42Device;
class ServoActuatorDevice;
class SystemStateModel;

#include "models/domain/systemstatemodel.h"
#include "../utils/ballisticsprocessorlut.h"

enum class AmmoState {
    Idle,
    LoadingFirstCycleForward,
    LoadingFirstCycleBackward,
    LoadingSecondCycleForward,
    LoadingSecondCycleBackward,
    UnloadingFirstCycleForward,
    UnloadingFirstCycleBackward,
    UnloadingSecondCycleForward,
    UnloadingSecondCycleBackward,
    Loaded,
    Cleared
};

class WeaponController : public QObject
{
    Q_OBJECT
public:
    explicit WeaponController( SystemStateModel* m_stateModel,
                                   ServoActuatorDevice* servoActuator,
                                   Plc42Device* plc42,
                                   QObject* parent = nullptr);

    void armWeapon(bool enable);
    void fireSingleShot();

    virtual void startFiring();
    virtual void stopFiring();
    virtual void unloadAmmo();

    virtual void updateFireControlSolution();
signals:
    void weaponArmed(bool armed);
    void weaponFired();

private slots:
    //void onPlc21DataChanged(const Plc21PanelData &data);

    void onSystemStateChanged(const SystemStateData &newData);
    void onActuatorPositionReached();

private:
    /**
     * @brief Calculate crosswind component from wind direction and gimbal azimuth
     *
     * This function converts absolute wind (direction + speed) into the crosswind
     * component perpendicular to the firing direction. The crosswind varies with
     * gimbal azimuth - same wind conditions produce different crosswind based on
     * where the weapon is pointing.
     *
     * @param windSpeedMS Wind speed in meters per second
     * @param windDirectionDeg Wind direction in degrees (where wind is FROM)
     * @param gimbalAzimuthDeg Current gimbal azimuth (where weapon is pointing TO)
     * @return Crosswind component in m/s (positive = right deflection, negative = left)
     *
     * Example:
     *   Wind from North (0°), firing North (0°) → crosswind = 0 m/s (headwind)
     *   Wind from North (0°), firing East (90°) → crosswind = +wind_speed (full crosswind right)
     *   Wind from North (0°), firing South (180°) → crosswind = 0 m/s (tailwind)
     *   Wind from North (0°), firing West (270°) → crosswind = -wind_speed (full crosswind left)
     */
    float calculateCrosswindComponent(float windSpeedMS,
                                       float windDirectionDeg,
                                       float gimbalAzimuthDeg);

    SystemStateModel*  m_stateModel = nullptr;
    Plc42Device* m_plc42 = nullptr;
    ServoActuatorDevice* m_servoActuator = nullptr;
    SystemStateData m_oldState;
    BallisticsProcessorLUT* m_ballisticsProcessor = nullptr;


    bool m_weaponArmed = false;
    bool m_systemArmed = false;
    bool m_fireReady   = false;

    int m_ammoFlag = 0;
AmmoState m_ammoState { AmmoState::Idle };

};


#endif // WEAPONCONTROLLER_H
