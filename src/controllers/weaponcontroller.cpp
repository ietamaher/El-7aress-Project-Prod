#include "weaponcontroller.h"

// ============================================================================
// Project
// ============================================================================
#include "models/domain/systemstatemodel.h"
#include "hardware/devices/servoactuatordevice.h"
#include "hardware/devices/plc42device.h"

// ============================================================================
// Qt Framework
// ============================================================================
#include <QDebug>

// ============================================================================
// Standard Library
// ============================================================================
#include <cmath>

// ============================================================================
// CONSTRUCTOR & DESTRUCTOR
// ============================================================================

WeaponController::WeaponController(SystemStateModel* stateModel,
                                   ServoActuatorDevice* servoActuator,
                                   Plc42Device* plc42,
                                   QObject* parent)
    : QObject(parent)
    , m_stateModel(stateModel)
    , m_servoActuator(servoActuator)
    , m_plc42(plc42)
{
    // ========================================================================
    // SYSTEM STATE MONITORING
    // ========================================================================
    if (m_stateModel) {
        // LATENCY FIX: Queued connection prevents ballistics calculations
        // from blocking device I/O thread
        connect(m_stateModel, &SystemStateModel::dataChanged,
                this, &WeaponController::onSystemStateChanged,
                Qt::QueuedConnection);

        // Initialize GUI state for ammo feed
        m_stateModel->setAmmoFeedCycleInProgress(false);
    }

    // ========================================================================
    // AMMO FEED FSM SETUP
    // ========================================================================
    m_feedTimer = new QTimer(this);
    m_feedTimer->setSingleShot(true);
    connect(m_feedTimer, &QTimer::timeout, this, &WeaponController::onFeedTimeout);

    // Connect actuator position feedback for FSM state transitions
    if (m_servoActuator) {
        connect(m_servoActuator, &ServoActuatorDevice::actuatorDataChanged,
                this, &WeaponController::onActuatorFeedback,
                Qt::QueuedConnection);
    }

    // ========================================================================
    // BALLISTICS: Professional LUT System (Kongsberg/Rafael approach)
    // ========================================================================
    m_ballisticsProcessor = new BallisticsProcessorLUT();

    bool lutLoaded = m_ballisticsProcessor->loadAmmunitionTable(":/ballistic/tables/m2_ball.json");

    if (lutLoaded) {
        qInfo() << "[WeaponController] Ballistic LUT loaded successfully:"
                << m_ballisticsProcessor->getAmmunitionName();
    } else {
        qCritical() << "[WeaponController] CRITICAL: Failed to load ballistic table!"
                    << "Fire control accuracy will be degraded!";
    }
}

WeaponController::~WeaponController()
{
    // Stop any active timers
    if (m_feedTimer) {
        m_feedTimer->stop();
    }

    // Clean up ballistics processor
    delete m_ballisticsProcessor;
    m_ballisticsProcessor = nullptr;
}

// ============================================================================
// STATE CHANGE HANDLER
// ============================================================================

