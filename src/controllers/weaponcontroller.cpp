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
        // ✅ LATENCY FIX: Queued connection prevents ballistics calculations from blocking device I/O
        connect(m_stateModel, &SystemStateModel::dataChanged,
                this, &WeaponController::onSystemStateChanged,
                Qt::QueuedConnection);  // Non-blocking signal delivery
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
    // Check both HFOV and VFOV (cameras are NOT square)
    if (!qFuzzyCompare(m_oldState.dayCurrentHFOV, newData.dayCurrentHFOV) ||
        !qFuzzyCompare(m_oldState.dayCurrentVFOV, newData.dayCurrentVFOV) ||
        !qFuzzyCompare(m_oldState.nightCurrentHFOV, newData.nightCurrentHFOV) ||
        !qFuzzyCompare(m_oldState.nightCurrentVFOV, newData.nightCurrentVFOV)) {
        ballisticsInputsChanged = true;

        // Determine which camera's FOV actually changed
        if (!qFuzzyCompare(m_oldState.dayCurrentHFOV, newData.dayCurrentHFOV) ||
            !qFuzzyCompare(m_oldState.dayCurrentVFOV, newData.dayCurrentVFOV)) {
            qDebug() << "[WeaponController] Day camera ZOOM:"
                     << m_oldState.dayCurrentHFOV << "×" << m_oldState.dayCurrentVFOV << "° →"
                     << newData.dayCurrentHFOV << "×" << newData.dayCurrentVFOV << "°"
                     << (newData.activeCameraIsDay ? "(ACTIVE)" : "(inactive)");
        }
        if (!qFuzzyCompare(m_oldState.nightCurrentHFOV, newData.nightCurrentHFOV) ||
            !qFuzzyCompare(m_oldState.nightCurrentVFOV, newData.nightCurrentVFOV)) {
            qDebug() << "[WeaponController] Night camera ZOOM:"
                     << m_oldState.nightCurrentHFOV << "×" << m_oldState.nightCurrentVFOV << "° →"
                     << newData.nightCurrentHFOV << "×" << newData.nightCurrentVFOV << "°"
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
        m_oldState.environmentalAppliedToBallistics != newData.environmentalAppliedToBallistics)
    {
        environmentalParamsChanged = true;

        qDebug() << "[WeaponController] Environmental parameters changed:"
                 << "Temp:" << m_oldState.environmentalTemperatureCelsius << "→" << newData.environmentalTemperatureCelsius
                 << "Alt:" << m_oldState.environmentalAltitudeMeters << "→" << newData.environmentalAltitudeMeters
                 << "Applied:" << m_oldState.environmentalAppliedToBallistics << "→" << newData.environmentalAppliedToBallistics;
    }

    // ========================================================================
    // WINDAGE PARAMETERS CHANGE DETECTION
    // ========================================================================
    // Windage (wind direction + speed) affects crosswind calculation.
    // If windage changes, we need to recalculate ballistics for all directions.
    // ========================================================================
    bool windageParamsChanged = false;

    if (!qFuzzyCompare(m_oldState.windageSpeedKnots, newData.windageSpeedKnots) ||
        !qFuzzyCompare(m_oldState.windageDirectionDegrees, newData.windageDirectionDegrees) ||
        m_oldState.windageAppliedToBallistics != newData.windageAppliedToBallistics)
    {
        windageParamsChanged = true;

        qDebug() << "[WeaponController] Windage parameters changed:"
                 << "Speed:" << m_oldState.windageSpeedKnots << "→" << newData.windageSpeedKnots << "knots"
                 << "Direction:" << m_oldState.windageDirectionDegrees << "→" << newData.windageDirectionDegrees << "°"
                 << "Applied:" << m_oldState.windageAppliedToBallistics << "→" << newData.windageAppliedToBallistics;
    }

    // ========================================================================
    // ABSOLUTE BEARING CHANGE DETECTION (Critical for Windage!)
    // ========================================================================
    // When absolute gimbal bearing changes, the crosswind component changes even if
    // wind conditions remain constant. This is because crosswind = wind × sin(angle).
    // Absolute bearing = IMU yaw (vehicle heading) + Station azimuth
    // Changes when EITHER:
    //   1. Station azimuth changes (gimbal rotates relative to vehicle)
    //   2. IMU yaw changes (vehicle rotates)
    // Example: Wind from North at 10 m/s
    //   - Firing North: crosswind = 0 m/s
    //   - Firing East: crosswind = 10 m/s
    // We only recalculate if windage is active (otherwise no point).
    // NOTE: azimuthDirection is uint16_t, use direct comparison not qFuzzyCompare
    // ========================================================================
    bool absoluteBearingChanged = false;

    // Check station azimuth change
    if (m_oldState.azimuthDirection != newData.azimuthDirection) {
        // Only flag as change if windage is actually applied
        if (newData.windageAppliedToBallistics && newData.windageSpeedKnots > 0.001f) {
            absoluteBearingChanged = true;
            ballisticsInputsChanged = true;

            // Debug output commented to prevent app freeze (runs every gimbal move)
            /*qDebug() << "[WeaponController] *** STATION AZIMUTH CHANGED (windage active):"
                     << m_oldState.azimuthDirection << "°" << "→" << newData.azimuthDirection << "°"
                     << "| Crosswind will be recalculated for new gimbal position";*/
        }
    }

    // Check IMU yaw change (vehicle rotation)
    if (!qFuzzyCompare(m_oldState.imuYawDeg, newData.imuYawDeg)) {
        // Only flag as change if windage is actually applied
        if (newData.windageAppliedToBallistics && newData.windageSpeedKnots > 0.001f) {
            absoluteBearingChanged = true;
            ballisticsInputsChanged = true;

            // Debug output commented to prevent app freeze (runs every vehicle rotation)
            /*qDebug() << "[WeaponController] *** VEHICLE HEADING CHANGED (windage active):"
                     << m_oldState.imuYawDeg << "°" << "→" << newData.imuYawDeg << "°"
                     << "| Crosswind will be recalculated for new vehicle orientation";*/
        }
    }

    // ========================================================================
    // TRIGGER BALLISTICS RECALCULATION
    // ========================================================================
    // Recalculate if LAC is active AND any input changed
    // OR if LAC was just activated/deactivated (to set offsets to zero)
    // OR if windage parameters changed (affects crosswind calculation)
    // OR if absolute bearing changed with windage active (crosswind component changes)
    // ========================================================================
    if (ballisticsInputsChanged || environmentalParamsChanged || windageParamsChanged) {
        // Debug output commented to prevent app freeze (runs frequently)
        /*qDebug() << "[WeaponController] Triggering ballistics recalculation:"
                 << "InputsChanged=" << ballisticsInputsChanged
                 << "EnvChanged=" << environmentalParamsChanged
                 << "WindageChanged=" << windageParamsChanged
                 << "AbsoluteBearingChanged=" << absoluteBearingChanged
                 << "LAC=" << newData.leadAngleCompensationActive;*/
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

    // Get system state data first (needed for both windage and LAC)
    SystemStateData sData = m_stateModel->data();

    // ========================================================================
    // CALCULATE CROSSWIND FROM WINDAGE (Direction + Speed)
    // ========================================================================
    // ⚠️ CRITICAL: This calculation must run INDEPENDENTLY of LAC status!
    // Windage provides absolute wind direction and speed (relative to true North).
    // Crosswind component varies with absolute gimbal bearing (where weapon points).
    // Absolute gimbal bearing = Vehicle heading (IMU yaw) + Station azimuth
    // This ensures correct wind correction for any firing direction.
    // The crosswind value is used by:
    //   1. Ballistics calculations (when environmental corrections enabled)
    //   2. OSD display (always shown when windage is set)
    // ========================================================================
    float currentCrosswind = 0.0f;
    if (sData.windageAppliedToBallistics && sData.windageSpeedKnots > 0.001f) {
        // Convert wind speed from knots to m/s
        float windSpeedMS = sData.windageSpeedKnots * 0.514444f;  // 1 knot = 0.514444 m/s

        // Calculate absolute gimbal bearing (true bearing where weapon is pointing)
        // Vehicle heading (IMU yaw) + Station azimuth (relative to vehicle)
        float absoluteGimbalBearing = static_cast<float>(sData.imuYawDeg) + sData.azimuthDirection;

        // Normalize to 0-360 range
        while (absoluteGimbalBearing >= 360.0f) absoluteGimbalBearing -= 360.0f;
        while (absoluteGimbalBearing < 0.0f) absoluteGimbalBearing += 360.0f;

        // Calculate crosswind component based on absolute gimbal bearing
        currentCrosswind = calculateCrosswindComponent(
            windSpeedMS,
            sData.windageDirectionDegrees,
            absoluteGimbalBearing  // Absolute gimbal bearing (IMU yaw + station azimuth)
        );

        // Store calculated crosswind in state data for OSD display
        if (std::abs(sData.calculatedCrosswindMS - currentCrosswind) > 0.01f) {
            SystemStateData updatedData = sData;
            updatedData.calculatedCrosswindMS = currentCrosswind;
            m_stateModel->updateData(updatedData);
            sData = updatedData;  // Update local copy
        }

        // Debug output commented to prevent app freeze (runs every frame)
        /*qDebug() << "[WeaponController] WINDAGE TO CROSSWIND CONVERSION:"
                 << "Wind Dir:" << sData.windageDirectionDegrees << "° (FROM)"
                 << "| Wind Speed:" << sData.windageSpeedKnots << "knots (" << windSpeedMS << "m/s)"
                 << "| Vehicle Heading (IMU):" << sData.imuYawDeg << "°"
                 << "| Station Az:" << sData.azimuthDirection << "°"
                 << "| Absolute Gimbal Bearing:" << absoluteGimbalBearing << "°"
                 << "| Crosswind Component:" << currentCrosswind << "m/s"
                 << (currentCrosswind > 0 ? "(right deflection)" : currentCrosswind < 0 ? "(left deflection)" : "(no crosswind)");*/
    } else {
        // Clear crosswind when windage is not applied
        if (std::abs(sData.calculatedCrosswindMS) > 0.01f) {
            SystemStateData updatedData = sData;
            updatedData.calculatedCrosswindMS = 0.0f;
            m_stateModel->updateData(updatedData);
            sData = updatedData;  // Update local copy
        }
    }

    // ========================================================================
    // PROFESSIONAL FCS: SPLIT BALLISTIC DROP AND MOTION LEAD
    // ========================================================================
    // Drop compensation:  Auto-applied when LRF range valid (like Kongsberg/Rafael)
    // Motion lead:        Only when LAC toggle active
    // ========================================================================

    if (!m_ballisticsProcessor) {
        qCritical() << "[WeaponController] BallisticsComputer pointer is NULL!";
        return;
    }

    // Get common inputs
    float targetRange = sData.currentTargetRange;
    float currentHFOV = sData.activeCameraIsDay ? sData.dayCurrentHFOV : sData.nightCurrentHFOV;
    float currentVFOV = sData.activeCameraIsDay ? sData.dayCurrentVFOV : sData.nightCurrentVFOV;

    // ========================================================================
    // STEP 1: UPDATE ENVIRONMENTAL CONDITIONS (affects both drop and lead)
    // ========================================================================
    if (sData.environmentalAppliedToBallistics) {
        m_ballisticsProcessor->setEnvironmentalConditions(
            sData.environmentalTemperatureCelsius,
            sData.environmentalAltitudeMeters,
            currentCrosswind
        );
    } else {
        m_ballisticsProcessor->setEnvironmentalConditions(
            15.0f, 0.0f, currentCrosswind  // Standard + wind
        );
    }

    // ========================================================================
    // STEP 2: BALLISTIC DROP - AUTO-APPLIED WHEN RANGE VALID (Professional FCS)
    // ========================================================================
    bool applyDrop = (targetRange > 0.1f);  // Valid range threshold

    if (applyDrop) {
        // Calculate ballistic drop (gravity + wind)
        LeadCalculationResult drop = m_ballisticsProcessor->calculateBallisticDrop(targetRange);

        // Update state with drop offsets
        SystemStateData updatedData = sData;
        updatedData.ballisticDropOffsetAz = drop.leadAzimuthDegrees;
        updatedData.ballisticDropOffsetEl = drop.leadElevationDegrees;
        updatedData.ballisticDropActive = true;
        m_stateModel->updateData(updatedData);
        sData = updatedData;  // Update local copy

        qDebug() << "[WeaponController] 🎯 AUTO DROP APPLIED:"
                 << "Range:" << targetRange << "m"
                 << "| Drop Az:" << drop.leadAzimuthDegrees << "° (wind)"
                 << "| Drop El:" << drop.leadElevationDegrees << "° (gravity)";
    } else {
        // Clear drop when no valid range
        if (sData.ballisticDropActive) {
            SystemStateData updatedData = sData;
            updatedData.ballisticDropOffsetAz = 0.0f;
            updatedData.ballisticDropOffsetEl = 0.0f;
            updatedData.ballisticDropActive = false;
            m_stateModel->updateData(updatedData);
            sData = updatedData;

            qDebug() << "[WeaponController] DROP CLEARED (no valid range)";
        }
    }

    // ========================================================================
    // STEP 3: MOTION LEAD - ONLY WHEN LAC TOGGLE ACTIVE (Professional FCS)
    // ========================================================================
    if (!sData.leadAngleCompensationActive) {
        // LAC toggle is OFF - clear motion lead
        if (sData.motionLeadOffsetAz != 0.0f || sData.motionLeadOffsetEl != 0.0f ||
            sData.currentLeadAngleStatus != LeadAngleStatus::Off) {

            SystemStateData updatedData = sData;
            updatedData.motionLeadOffsetAz = 0.0f;
            updatedData.motionLeadOffsetEl = 0.0f;
            updatedData.currentLeadAngleStatus = LeadAngleStatus::Off;
            // Update deprecated fields for backward compatibility
            updatedData.leadAngleOffsetAz = 0.0f;
            updatedData.leadAngleOffsetEl = 0.0f;
            m_stateModel->updateData(updatedData);

            qDebug() << "[WeaponController] MOTION LEAD CLEARED (LAC off)";
        }
        return;  // Exit - drop already applied above if range valid
    }

    // LAC is ACTIVE - calculate motion lead for moving target
    float targetAngRateAz = sData.currentTargetAngularRateAz;
    float targetAngRateEl = sData.currentTargetAngularRateEl;

    LeadCalculationResult lead = m_ballisticsProcessor->calculateMotionLead(
        targetRange,
        targetAngRateAz,
        targetAngRateEl,
        currentHFOV,
        currentVFOV
    );

    // Update state with motion lead
    SystemStateData updatedData = sData;
    updatedData.motionLeadOffsetAz = lead.leadAzimuthDegrees;
    updatedData.motionLeadOffsetEl = lead.leadElevationDegrees;
    updatedData.currentLeadAngleStatus = lead.status;
    // Update deprecated fields for backward compatibility
    updatedData.leadAngleOffsetAz = lead.leadAzimuthDegrees;
    updatedData.leadAngleOffsetEl = lead.leadElevationDegrees;
    m_stateModel->updateData(updatedData);

    qDebug() << "[WeaponController] 🎯 MOTION LEAD:"
             << "Az:" << lead.leadAzimuthDegrees << "°"
             << "| El:" << lead.leadElevationDegrees << "°"
             << "| Status:" << static_cast<int>(lead.status)
             << (lead.status == LeadAngleStatus::On ? "(On)" :
                 lead.status == LeadAngleStatus::Lag ? "(Lag)" :
                 lead.status == LeadAngleStatus::ZoomOut ? "(ZoomOut)" : "(Unknown)");
}

// ============================================================================
// CROSSWIND CALCULATION FROM WINDAGE
// ============================================================================

float WeaponController::calculateCrosswindComponent(float windSpeedMS,
                                                      float windDirectionDeg,
                                                      float gimbalAzimuthDeg)
{
    // ========================================================================
    // BALLISTICS PHYSICS: Crosswind Component Calculation
    // ========================================================================
    // Wind direction: Direction wind is coming FROM (meteorological convention)
    // Gimbal azimuth: Direction weapon is pointing TO
    //
    // Relative angle = wind_direction - firing_direction
    // Crosswind = wind_speed × sin(relative_angle)
    //
    // Examples:
    //   Wind from 0° (North), firing at 0° (North):
    //     relative = 0° → sin(0°) = 0 → no crosswind (headwind)
    //
    //   Wind from 0° (North), firing at 90° (East):
    //     relative = -90° → sin(-90°) = -1 → full crosswind (left deflection)
    //
    //   Wind from 0° (North), firing at 180° (South):
    //     relative = -180° → sin(-180°) = 0 → no crosswind (tailwind)
    //
    //   Wind from 0° (North), firing at 270° (West):
    //     relative = -270° = +90° → sin(90°) = +1 → full crosswind (right deflection)
    // ========================================================================

    // Calculate relative angle between wind and firing direction
    float relativeAngle = windDirectionDeg - gimbalAzimuthDeg;

    // Normalize to [-180, +180] for correct trigonometry
    while (relativeAngle > 180.0f) {
        relativeAngle -= 360.0f;
    }
    while (relativeAngle < -180.0f) {
        relativeAngle += 360.0f;
    }

    // Calculate crosswind component
    // sin(90°) = 1.0 → full crosswind (wind perpendicular to fire direction)
    // sin(0°) = 0.0 → no crosswind (headwind or tailwind)
    float crosswindMS = windSpeedMS * std::sin(relativeAngle * M_PI / 180.0f);

    return crosswindMS;
}
