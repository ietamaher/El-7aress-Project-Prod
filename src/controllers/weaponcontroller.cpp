#include "weaponcontroller.h"

// ============================================================================
// Project
// ============================================================================
#include "models/domain/systemstatemodel.h"
#include "hardware/devices/servoactuatordevice.h"
#include "hardware/devices/plc42device.h"
#include "safety/SafetyInterlock.h"

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
                                   SafetyInterlock* safetyInterlock,
                                   QObject* parent)
    : QObject(parent)
    , m_stateModel(stateModel)
    , m_servoActuator(servoActuator)
    , m_plc42(plc42)
    , m_safetyInterlock(safetyInterlock)
{
    // Log SafetyInterlock status
    if (m_safetyInterlock) {
        qInfo() << "[WeaponController] SafetyInterlock connected - centralized safety active";
    } else {
        qWarning() << "[WeaponController] WARNING: SafetyInterlock is null - using legacy safety checks";
    }
    // ========================================================================
    // SYSTEM STATE MONITORING
    // ========================================================================
    if (m_stateModel) {
        // ========================================================================
        // BUG FIX #4: MOTOR CONTROL LATENCY - USE DIRECT CONNECTION
        // ========================================================================
        // ORIGINAL: Qt::QueuedConnection caused event queue saturation over time
        // - Servo updates at 110Hz → ~1200 events/min from state changes
        // - Queued events accumulate if processing takes longer than arrival rate
        // - Result: Latency increases from 0ms to several seconds over time
        //
        // FIX: Use Qt::DirectConnection (all components in main thread)
        // - Immediate processing, no event queue buildup
        // - Safe because SystemStateModel, WeaponController, and GimbalController
        //   all run in the main thread (required by QModbus)
        // - Ballistics calculations are fast (<1ms), won't block I/O
        // ========================================================================
        connect(m_stateModel, &SystemStateModel::dataChanged,
                this, &WeaponController::onSystemStateChanged,
                Qt::DirectConnection);  // ✅ FIX: Changed from QueuedConnection

        // Initialize GUI state for charging
        m_stateModel->setChargeCycleInProgress(false);
    }

    // ========================================================================
    // CHARGING STATE MACHINE SETUP (Cocking Actuator FSM - Extracted)
    // ========================================================================
    m_chargingStateMachine = new ChargingStateMachine(m_servoActuator, m_safetyInterlock, this);

    // Connect ChargingStateMachine signals
    connect(m_chargingStateMachine, &ChargingStateMachine::stateChanged,
            this, &WeaponController::onChargingStateChanged);
    connect(m_chargingStateMachine, &ChargingStateMachine::cycleStarted,
            this, &WeaponController::onChargeCycleStarted);
    connect(m_chargingStateMachine, &ChargingStateMachine::cycleCompleted,
            this, &WeaponController::onChargeCycleCompleted);
    connect(m_chargingStateMachine, &ChargingStateMachine::cycleFaulted,
            this, &WeaponController::onChargeCycleFaulted);
    connect(m_chargingStateMachine, &ChargingStateMachine::lockoutExpired,
            this, &WeaponController::onChargeLockoutExpired);

    // Connect actuator control signals from ChargingStateMachine
    connect(m_chargingStateMachine, &ChargingStateMachine::requestActuatorMove,
            this, &WeaponController::onActuatorMoveRequested);
    connect(m_chargingStateMachine, &ChargingStateMachine::requestActuatorStop,
            this, &WeaponController::onActuatorStopRequested);

    // Connect actuator position feedback for FSM state transitions
    if (m_servoActuator) {
        connect(m_servoActuator, &ServoActuatorDevice::actuatorDataChanged,
                this, &WeaponController::onActuatorFeedback,
                Qt::QueuedConnection);

        // ====================================================================
        // CROWS M153: AUTOMATIC RETRACTION ON STARTUP
        // ====================================================================
        // Per CROWS spec: "Upon powering on the CROWS, the system automatically
        // retracts the Cocking Actuator if it is extended to ensure it does not
        // interfere with firing."
        // Use a single-shot timer to perform this after construction completes
        QTimer::singleShot(500, this, [this]() {
            if (m_stateModel) {
                m_chargingStateMachine->performStartupRetraction(m_stateModel->data().actuatorPosition);
            }
        });
    }

    // ========================================================================
    // Log installed weapon type
    // ========================================================================
    if (m_stateModel) {
        WeaponType weaponType = m_stateModel->data().installedWeaponType;
        qInfo() << "[WeaponController] Installed weapon type:" << static_cast<int>(weaponType);
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
    // Clean up ballistics processor
    delete m_ballisticsProcessor;
    m_ballisticsProcessor = nullptr;

    // Note: m_chargingStateMachine is a QObject child, cleaned up automatically
}

// ============================================================================
// STATE CHANGE HANDLER
// ============================================================================

void WeaponController::onSystemStateChanged(const SystemStateData& newData)
{
    // ========================================================================
    // PRIORITY 1: EMERGENCY STOP HANDLING (HIGHEST PRIORITY - MIL-STD SAFETY)
    // ========================================================================
    // When emergency stop is activated:
    // 1. IMMEDIATELY stop solenoid (cease fire)
    // 2. IMMEDIATELY stop servo actuator (halt charging mechanism)
    // 3. Block ALL weapon operations until emergency is cleared
    //
    // This is a fail-safe mechanism - we stop FIRST, ask questions later.
    // ========================================================================
    if (newData.emergencyStopActive != m_oldState.emergencyStopActive) {
        if (newData.emergencyStopActive && !m_wasInEmergencyStop) {
            // EMERGENCY STOP ACTIVATED - IMMEDIATE HALT
            qCritical() << "";
            qCritical() << "╔══════════════════════════════════════════════════════════════╗";
            qCritical() << "║     WEAPON CONTROLLER: EMERGENCY STOP ACTIVATED              ║";
            qCritical() << "╚══════════════════════════════════════════════════════════════╝";

            // STEP 1: CEASE FIRE IMMEDIATELY
            if (m_plc42) {
                m_plc42->setSolenoidState(0);
                qCritical() << "[WeaponController] SOLENOID DISABLED - Fire circuit interrupted";
            }

            // STEP 2: STOP SERVO ACTUATOR IMMEDIATELY
            if (m_servoActuator) {
                m_servoActuator->stopMove();
                qCritical() << "[WeaponController] SERVO ACTUATOR STOPPED - Charging halted";
            }

            // STEP 3: ABORT ANY IN-PROGRESS CHARGE CYCLE (delegated to ChargingStateMachine)
            if (m_chargingStateMachine->isChargingInProgress()) {
                m_chargingStateMachine->abort("Emergency stop activated");
                qCritical() << "[WeaponController] CHARGE CYCLE ABORTED - State set to FAULT";
            }

            m_wasInEmergencyStop = true;
            qCritical() << "[WeaponController] ALL WEAPON OPERATIONS BLOCKED";
            qCritical() << "";

        } else if (!newData.emergencyStopActive && m_wasInEmergencyStop) {
            // EMERGENCY STOP CLEARED - INITIATE SAFE RECOVERY
            qInfo() << "";
            qInfo() << "╔══════════════════════════════════════════════════════════════╗";
            qInfo() << "║     WEAPON CONTROLLER: EMERGENCY STOP CLEARED                ║";
            qInfo() << "╚══════════════════════════════════════════════════════════════╝";

            // ================================================================
            // SAFE RECOVERY: AUTO-RETRACT ACTUATOR TO HOME POSITION
            // ================================================================
            // Per CROWS spec: Actuator must be retracted before firing allowed
            // If actuator was stopped mid-cycle, it could be extended and
            // interfere with firing. We automatically retract to safe position.
            // Delegated to ChargingStateMachine via resetFault().
            // ================================================================
            if (m_servoActuator && m_chargingStateMachine->currentState() == ChargingState::Fault) {
                qInfo() << "[WeaponController] SAFE RECOVERY: Initiating safe retraction via ChargingStateMachine";
                m_chargingStateMachine->resetFault();
            }

            qInfo() << "[WeaponController] Weapon operations now permitted";
            qInfo() << "[WeaponController] Operator must re-arm and re-charge weapon";
            qInfo() << "";

            m_wasInEmergencyStop = false;
        }
    }

    // If emergency stop is active, skip ALL other processing
    // This is a hard block on all weapon operations
    if (newData.emergencyStopActive) {
        m_oldState = newData;
        return;
    }

    // ========================================================================
    // CHARGING CYCLE (delegated to ChargingStateMachine)
    // Detect button PRESS/RELEASE and forward to FSM
    // ========================================================================
    if (newData.chargeButtonPressed && !m_oldState.chargeButtonPressed) {
        // Button was just pressed - try to start cycle
        onOperatorRequestCharge();
    }

    // Handle button release (for continuous hold mode)
    if (!newData.chargeButtonPressed && m_oldState.chargeButtonPressed) {
        m_chargingStateMachine->onButtonReleased();
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
        /*qDebug() << "[WeaponController] Target range changed:"
                 << m_oldState.currentTargetRange << "->" << newData.currentTargetRange << "m";*/
    }

    // Target angular rates (tracking motion)
    if (!qFuzzyCompare(m_oldState.currentTargetAngularRateAz, newData.currentTargetAngularRateAz) ||
        !qFuzzyCompare(m_oldState.currentTargetAngularRateEl, newData.currentTargetAngularRateEl)) {
        ballisticsInputsChanged = true;
        /*qDebug() << "[WeaponController] Target angular rates changed: Az:"
                 << m_oldState.currentTargetAngularRateAz << "->" << newData.currentTargetAngularRateAz
                 << "El:" << m_oldState.currentTargetAngularRateEl << "->" << newData.currentTargetAngularRateEl;*/
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
// CHARGING CYCLE - OPERATOR ENTRY POINT (delegated to ChargingStateMachine)
// ============================================================================

void WeaponController::onOperatorRequestCharge()
{
    // Get current weapon type from state model
    WeaponType weaponType = WeaponType::Unknown;
    if (m_stateModel) {
        weaponType = m_stateModel->data().installedWeaponType;
    }

    // Delegate to ChargingStateMachine
    // It handles: fault reset, safety checks, multi-cycle initialization
    m_chargingStateMachine->requestCharge(weaponType);
}

// ============================================================================
// CHARGINGSTATEMACHINE SIGNAL HANDLERS
// ============================================================================

void WeaponController::onChargingStateChanged(ChargingState newState)
{
    // Update SystemStateModel for OSD display
    if (m_stateModel) {
        m_stateModel->setChargingState(newState);
    }
}

void WeaponController::onChargeCycleStarted()
{
    // Notify GUI
    if (m_stateModel) {
        m_stateModel->setChargeCycleInProgress(true);

        // Update cycle progress in state model
        SystemStateData data = m_stateModel->data();
        data.chargeCyclesCompleted = m_chargingStateMachine->currentCycle();
        data.chargeCyclesRequired = m_chargingStateMachine->requiredCycles();
        m_stateModel->updateData(data);
    }
    emit chargeCycleStarted();
}

void WeaponController::onChargeCycleCompleted()
{
    // Notify GUI - weapon is now charged
    if (m_stateModel) {
        m_stateModel->setChargeCycleInProgress(false);
        m_stateModel->setWeaponCharged(true);

        // Update cycle count in state for OSD
        SystemStateData data = m_stateModel->data();
        data.chargeCyclesCompleted = m_chargingStateMachine->currentCycle();
        m_stateModel->updateData(data);
    }
    emit chargeCycleCompleted();
}

void WeaponController::onChargeCycleFaulted()
{
    // Notify GUI
    if (m_stateModel) {
        m_stateModel->setChargeCycleInProgress(false);
    }
    emit chargeCycleFaulted();
}

void WeaponController::onChargeLockoutExpired()
{
    // Update state model for GUI feedback
    if (m_stateModel) {
        SystemStateData data = m_stateModel->data();
        data.chargeLockoutActive = false;
        m_stateModel->updateData(data);
    }
}

void WeaponController::onActuatorFeedback(const ServoActuatorData& data)
{
    // Forward feedback to ChargingStateMachine for FSM processing
    m_chargingStateMachine->processActuatorFeedback(data);
}

void WeaponController::onActuatorMoveRequested(double positionMM)
{
    // Execute actuator move request from ChargingStateMachine
    if (m_servoActuator) {
        m_servoActuator->moveToPosition(positionMM);
    }
}

void WeaponController::onActuatorStopRequested()
{
    // Execute actuator stop request from ChargingStateMachine
    if (m_servoActuator) {
        m_servoActuator->stopMove();
    }
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
    // ========================================================================
    // SAFETY INTERLOCK INTEGRATION (Phase 2 - Centralized Fire Authorization)
    // ========================================================================
    // ALL fire authorization now routes through SafetyInterlock.canFire().
    // This ensures:
    // 1. Single authority for fire decisions (auditable)
    // 2. All safety checks in one place (certification-ready)
    // 3. Comprehensive audit logging with timestamps
    //
    // SafetyInterlock.canFire() checks:
    // - Emergency stop not active
    // - Station enabled
    // - Dead man switch held
    // - Gun armed
    // - Operational mode is Engagement
    // - System authorized
    // - Not in no-fire zone
    // - Not actively charging
    // - No charge fault/jam
    // ========================================================================
    if (m_safetyInterlock) {
        SafetyDenialReason reason;
        if (!m_safetyInterlock->canFire(&reason)) {
            // Denial is already logged by SafetyInterlock with timestamp
            // Only log critical events at WeaponController level
            if (reason == SafetyDenialReason::EmergencyStopActive) {
                qCritical() << "[WeaponController] FIRE BLOCKED: EMERGENCY STOP ACTIVE";
            }
            return;
        }
    } else {
        // ====================================================================
        // LEGACY FALLBACK (if SafetyInterlock not available)
        // ====================================================================
        qWarning() << "[WeaponController] WARNING: SafetyInterlock unavailable - using legacy checks";

        if (m_stateModel && m_stateModel->data().emergencyStopActive) {
            qCritical() << "[WeaponController] FIRE BLOCKED: EMERGENCY STOP ACTIVE";
            return;
        }

        if (!m_systemArmed) {
            qDebug() << "[WeaponController] Cannot fire: system is not armed";
            return;
        }

        if (m_chargingStateMachine->isChargingInProgress()) {
            qWarning() << "[WeaponController] FIRE BLOCKED: Charging cycle in progress";
            return;
        }

        if (m_stateModel) {
            SystemStateData s = m_stateModel->data();
            if (m_stateModel->isPointInNoFireZone(s.gimbalAz, s.gimbalEl)) {
                qWarning() << "[WeaponController] FIRE BLOCKED: In No-Fire zone";
                return;
            }
        }
    }

    // ========================================================================
    // FIRE COMMAND AUTHORIZED - ACTIVATE SOLENOID
    // ========================================================================
    // All safety checks passed via SafetyInterlock.canFire()
    // Proceed with fire command
    // ========================================================================

    // Future: CROWS/SARP LAC WORKFLOW (commented out for Phase 2)
    // Per CROWS doctrine: Lead is only applied when fire trigger is pressed.
    /*if (m_stateModel) {
        SystemStateData s = m_stateModel->data();
        if (s.lacArmed && !s.leadAngleCompensationActive) {
            qInfo() << "[CROWS] FIRE TRIGGER - Engaging LAC (lead now applied)";
            m_stateModel->engageLAC();
        }
    }*/

    // Future: CROWS DEAD RECKONING (commented out for Phase 2)
    // Per TM 9-1090-225-10-2 page 38
    /*if (m_stateModel) {
        SystemStateData s = m_stateModel->data();
        if (s.currentTrackingPhase == TrackingPhase::Tracking_ActiveLock ||
            s.currentTrackingPhase == TrackingPhase::Tracking_Coast) {
            qInfo() << "[CROWS] FIRING DURING TRACKING - Entering dead reckoning mode";
            m_stateModel->enterDeadReckoning(
                s.currentTargetAngularRateAz,
                s.currentTargetAngularRateEl
            );
        }
    }*/

    // Send solenoid command - fire authorized
    m_plc42->setSolenoidState(1);
}

void WeaponController::stopFiring()
{
    m_plc42->setSolenoidState(0);

    if (m_stateModel) {
        SystemStateData s = m_stateModel->data();

        // =========================================================================
        // CROWS/SARP LAC WORKFLOW - DISENGAGE LAC ON TRIGGER RELEASE
        // =========================================================================
        // Per CROWS doctrine: Lead is only applied during firing.
        // When fire trigger is released, disengage LAC (but keep it armed).
        // =========================================================================
       /*if (s.lacArmed && s.leadAngleCompensationActive) {
            qInfo() << "[CROWS] TRIGGER RELEASED - Disengaging LAC (lead removed, LAC still armed)";
            m_stateModel->disengageLAC();
        }*/

        // =========================================================================
        // CROWS: Exit dead reckoning when firing stops
        // =========================================================================
        // Per CROWS doctrine: Firing terminates tracking completely.
        // After firing, operator must re-acquire target to resume tracking.
        // =========================================================================
        /*if (s.deadReckoningActive) {
            m_stateModel->exitDeadReckoning();
        }*/
    }
}

void WeaponController::unloadWeapon()
{
    // Legacy API - stop firing first
    stopFiring();

    // Unload is essentially a safe retraction
    // Only allow when idle
    if (m_chargingStateMachine->currentState() == ChargingState::Idle) {
        qDebug() << "[WeaponController] Unload requested - initiating retraction via ChargingStateMachine";
        // Use performStartupRetraction which handles safe retraction
        if (m_stateModel) {
            m_chargingStateMachine->performStartupRetraction(m_stateModel->data().actuatorPosition);
        }
    } else {
        qDebug() << "[WeaponController] Cannot unload: charge cycle active or faulted";
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
    // BUILD INPUT FOR FIRE CONTROL COMPUTATION
    // ========================================================================
    FireControlInput input = buildFireControlInput(sData);

    // ========================================================================
    // COMPUTE FIRE CONTROL SOLUTION (delegated to FireControlComputation)
    // ========================================================================
    // This is now a pure computation with no side effects.
    // All state updates happen in applyFireControlResult().
    // ========================================================================
    FireControlResult result = m_fireControlComputation.compute(
        input,
        m_ballisticsProcessor,
        m_previousFireControlResult
    );

    // ========================================================================
    // APPLY RESULT TO STATE MODEL
    // ========================================================================
    applyFireControlResult(result, sData);

    // Store result for next iteration's change detection
    m_previousFireControlResult = result;
}

// ============================================================================
// FIRE CONTROL HELPER METHODS
// ============================================================================

FireControlInput WeaponController::buildFireControlInput(const SystemStateData& data) const
{
    FireControlInput input;

    // Range data
    input.currentTargetRange = data.currentTargetRange;

    // Angular rate data
    input.currentTargetAngularRateAz = data.currentTargetAngularRateAz;
    input.currentTargetAngularRateEl = data.currentTargetAngularRateEl;

    // Camera data
    input.activeCameraIsDay = data.activeCameraIsDay;
    input.dayCurrentHFOV = data.dayCurrentHFOV;
    input.dayCurrentVFOV = data.dayCurrentVFOV;
    input.nightCurrentHFOV = data.nightCurrentHFOV;
    input.nightCurrentVFOV = data.nightCurrentVFOV;

    // Windage data
    input.windageAppliedToBallistics = data.windageAppliedToBallistics;
    input.windageSpeedKnots = data.windageSpeedKnots;
    input.windageDirectionDegrees = data.windageDirectionDegrees;

    // Gimbal / vehicle data
    input.imuYawDeg = static_cast<float>(data.imuYawDeg);
    input.azimuthDirection = data.azimuthDirection;

    // Environmental data
    input.environmentalAppliedToBallistics = data.environmentalAppliedToBallistics;
    input.environmentalTemperatureCelsius = data.environmentalTemperatureCelsius;
    input.environmentalAltitudeMeters = data.environmentalAltitudeMeters;

    // LAC state
    input.leadAngleCompensationActive = data.leadAngleCompensationActive;

    return input;
}

void WeaponController::applyFireControlResult(const FireControlResult& result, SystemStateData& data)
{
    bool needsUpdate = false;

    // ========================================================================
    // APPLY CROSSWIND
    // ========================================================================
    if (result.crosswindChanged) {
        data.calculatedCrosswindMS = result.calculatedCrosswindMS;
        needsUpdate = true;
    }

    // ========================================================================
    // APPLY BALLISTIC DROP
    // ========================================================================
    if (result.dropChanged ||
        data.ballisticDropOffsetAz != result.ballisticDropOffsetAz ||
        data.ballisticDropOffsetEl != result.ballisticDropOffsetEl ||
        data.ballisticDropActive != result.ballisticDropActive) {

        data.ballisticDropOffsetAz = result.ballisticDropOffsetAz;
        data.ballisticDropOffsetEl = result.ballisticDropOffsetEl;
        data.ballisticDropActive = result.ballisticDropActive;
        needsUpdate = true;
    }

    // ========================================================================
    // APPLY MOTION LEAD
    // ========================================================================
    if (result.leadChanged ||
        data.motionLeadOffsetAz != result.motionLeadOffsetAz ||
        data.motionLeadOffsetEl != result.motionLeadOffsetEl ||
        data.currentLeadAngleStatus != result.currentLeadAngleStatus) {

        data.motionLeadOffsetAz = result.motionLeadOffsetAz;
        data.motionLeadOffsetEl = result.motionLeadOffsetEl;
        data.currentLeadAngleStatus = result.currentLeadAngleStatus;

        // Backward compatibility
        data.leadAngleOffsetAz = result.leadAngleOffsetAz;
        data.leadAngleOffsetEl = result.leadAngleOffsetEl;
        needsUpdate = true;
    }

    // ========================================================================
    // UPDATE STATE MODEL IF ANY VALUES CHANGED
    // ========================================================================
    if (needsUpdate) {
        m_stateModel->updateData(data);
    }
}

// ============================================================================
// NOTE: CROWS M153 charging functions have been extracted to ChargingStateMachine
// See: chargingstatemachine.h for getRequiredCyclesForWeapon, startLockout, etc.
// ============================================================================

// ============================================================================
// NOTE: Crosswind calculation has been extracted to FireControlComputation class
// for unit-testability. See: FireControlComputation::calculateCrosswindComponent()
// ============================================================================