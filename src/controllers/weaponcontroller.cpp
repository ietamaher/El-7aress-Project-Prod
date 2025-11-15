#include "weaponcontroller.h"
#include "models/domain/systemstatemodel.h"
#include "hardware/devices/servoactuatordevice.h"
#include "hardware/devices/plc42device.h"
#include "weaponcontroller.h"
#include <QDebug>

WeaponController::WeaponController(SystemStateModel* m_stateModel,
                                   ServoActuatorDevice* servoActuator,
                                   Plc42Device* plc42,
                                   QObject* parent)
    : QObject(parent),
    m_stateModel(m_stateModel),
    m_servoActuator(servoActuator),
    m_plc42(plc42)
{
    if (m_stateModel) {
        connect(m_stateModel, &SystemStateModel::dataChanged,
                this, &WeaponController::onSystemStateChanged);
    }

    // ========================================================================
    // BALLISTICS: Professional LUT System (Kongsberg/Rafael approach)
    // ========================================================================
    m_ballisticsProcessor = new BallisticsProcessorLUT();

    // Load ammunition table from Qt resources
    bool lutLoaded = m_ballisticsProcessor->loadAmmunitionTable(":/ballistic/tables/m2_ball.json");

    if (lutLoaded) {
        qInfo() << "[WeaponController] Ballistic LUT loaded successfully:"
                << m_ballisticsProcessor->getAmmunitionName();
    } else {
        qCritical() << "[WeaponController] CRITICAL: Failed to load ballistic table!"
                    << "Fire control accuracy will be degraded!";
    }
}