void WeaponController::onSystemStateChanged(const SystemStateData& newData)
{
    // ========================================================================
    // AMMO FEED CYCLE - NEW FSM-BASED APPROACH
    // Detect button PRESS (edge trigger, not level)
    // Button RELEASE is intentionally ignored - FSM controls the cycle
    // ========================================================================
    if (newData.ammoLoadButtonPressed && !m_oldState.ammoLoadButtonPressed) {
        // Button was just pressed - try to start cycle
        onOperatorRequestLoad();
    }

    // If button released while cycle running - log but ignore
    if (!newData.ammoLoadButtonPressed && m_oldState.ammoLoadButtonPressed) {
        if (m_feedState != AmmoFeedState::Idle && m_feedState != AmmoFeedState::Fault) {
            qDebug() << "[WeaponController] Operator released load button early - ignored (cycle in progress)";
        }
    }

    // ========================================================================
    // DEAD MAN SWITCH
    // ========================================================================
    if (m_oldState.deadManSwitchActive != newData.deadManSwitchActive) {
        m_fireReady = newData.deadManSwitchActive;
    }

    // ========================================================================
    // FIRE MODE SELECTION
    // ========================================================================
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

    // ========================================================================
    // SYSTEM ARMING CONDITIONS
    // ========================================================================
    m_systemArmed = (newData.opMode == OperationalMode::Engagement &&
                     newData.gunArmed &&
                     m_fireReady);

    // ========================================================================
    // BALLISTICS RECALCULATION TRIGGERS
    // Detect changes in any parameter that affects lead angle calculation
    // ========================================================================
    bool ballisticsInputsChanged = false;

    // LAC activation/deactivation
    if (m_oldState.leadAngleCompensationActive != newData.leadAngleCompensationActive) {
        ballisticsInputsChanged = true;
        qDebug() << "[WeaponController] LAC state changed:"
                 << m_oldState.leadAngleCompensationActive << "->"
                 << newData.leadAngleCompensationActive;
    }

    // Camera switching (Day <-> Night) - affects which FOV is used
    if (m_oldState.activeCameraIsDay != newData.activeCameraIsDay) {
        ballisticsInputsChanged = true;
        QString oldCam = m_oldState.activeCameraIsDay ? "DAY" : "NIGHT";
        QString newCam = newData.activeCameraIsDay ? "DAY" : "NIGHT";
        float oldFov = m_oldState.activeCameraIsDay ? m_oldState.dayCurrentHFOV : m_oldState.nightCurrentHFOV;
        float newFov = newData.activeCameraIsDay ? newData.dayCurrentHFOV : newData.nightCurrentHFOV;
        qDebug() << "[WeaponController] CAMERA SWITCHED:" << oldCam << "->" << newCam
                 << "| Active FOV:" << oldFov << "deg ->" << newFov << "deg";
    }

    // FOV changes (zoom in/out)
    if (!qFuzzyCompare(m_oldState.dayCurrentHFOV, newData.dayCurrentHFOV) ||
        !qFuzzyCompare(m_oldState.dayCurrentVFOV, newData.dayCurrentVFOV) ||
        !qFuzzyCompare(m_oldState.nightCurrentHFOV, newData.nightCurrentHFOV) ||
        !qFuzzyCompare(m_oldState.nightCurrentVFOV, newData.nightCurrentVFOV)) {
        ballisticsInputsChanged = true;

        if (!qFuzzyCompare(m_oldState.dayCurrentHFOV, newData.dayCurrentHFOV) ||
            !qFuzzyCompare(m_oldState.dayCurrentVFOV, newData.dayCurrentVFOV)) {
            qDebug() << "[WeaponController] Day camera ZOOM:"
                     << m_oldState.dayCurrentHFOV << "x" << m_oldState.dayCurrentVFOV << "deg ->"
                     << newData.dayCurrentHFOV << "x" << newData.dayCurrentVFOV << "deg"
                     << (newData.activeCameraIsDay ? "(ACTIVE)" : "(inactive)");
        }
        if (!qFuzzyCompare(m_oldState.nightCurrentHFOV, newData.nightCurrentHFOV) ||
            !qFuzzyCompare(m_oldState.nightCurrentVFOV, newData.nightCurrentVFOV)) {
            qDebug() << "[WeaponController] Night camera ZOOM:"
                     << m_oldState.nightCurrentHFOV << "x" << m_oldState.nightCurrentVFOV << "deg ->"
                     << newData.nightCurrentHFOV << "x" << newData.nightCurrentVFOV << "deg"
                     << (!newData.activeCameraIsDay ? "(ACTIVE)" : "(inactive)");
        }
    }

    // Target range changes (LRF measurement)
    if (!qFuzzyCompare(m_oldState.currentTargetRange, newData.currentTargetRange)) {
        ballisticsInputsChanged = true;
        qDebug() << "[WeaponController] Target range changed:"
                 << m_oldState.currentTargetRange << "->" << newData.currentTargetRange << "m";
    }

    // Target angular rates (tracking motion)
    if (!qFuzzyCompare(m_oldState.currentTargetAngularRateAz, newData.currentTargetAngularRateAz) ||
        !qFuzzyCompare(m_oldState.currentTargetAngularRateEl, newData.currentTargetAngularRateEl)) {
        ballisticsInputsChanged = true;
        qDebug() << "[WeaponController] Target angular rates changed: Az:"
                 << m_oldState.currentTargetAngularRateAz << "->" << newData.currentTargetAngularRateAz
                 << "El:" << m_oldState.currentTargetAngularRateEl << "->" << newData.currentTargetAngularRateEl;
    }

    // ========================================================================
    // ENVIRONMENTAL PARAMETERS CHANGE DETECTION
    // If environmental parameters changed and LAC is active, immediately
    // update ballistics processor with new conditions.
    // ========================================================================
    bool environmentalParamsChanged = false;

    if (!qFuzzyCompare(m_oldState.environmentalTemperatureCelsius, newData.environmentalTemperatureCelsius) ||
        !qFuzzyCompare(m_oldState.environmentalAltitudeMeters, newData.environmentalAltitudeMeters) ||
        m_oldState.environmentalAppliedToBallistics != newData.environmentalAppliedToBallistics) {
        environmentalParamsChanged = true;

        qDebug() << "[WeaponController] Environmental parameters changed:"
                 << "Temp:" << m_oldState.environmentalTemperatureCelsius << "->"
                 << newData.environmentalTemperatureCelsius
                 << "Alt:" << m_oldState.environmentalAltitudeMeters << "->"
                 << newData.environmentalAltitudeMeters
                 << "Applied:" << m_oldState.environmentalAppliedToBallistics << "->"
                 << newData.environmentalAppliedToBallistics;
    }

    // ========================================================================
    // WINDAGE PARAMETERS CHANGE DETECTION
    // Windage (wind direction + speed) affects crosswind calculation.
    // ========================================================================
    bool windageParamsChanged = false;

    if (!qFuzzyCompare(m_oldState.windageSpeedKnots, newData.windageSpeedKnots) ||
        !qFuzzyCompare(m_oldState.windageDirectionDegrees, newData.windageDirectionDegrees) ||
        m_oldState.windageAppliedToBallistics != newData.windageAppliedToBallistics) {
        windageParamsChanged = true;

        qDebug() << "[WeaponController] Windage parameters changed:"
                 << "Speed:" << m_oldState.windageSpeedKnots << "->" << newData.windageSpeedKnots << "knots"
                 << "Direction:" << m_oldState.windageDirectionDegrees << "->"
                 << newData.windageDirectionDegrees << "deg"
                 << "Applied:" << m_oldState.windageAppliedToBallistics << "->"
                 << newData.windageAppliedToBallistics;
    }

    // ========================================================================
    // ABSOLUTE BEARING CHANGE DETECTION (Critical for Windage!)
    // When absolute gimbal bearing changes, the crosswind component changes
    // even if wind conditions remain constant (crosswind = wind x sin(angle))
    // Absolute bearing = IMU yaw (vehicle heading) + Station azimuth
    // NOTE: azimuthDirection is uint16_t, use direct comparison not qFuzzyCompare
    // ========================================================================

    // Check station azimuth change
    if (m_oldState.azimuthDirection != newData.azimuthDirection) {
        if (newData.windageAppliedToBallistics && newData.windageSpeedKnots > 0.001f) {
            ballisticsInputsChanged = true;
        }
    }

    // Check IMU yaw change (vehicle rotation)
    if (!qFuzzyCompare(m_oldState.imuYawDeg, newData.imuYawDeg)) {
        if (newData.windageAppliedToBallistics && newData.windageSpeedKnots > 0.001f) {
            ballisticsInputsChanged = true;
        }
    }

    // ========================================================================
    // TRIGGER BALLISTICS RECALCULATION
    // Recalculate if LAC is active AND any input changed
    // OR if LAC was just activated/deactivated
    // OR if windage parameters changed
    // OR if absolute bearing changed with windage active
    // ========================================================================
    if (ballisticsInputsChanged || environmentalParamsChanged || windageParamsChanged) {
        updateFireControlSolution();
    }

    m_oldState = newData;
}