void WeaponController::onSystemStateChanged(const SystemStateData &newData)
{
    // Assume that newData includes an 'ammoLoadRequested' flag indicating the ammo load button press.
    // This is a cleaner trigger than comparing a boolean that represents "ammo loaded".
    /*if (newData.ammoLoaded && m_ammoState == AmmoState::Idle) {
        // Begin the ammo-loading sequence.
        m_ammoState = AmmoState::LoadingFirstCycleForward;
        m_servoActuator->moveToPosition(100);
        qDebug() << "Ammo loading started: moving forward (first cycle)";
    }*/

    if (m_oldState.ammoLoaded != newData.ammoLoaded) {
        // Begin the ammo-loading sequence.
        m_ammoState = AmmoState::Loaded;
        if (newData.ammoLoaded) {
                    m_servoActuator->moveToPosition(63000);
                    qDebug() << "Ammo loading started: moving to extended position";
        } else {
                    m_servoActuator->moveToPosition(2048);
                    qDebug() << "Ammo unloading started: moving to retracted position";
        }
 
    }
    // Update fire readiness based on the dead-man switch.
    if (m_oldState.deadManSwitchActive != newData.deadManSwitchActive) {
        m_fireReady = newData.deadManSwitchActive;
    }

    if (m_oldState.fireMode != newData.fireMode) {

        switch (newData.fireMode) {
            case FireMode::SingleShot:
                m_plc42->setSolenoidMode(1);
                break;
            case FireMode::ShortBurst:
                m_plc42->setSolenoidMode(2);
                break;
            case FireMode::LongBurst:
                m_plc42->setSolenoidMode(3);
                break;
            default:
                m_plc42->setSolenoidMode(1);
                break;
        }
    }

    // Verify that all conditions are met for arming the system.
    if (newData.opMode == OperationalMode::Engagement &&
        newData.gunArmed &&
        m_fireReady )//&&         (m_ammoState == AmmoState::Loaded))
    {
        m_systemArmed = true;
    }
    else {
        m_systemArmed = false;
    }

    // ========================================================================
    // BALLISTICS RECALCULATION TRIGGERS
    // ========================================================================
    // Detect changes in any parameter that affects lead angle calculation
    // ========================================================================
    bool ballisticsInputsChanged = false;

    // LAC activation/deactivation
    if (m_oldState.leadAngleCompensationActive != newData.leadAngleCompensationActive) {
        ballisticsInputsChanged = true;
        qDebug() << "[WeaponController] LAC state changed:"
                 << m_oldState.leadAngleCompensationActive << "→" << newData.leadAngleCompensationActive;
    }

    // Camera switching (Day ↔ Night) - affects which FOV is used
    if (m_oldState.activeCameraIsDay != newData.activeCameraIsDay) {
        ballisticsInputsChanged = true;
        QString oldCam = m_oldState.activeCameraIsDay ? "DAY" : "NIGHT";
        QString newCam = newData.activeCameraIsDay ? "DAY" : "NIGHT";
        float oldFov = m_oldState.activeCameraIsDay ? m_oldState.dayCurrentHFOV : m_oldState.nightCurrentHFOV;
        float newFov = newData.activeCameraIsDay ? newData.dayCurrentHFOV : newData.nightCurrentHFOV;
        qDebug() << "[WeaponController] *** CAMERA SWITCHED:" << oldCam << "→" << newCam
                 << "| Active FOV:" << oldFov << "°" << "→" << newFov << "°";
    }

    // FOV changes (zoom in/out) - affects ZoomOut status
    if (!qFuzzyCompare(m_oldState.dayCurrentHFOV, newData.dayCurrentHFOV) ||
        !qFuzzyCompare(m_oldState.nightCurrentHFOV, newData.nightCurrentHFOV)) {
        ballisticsInputsChanged = true;

        // Determine which camera's FOV actually changed
        if (!qFuzzyCompare(m_oldState.dayCurrentHFOV, newData.dayCurrentHFOV)) {
            qDebug() << "[WeaponController] Day camera ZOOM:"
                     << m_oldState.dayCurrentHFOV << "° →" << newData.dayCurrentHFOV << "°"
                     << (newData.activeCameraIsDay ? "(ACTIVE)" : "(inactive)");
        }
        if (!qFuzzyCompare(m_oldState.nightCurrentHFOV, newData.nightCurrentHFOV)) {
            qDebug() << "[WeaponController] Night camera ZOOM:"
                     << m_oldState.nightCurrentHFOV << "° →" << newData.nightCurrentHFOV << "°"
                     << (!newData.activeCameraIsDay ? "(ACTIVE)" : "(inactive)");
        }
    }

    // Target range changes (LRF measurement)
    if (!qFuzzyCompare(m_oldState.currentTargetRange, newData.currentTargetRange)) {
        ballisticsInputsChanged = true;
        qDebug() << "[WeaponController] Target range changed:"
                 << m_oldState.currentTargetRange << "→" << newData.currentTargetRange << "m";
    }

    // Target angular rates (tracking motion)
    if (!qFuzzyCompare(m_oldState.currentTargetAngularRateAz, newData.currentTargetAngularRateAz) ||
        !qFuzzyCompare(m_oldState.currentTargetAngularRateEl, newData.currentTargetAngularRateEl)) {
        ballisticsInputsChanged = true;
        qDebug() << "[WeaponController] Target angular rates changed: Az:"
                 << m_oldState.currentTargetAngularRateAz << "→" << newData.currentTargetAngularRateAz
                 << "El:" << m_oldState.currentTargetAngularRateEl << "→" << newData.currentTargetAngularRateEl;
    }

    // ========================================================================
    // ENVIRONMENTAL PARAMETERS CHANGE DETECTION
    // ========================================================================
    // If environmental parameters changed and LAC is active, immediately
    // update ballistics processor with new conditions.
    // This ensures user doesn't have to toggle LAC to apply new values.
    // ========================================================================
    bool environmentalParamsChanged = false;

    if (!qFuzzyCompare(m_oldState.environmentalTemperatureCelsius, newData.environmentalTemperatureCelsius) ||
        !qFuzzyCompare(m_oldState.environmentalAltitudeMeters, newData.environmentalAltitudeMeters) ||
        !qFuzzyCompare(m_oldState.environmentalCrosswindMS, newData.environmentalCrosswindMS) ||
        m_oldState.environmentalAppliedToBallistics != newData.environmentalAppliedToBallistics)
    {
        environmentalParamsChanged = true;

        qDebug() << "[WeaponController] Environmental parameters changed:"
                 << "Temp:" << m_oldState.environmentalTemperatureCelsius << "→" << newData.environmentalTemperatureCelsius
                 << "Alt:" << m_oldState.environmentalAltitudeMeters << "→" << newData.environmentalAltitudeMeters
                 << "Wind:" << m_oldState.environmentalCrosswindMS << "→" << newData.environmentalCrosswindMS
                 << "Applied:" << m_oldState.environmentalAppliedToBallistics << "→" << newData.environmentalAppliedToBallistics;
    }

    // ========================================================================
    // TRIGGER BALLISTICS RECALCULATION
    // ========================================================================
    // Recalculate if LAC is active AND any input changed
    // OR if LAC was just activated/deactivated (to set offsets to zero)
    // ========================================================================
    if (ballisticsInputsChanged || environmentalParamsChanged) {
        qDebug() << "[WeaponController] Triggering ballistics recalculation:"
                 << "InputsChanged=" << ballisticsInputsChanged
                 << "EnvChanged=" << environmentalParamsChanged
                 << "LAC=" << newData.leadAngleCompensationActive;
        updateFireControlSolution();
    }

    m_oldState = newData;
}

void WeaponController::onActuatorPositionReached()
{
    // This slot is called whenever the servo reaches a commanded position.
    // We transition the ammo state machine accordingly.
    switch (m_ammoState) {
    case AmmoState::LoadingFirstCycleForward:
        m_ammoState = AmmoState::LoadingFirstCycleBackward;
        m_servoActuator->moveToPosition(2500);
        qDebug() << "Ammo loading: first forward cycle complete, moving backward";
        break;

    case AmmoState::LoadingFirstCycleBackward:
        m_ammoState = AmmoState::LoadingSecondCycleForward;
        m_servoActuator->moveToPosition(50000);
        qDebug() << "Ammo loading: first backward complete, starting second forward cycle";
        break;

    case AmmoState::LoadingSecondCycleForward:
        m_ammoState = AmmoState::LoadingSecondCycleBackward;
        m_servoActuator->moveToPosition(2500);
        qDebug() << "Ammo loading: second forward complete, moving backward final time";
        break;

    case AmmoState::LoadingSecondCycleBackward:
        m_ammoState = AmmoState::Loaded;
        qDebug() << "Ammo loading: sequence complete. Ammo is loaded.";
        // Optionally, notify other system components that ammo is now loaded.
        break;

        // Similarly, you could handle unloading/clearing with a parallel state machine.
    case AmmoState::UnloadingFirstCycleForward:
        m_ammoState = AmmoState::UnloadingFirstCycleBackward;
        m_servoActuator->moveToPosition(2500);
        qDebug() << "Ammo clearing: first forward cycle complete, moving backward";
        break;

    case AmmoState::UnloadingFirstCycleBackward:
        m_ammoState = AmmoState::UnloadingSecondCycleForward;
        m_servoActuator->moveToPosition(50000);
        qDebug() << "Ammo clearing: first backward complete, starting second forward cycle";
        break;

    case AmmoState::UnloadingSecondCycleForward:
        m_ammoState = AmmoState::UnloadingSecondCycleBackward;
        m_servoActuator->moveToPosition(2500);
        qDebug() << "Ammo clearing: second forward complete, moving backward final time";
        break;

    case AmmoState::UnloadingSecondCycleBackward:
        m_ammoState = AmmoState::Cleared;
        qDebug() << "Ammo clearing: sequence complete. Gun is cleared.";
        // Optionally, notify other system components that the gun is cleared.
        break;

        // -------------- Default --------------
    default:
        qDebug() << "Actuator reached position in state" << (int)m_ammoState << ". No action.";
        break;
    }

}