// ============================================================================
// AMMO FEED CYCLE FSM - OPERATOR ENTRY POINT
// ============================================================================

void WeaponController::onOperatorRequestLoad()
{
    qDebug() << "[WeaponController] Operator REQUEST LOAD. Current feed state:"
             << feedStateName(m_feedState);

    // Handle FAULT state - operator presses button again to reset
    if (m_feedState == AmmoFeedState::Fault) {
        qDebug() << "[WeaponController] Button pressed in FAULT state - resetting fault";
        resetFeedFault();
        return;
    }

    // Only allow start when idle
    if (m_feedState != AmmoFeedState::Idle) {
        qDebug() << "[WeaponController] Feed cycle start IGNORED - cycle already running";
        return;
    }

    startAmmoFeedCycle();
}

// ============================================================================
// AMMO FEED CYCLE FSM - CYCLE START
// ============================================================================

void WeaponController::startAmmoFeedCycle()
{
    if (!m_servoActuator) {
        qWarning() << "[WeaponController] No servo actuator - cannot start feed cycle";
        return;
    }

    qDebug() << "========== FEED CYCLE STARTED ==========";
    transitionFeedState(AmmoFeedState::Extending);

    // Notify GUI
    if (m_stateModel) {
        m_stateModel->setAmmoFeedCycleInProgress(true);
    }
    emit ammoFeedCycleStarted();

    // Command full extension
    qDebug() << "[WeaponController] Commanding actuator to EXTEND to" << FEED_EXTEND_POS;
    m_servoActuator->moveToPosition(FEED_EXTEND_POS);

    // Start watchdog
    m_feedTimer->start(FEED_TIMEOUT_MS);
}

// ============================================================================
// AMMO FEED CYCLE FSM - ACTUATOR FEEDBACK HANDLER
// ============================================================================

void WeaponController::onActuatorFeedback(const ServoActuatorData& data)
{
    // Extract position (adjust field name to match your ServoActuatorData struct)
    double pos = data.position_mm;  // or data.position_countsposition_mm if using mm

    processActuatorPosition(pos);
}

void WeaponController::processActuatorPosition(double posCounts)
{
    switch (m_feedState) {
    case AmmoFeedState::Extending:
        if (posCounts >= (FEED_EXTEND_POS - FEED_POSITION_TOLERANCE)) {
            qDebug() << "[WeaponController] Extension confirmed at pos =" << posCounts;

            // Extension reached - now retract
            transitionFeedState(AmmoFeedState::Retracting);

            qDebug() << "[WeaponController] Commanding actuator to RETRACT to" << FEED_RETRACT_POS;
            m_servoActuator->moveToPosition(FEED_RETRACT_POS);

            // Restart watchdog for retraction phase
            m_feedTimer->start(FEED_TIMEOUT_MS);
        }
        break;

    case AmmoFeedState::Retracting:
        if (posCounts <= (FEED_RETRACT_POS + FEED_POSITION_TOLERANCE)) {
            qDebug() << "[WeaponController] Retraction confirmed at pos =" << posCounts;

            // Cycle complete
            transitionFeedState(AmmoFeedState::Idle);

            // Notify GUI - belt is now loaded (inferred from successful cycle)
            if (m_stateModel) {
                m_stateModel->setAmmoFeedCycleInProgress(false);
                m_stateModel->setAmmoLoaded(true);
            }

            m_feedTimer->stop();
            emit ammoFeedCycleCompleted();

            qDebug() << "========== FEED CYCLE COMPLETED - BELT LOADED ==========";
        }
        break;

    case AmmoFeedState::Idle:
    case AmmoFeedState::Fault:
    default:
        // Ignore feedback in other states
        break;
    }
}

// ============================================================================
// AMMO FEED CYCLE FSM - TIMEOUT HANDLER
// ============================================================================

void WeaponController::onFeedTimeout()
{
    qWarning() << "[WeaponController] FEED TIMEOUT in state:" << feedStateName(m_feedState)
               << "- actuator did not reach expected position within" << FEED_TIMEOUT_MS << "ms";

    // Enter fault state
    transitionFeedState(AmmoFeedState::Fault);

    // Notify GUI
    if (m_stateModel) {
        m_stateModel->setAmmoFeedCycleInProgress(false);
    }
    emit ammoFeedCycleFaulted();

    // NOTE: We do NOT automatically retract here - operator must clear jam first
    // and then use resetFeedFault() to attempt recovery
}

// ============================================================================
// AMMO FEED CYCLE FSM - FAULT RESET
// ============================================================================

void WeaponController::resetFeedFault()
{
    if (m_feedState != AmmoFeedState::Fault) {
        qDebug() << "[WeaponController] Feed fault reset IGNORED - not in FAULT state";
        return;
    }

    if (!m_servoActuator) {
        qWarning() << "[WeaponController] No actuator available for fault recovery";
        return;
    }

    qDebug() << "[WeaponController] Operator reset - attempting safe retraction";
    transitionFeedState(AmmoFeedState::Retracting);

    // Attempt safe retraction
    m_servoActuator->moveToPosition(FEED_RETRACT_POS);
    m_feedTimer->start(FEED_TIMEOUT_MS);

    // Notify GUI
    if (m_stateModel) {
        m_stateModel->setAmmoFeedCycleInProgress(true);
    }
}

// ============================================================================
// AMMO FEED CYCLE FSM - STATE TRANSITION HELPER
// ============================================================================