void WeaponController::unloadAmmo()
{
    stopFiring();

    if (m_ammoState == AmmoState::Loaded) {
        m_ammoState = AmmoState::UnloadingFirstCycleForward;
        m_servoActuator->moveToPosition(50000);
        qDebug() << "Unloading ammo: first forward cycle started.";
    } else {
        qDebug() << "Cannot unload: ammo state is not 'Loaded'.";
    }
}


void WeaponController::startFiring()
{
    if (!m_systemArmed) {
        qDebug() << "Cannot fire: system is not armed.";
        return;
    }
    m_plc42->setSolenoidState(1);
}

void WeaponController::stopFiring()
{
    m_plc42->setSolenoidState(0);
    //m_systemArmed = false;

}

void WeaponController::updateFireControlSolution() {
    if (!m_stateModel) return;

    if (!m_stateModel->data().leadAngleCompensationActive) {
        // LAC is off, ensure model reflects zero offsets for reticle
        if (m_stateModel->data().leadAngleOffsetAz != 0.0f ||
            m_stateModel->data().leadAngleOffsetEl != 0.0f ||
            m_stateModel->data().currentLeadAngleStatus != LeadAngleStatus::Off) {
            m_stateModel->updateCalculatedLeadOffsets(0.0f, 0.0f, LeadAngleStatus::Off);
        }
        return; // No lead calculation needed
    }

    // Get necessary inputs (target range, angular rates, FOV, etc.)
    SystemStateData sData = m_stateModel->data();
    float targetRange = sData.currentTargetRange;
    float targetAngRateAz = sData.currentTargetAngularRateAz;
    float targetAngRateEl = sData.currentTargetAngularRateEl;
    // ✅ FIXED (2025-11-14): Use correct FOV based on active camera
    // Day: Sony FCB-EV7520A (2.3° - 63.7° variable zoom)
    // Night: FLIR TAU 2 60mm lens (10° × 8.3° fixed)
    float currentFOV = sData.activeCameraIsDay ? sData.dayCurrentHFOV : sData.nightCurrentHFOV;
    float tofGuess = (targetRange > 0 && sData.muzzleVelocityMPS > 0) ? (targetRange / sData.muzzleVelocityMPS) : 0.0f; // Ensure muzzleVelocity is in SystemStateData

    if (!m_ballisticsProcessor) { // If m_ballisticsProcessor is a pointer
        qCritical() << "BallisticsComputer pointer is NULL!";
        return; // Or handle error
    }

    // ========================================================================
    // UPDATE ENVIRONMENTAL CONDITIONS (if enabled)
    // ========================================================================
    // Apply real-time environmental data from sensors/user input
    // ========================================================================
    if (sData.environmentalAppliedToBallistics) {
        // Update ballistics processor with current environmental conditions
        m_ballisticsProcessor->setEnvironmentalConditions(
            sData.environmentalTemperatureCelsius,
            sData.environmentalAltitudeMeters,
            sData.environmentalCrosswindMS
        );

        qDebug() << "[WeaponController] Environmental corrections ACTIVE:"
                 << "Temp:" << sData.environmentalTemperatureCelsius << "°C"
                 << "Alt:" << sData.environmentalAltitudeMeters << "m"
                 << "Wind:" << sData.environmentalCrosswindMS << "m/s";
    } else {
        // Set to standard conditions when environmental corrections are disabled
        m_ballisticsProcessor->setEnvironmentalConditions(15.0f, 0.0f, 0.0f);
    }

    LeadCalculationResult lead = m_ballisticsProcessor->calculateLeadAngle(
        targetRange, targetAngRateAz, targetAngRateEl,
        sData.muzzleVelocityMPS, // Pass actual muzzle velocity
        tofGuess, currentFOV
        );

    // Update the model with the calculated offsets (these are now for the reticle)
    m_stateModel->updateCalculatedLeadOffsets(lead.leadAzimuthDegrees, lead.leadElevationDegrees, lead.status);

    // NO gimbal offset application here for "Reticle Offset" method.
    // m_gimbalController->setAimpointOffsets(0,0); // Ensure gimbal is not offsetting.
}