void WeaponController::transitionFeedState(AmmoFeedState newState)
{
    qDebug() << "[WeaponController] Feed state:" << feedStateName(m_feedState)
             << "->" << feedStateName(newState);
    m_feedState = newState;

    // Update SystemStateModel for OSD display
    if (m_stateModel) {
        m_stateModel->setAmmoFeedState(newState);
    }
}

QString WeaponController::feedStateName(AmmoFeedState s) const
{
    switch (s) {
    case AmmoFeedState::Idle:       return "Idle";
    case AmmoFeedState::Extending:  return "Extending";
    case AmmoFeedState::Retracting: return "Retracting";
    case AmmoFeedState::Fault:      return "Fault";
    }
    return "Unknown";
}

// ============================================================================
// WEAPON CONTROL
// ============================================================================

void WeaponController::armWeapon(bool enable)
{
    Q_UNUSED(enable)
    // Implementation placeholder
}

void WeaponController::fireSingleShot()
{
    // Implementation placeholder
}

void WeaponController::startFiring()
{
    if (!m_systemArmed) {
        qDebug() << "[WeaponController] Cannot fire: system is not armed";
        return;
    }
    m_plc42->setSolenoidState(1);
}

void WeaponController::stopFiring()
{
    m_plc42->setSolenoidState(0);
}

void WeaponController::unloadAmmo()
{
    // Legacy API - delegate to FSM if not in fault
    // For actual unload cycle, you might want a separate FSM or extend current one
    stopFiring();

    if (m_feedState == AmmoFeedState::Idle) {
        qDebug() << "[WeaponController] Unload requested - initiating retraction cycle";
        // Just retract (unload doesn't need extend)
        transitionFeedState(AmmoFeedState::Retracting);
        m_servoActuator->moveToPosition(FEED_RETRACT_POS);
        m_feedTimer->start(FEED_TIMEOUT_MS);

        if (m_stateModel) {
            m_stateModel->setAmmoFeedCycleInProgress(true);
        }
    } else {
        qDebug() << "[WeaponController] Cannot unload: feed cycle active or faulted";
    }
}

// ============================================================================
// FIRE CONTROL SOLUTION
// ============================================================================

void WeaponController::updateFireControlSolution()
{
    if (!m_stateModel) return;

    SystemStateData sData = m_stateModel->data();

    // ========================================================================
    // CALCULATE CROSSWIND FROM WINDAGE (Direction + Speed)
    // CRITICAL: This calculation runs INDEPENDENTLY of LAC status!
    // Windage provides absolute wind direction and speed (relative to true North).
    // Crosswind component varies with absolute gimbal bearing.
    // Absolute gimbal bearing = Vehicle heading (IMU yaw) + Station azimuth
    // ========================================================================
    float currentCrosswind = 0.0f;

    if (sData.windageAppliedToBallistics && sData.windageSpeedKnots > 0.001f) {
        // Convert wind speed from knots to m/s
        float windSpeedMS = sData.windageSpeedKnots * 0.514444f;  // 1 knot = 0.514444 m/s

        // Calculate absolute gimbal bearing (true bearing where weapon is pointing)
        float absoluteGimbalBearing = static_cast<float>(sData.imuYawDeg) + sData.azimuthDirection;

        // Normalize to 0-360 range
        while (absoluteGimbalBearing >= 360.0f) absoluteGimbalBearing -= 360.0f;
        while (absoluteGimbalBearing < 0.0f) absoluteGimbalBearing += 360.0f;

        // Calculate crosswind component
        currentCrosswind = calculateCrosswindComponent(
            windSpeedMS,
            sData.windageDirectionDegrees,
            absoluteGimbalBearing
        );

        // Store calculated crosswind in state data for OSD display
        if (std::abs(sData.calculatedCrosswindMS - currentCrosswind) > 0.01f) {
            SystemStateData updatedData = sData;
            updatedData.calculatedCrosswindMS = currentCrosswind;
            m_stateModel->updateData(updatedData);
            sData = updatedData;
        }
    } else {
        // Clear crosswind when windage is not applied
        if (std::abs(sData.calculatedCrosswindMS) > 0.01f) {
            SystemStateData updatedData = sData;
            updatedData.calculatedCrosswindMS = 0.0f;
            m_stateModel->updateData(updatedData);
            sData = updatedData;
        }
    }

    // ========================================================================
    // PROFESSIONAL FCS: SPLIT BALLISTIC DROP AND MOTION LEAD
    // Drop compensation: Auto-applied when LRF range valid (Kongsberg/Rafael)
    // Motion lead: Only when LAC toggle active
    // ========================================================================
    if (!m_ballisticsProcessor) {
        qCritical() << "[WeaponController] BallisticsProcessor pointer is NULL!";
        return;
    }

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
            15.0f, 0.0f, currentCrosswind  // Standard conditions + wind
        );
    }

    // ========================================================================
    // STEP 2: BALLISTIC DROP - AUTO-APPLIED WHEN RANGE VALID (Professional FCS)
    // ========================================================================
    bool applyDrop = (targetRange > 0.1f);  // Valid range threshold

    if (applyDrop) {
        LeadCalculationResult drop = m_ballisticsProcessor->calculateBallisticDrop(targetRange);

        SystemStateData updatedData = sData;
        updatedData.ballisticDropOffsetAz = drop.leadAzimuthDegrees;
        updatedData.ballisticDropOffsetEl = drop.leadElevationDegrees;
        updatedData.ballisticDropActive = true;
        m_stateModel->updateData(updatedData);
        sData = updatedData;

        qDebug() << "[WeaponController] AUTO DROP APPLIED:"
                 << "Range:" << targetRange << "m"
                 << "| Drop Az:" << drop.leadAzimuthDegrees << "deg (wind)"
                 << "| Drop El:" << drop.leadElevationDegrees << "deg (gravity)";
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
            // Backward compatibility
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

    SystemStateData updatedData = sData;
    updatedData.motionLeadOffsetAz = lead.leadAzimuthDegrees;
    updatedData.motionLeadOffsetEl = lead.leadElevationDegrees;
    updatedData.currentLeadAngleStatus = lead.status;
    // Backward compatibility
    updatedData.leadAngleOffsetAz = lead.leadAzimuthDegrees;
    updatedData.leadAngleOffsetEl = lead.leadElevationDegrees;
    m_stateModel->updateData(updatedData);

    qDebug() << "[WeaponController] MOTION LEAD:"
             << "Az:" << lead.leadAzimuthDegrees << "deg"
             << "| El:" << lead.leadElevationDegrees << "deg"
             << "| Status:" << static_cast<int>(lead.status)
             << (lead.status == LeadAngleStatus::On ? "(On)" :
                 lead.status == LeadAngleStatus::Lag ? "(Lag)" :
                 lead.status == LeadAngleStatus::ZoomOut ? "(ZoomOut)" : "(Unknown)");
}

// ============================================================================
// CROSSWIND CALCULATION
// ============================================================================

float WeaponController::calculateCrosswindComponent(float windSpeedMS,
                                                    float windDirectionDeg,
                                                    float gimbalAzimuthDeg)
{
    // ========================================================================
    // BALLISTICS PHYSICS: Crosswind Component Calculation
    // Wind direction: Direction wind is coming FROM (meteorological convention)
    // Gimbal azimuth: Direction weapon is pointing TO
    //
    // Relative angle = wind_direction - firing_direction
    // Crosswind = wind_speed x sin(relative_angle)
    //
    // Examples:
    //   Wind from 0deg (North), firing at 0deg (North):
    //     relative = 0deg -> sin(0deg) = 0 -> no crosswind (headwind)
    //   Wind from 0deg (North), firing at 90deg (East):
    //     relative = -90deg -> sin(-90deg) = -1 -> full crosswind (left)
    //   Wind from 0deg (North), firing at 180deg (South):
    //     relative = -180deg -> sin(-180deg) = 0 -> no crosswind (tailwind)
    //   Wind from 0deg (North), firing at 270deg (West):
    //     relative = -270deg = +90deg -> sin(90deg) = +1 -> full crosswind (right)
    // ========================================================================

    float relativeAngle = windDirectionDeg - gimbalAzimuthDeg;

    // Normalize to [-180, +180] for correct trigonometry
    while (relativeAngle > 180.0f) {
        relativeAngle -= 360.0f;
    }
    while (relativeAngle < -180.0f) {
        relativeAngle += 360.0f;
    }

    // Calculate crosswind component
    // sin(90deg) = 1.0 -> full crosswind (wind perpendicular to fire direction)
    // sin(0deg) = 0.0 -> no crosswind (headwind or tailwind)
    float crosswindMS = windSpeedMS * std::sin(relativeAngle * static_cast<float>(M_PI) / 180.0f);

    return crosswindMS;
}
