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

        // Initialize GUI state for ammo feed
        m_stateModel->setAmmoFeedCycleInProgress(false);
    }

    // ========================================================================
    // AMMO FEED FSM SETUP
    // ========================================================================
    m_feedTimer = new QTimer(this);
    m_feedTimer->setSingleShot(true);
    connect(m_feedTimer, &QTimer::timeout, this, &WeaponController::onFeedTimeout);

    // ========================================================================
    // CROWS M153: 4-SECOND POST-CHARGE LOCKOUT TIMER
    // ========================================================================
    // Per CROWS spec: "Once charging completes, CROWS prevents additional
    // charging for four seconds."
    m_lockoutTimer = new QTimer(this);
    m_lockoutTimer->setSingleShot(true);
    connect(m_lockoutTimer, &QTimer::timeout, this, &WeaponController::onChargeLockoutExpired);

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
        QTimer::singleShot(500, this, &WeaponController::performStartupRetraction);
    }

    // ========================================================================
    // Initialize weapon type and required cycles from state model
    // ========================================================================
    if (m_stateModel) {
        WeaponType weaponType = m_stateModel->data().installedWeaponType;
        m_requiredCycles = getRequiredCyclesForWeapon(weaponType);
        qInfo() << "[WeaponController] Installed weapon type:" << static_cast<int>(weaponType)
                << "| Required charge cycles:" << m_requiredCycles;
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
    if (m_lockoutTimer) {
        m_lockoutTimer->stop();
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

            // STEP 3: ABORT ANY IN-PROGRESS FEED CYCLE
            if (m_feedState != AmmoFeedState::Idle && m_feedState != AmmoFeedState::Lockout) {
                m_feedTimer->stop();
                transitionFeedState(AmmoFeedState::Fault);
                if (m_stateModel) {
                    m_stateModel->setAmmoFeedCycleInProgress(false);
                }
                qCritical() << "[WeaponController] FEED CYCLE ABORTED - State set to FAULT";
            }

            m_wasInEmergencyStop = true;
            qCritical() << "[WeaponController] ALL WEAPON OPERATIONS BLOCKED";
            qCritical() << "";

        } else if (!newData.emergencyStopActive && m_wasInEmergencyStop) {
            // EMERGENCY STOP CLEARED
            qInfo() << "";
            qInfo() << "╔══════════════════════════════════════════════════════════════╗";
            qInfo() << "║     WEAPON CONTROLLER: EMERGENCY STOP CLEARED                ║";
            qInfo() << "╚══════════════════════════════════════════════════════════════╝";
            qInfo() << "[WeaponController] Weapon operations now permitted";
            qInfo() << "[WeaponController] Operator must re-arm and re-charge if needed";
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
    // AMMO FEED CYCLE - NEW FSM-BASED APPROACH
    // Detect button PRESS (edge trigger, not level)
    // Button RELEASE is intentionally ignored - FSM controls the cycle
    // ========================================================================
    if (newData.ammoLoadButtonPressed && !m_oldState.ammoLoadButtonPressed) {
        // Button was just pressed - try to start cycle
        onOperatorRequestLoad();
    }

    // Handle button release
    if (!newData.ammoLoadButtonPressed && m_oldState.ammoLoadButtonPressed) {
        if (m_feedState == AmmoFeedState::Extended) {
            // ================================================================
            // CONTINUOUS HOLD MODE: Button released while in Extended state
            // ================================================================
            // This counts as ONE complete cycle (manual control mode)
            // For M2HB: Operator must press and hold twice for full charge
            // For M240/M249: One hold-and-release is sufficient
            m_currentCycleCount++;
            m_isShortPressCharge = false;  // Mark as continuous hold mode

            qInfo() << "[WeaponController] Button RELEASED in Extended state - initiating retraction"
                    << "(Cycle" << m_currentCycleCount << "of" << m_requiredCycles << ")";

            transitionFeedState(AmmoFeedState::Retracting);
            m_servoActuator->moveToPosition(FEED_RETRACT_POS);
            m_feedTimer->start(FEED_TIMEOUT_MS);  // Start watchdog for retraction
        } else if (m_feedState == AmmoFeedState::Extending || m_feedState == AmmoFeedState::Retracting) {
            // Button released during extend/retract motion - logged but cycle continues
            qDebug() << "[WeaponController] Button released during" << feedStateName(m_feedState) << "- cycle continues";
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
// AMMO FEED CYCLE FSM - OPERATOR ENTRY POINT
// ============================================================================

void WeaponController::onOperatorRequestLoad()
{
    // ========================================================================
    // EMERGENCY STOP CHECK (HIGHEST PRIORITY - FAIL-FAST)
    // ========================================================================
    if (m_stateModel && m_stateModel->data().emergencyStopActive) {
        qCritical() << "[WeaponController] CHARGE BLOCKED: EMERGENCY STOP ACTIVE";
        return;
    }

    qDebug() << "[WeaponController] Operator REQUEST LOAD. Current feed state:"
             << feedStateName(m_feedState)
             << "| Lockout active:" << m_chargeLockoutActive;

    // ========================================================================
    // CROWS M153: CHECK LOCKOUT STATE
    // ========================================================================
    // Per CROWS spec: "Once charging completes, CROWS prevents additional
    // charging for four seconds."
    if (m_feedState == AmmoFeedState::Lockout || m_chargeLockoutActive) {
        qDebug() << "[WeaponController] CHARGE BLOCKED - 4-second lockout active";
        return;
    }

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

    // ========================================================================
    // CROWS M153: INITIALIZE MULTI-CYCLE CHARGING
    // ========================================================================
    // Reset cycle counter for new charge sequence
    m_currentCycleCount = 0;

    // Update required cycles from current weapon type
    if (m_stateModel) {
        WeaponType weaponType = m_stateModel->data().installedWeaponType;
        m_requiredCycles = getRequiredCyclesForWeapon(weaponType);
    }

    qInfo() << "[WeaponController] Starting charge sequence:"
            << m_requiredCycles << "cycle(s) required for weapon type";

    startAmmoFeedCycle();
}

// ============================================================================
// AMMO FEED CYCLE FSM - CYCLE START
// ============================================================================

void WeaponController::startAmmoFeedCycle()
{
    // ========================================================================
    // EMERGENCY STOP CHECK (HIGHEST PRIORITY - FAIL-FAST)
    // ========================================================================
    if (m_stateModel && m_stateModel->data().emergencyStopActive) {
        qCritical() << "[WeaponController] FEED CYCLE BLOCKED: EMERGENCY STOP ACTIVE";
        return;
    }

    if (!m_servoActuator) {
        qWarning() << "[WeaponController] No servo actuator - cannot start feed cycle";
        return;
    }

    qInfo() << "╔══════════════════════════════════════════════════════════════╗";
    qInfo() << "║           CHARGE CYCLE STARTING                              ║";
    qInfo() << "║   Cycle:" << (m_currentCycleCount + 1) << "of" << m_requiredCycles
            << "                                        ║";
    qInfo() << "╚══════════════════════════════════════════════════════════════╝";

    // Reset jam detection for new cycle
    resetJamDetection();

    transitionFeedState(AmmoFeedState::Extending);

    // Notify GUI
    if (m_stateModel) {
        m_stateModel->setAmmoFeedCycleInProgress(true);

        // Update cycle progress in state model
        SystemStateData data = m_stateModel->data();
        data.chargeCyclesCompleted = m_currentCycleCount;
        data.chargeCyclesRequired = m_requiredCycles;
        m_stateModel->updateData(data);
    }
    emit ammoFeedCycleStarted();

    // Command full extension
    m_servoActuator->moveToPosition(FEED_EXTEND_POS);

    // Start watchdog
    m_feedTimer->start(FEED_TIMEOUT_MS);
}

// ============================================================================
// AMMO FEED CYCLE FSM - ACTUATOR FEEDBACK HANDLER
// ============================================================================

void WeaponController::onActuatorFeedback(const ServoActuatorData& data)
{
    // ========================================================================
    // JAM DETECTION - CRITICAL SAFETY CHECK (runs BEFORE position processing)
    // Only active during motion states where jam can occur
    // ========================================================================
    if (m_feedState == AmmoFeedState::Extending || 
        m_feedState == AmmoFeedState::Retracting) {
        checkForJamCondition(data);
    }
    
    // Process position for FSM state transitions
    processActuatorPosition(data.position_mm);
}

void WeaponController::processActuatorPosition(double posMM)
{
    switch (m_feedState) {
    case AmmoFeedState::Extending:
        if (posMM >= (FEED_EXTEND_POS - FEED_POSITION_TOLERANCE)) {
            // Extension reached - check if button is still held
            m_feedTimer->stop();  // Stop watchdog while we check

            if (m_stateModel && m_stateModel->data().ammoLoadButtonPressed) {
                // ================================================================
                // CONTINUOUS HOLD MODE (Original behavior preserved)
                // ================================================================
                // Button still held - hold extended position until release
                m_isShortPressCharge = false;  // Mark as continuous hold
                transitionFeedState(AmmoFeedState::Extended);
                qDebug() << "[WeaponController] Extension complete - HOLDING (button still pressed)";
                // No watchdog in Extended state - operator controls timing
            } else {
                // ================================================================
                // SHORT PRESS MODE (CROWS M153 multi-cycle)
                // ================================================================
                // Button already released - this is a short press charge
                m_isShortPressCharge = true;
                m_currentCycleCount++;

                qInfo() << "[WeaponController] Cycle" << m_currentCycleCount << "of"
                        << m_requiredCycles << "complete - retracting";

                // Retract after extension
                transitionFeedState(AmmoFeedState::Retracting);
                m_servoActuator->moveToPosition(FEED_RETRACT_POS);
                m_feedTimer->start(FEED_TIMEOUT_MS);  // Restart watchdog for retraction
            }
        }
        break;

    case AmmoFeedState::Extended:
        // In Extended state, we just hold position
        // Transition to Retracting happens via button release in onSystemStateChanged()
        break;

    case AmmoFeedState::Retracting:
        if (posMM <= (FEED_RETRACT_POS + FEED_POSITION_TOLERANCE)) {
            m_feedTimer->stop();

            // ====================================================================
            // CROWS M153: CHECK IF MORE CYCLES NEEDED (SHORT PRESS MODE ONLY)
            // ====================================================================
            if (m_isShortPressCharge && m_currentCycleCount < m_requiredCycles) {
                // More cycles needed - start next extension cycle
                qInfo() << "[WeaponController] Starting cycle" << (m_currentCycleCount + 1)
                        << "of" << m_requiredCycles;

                transitionFeedState(AmmoFeedState::Extending);
                m_servoActuator->moveToPosition(FEED_EXTEND_POS);
                m_feedTimer->start(FEED_TIMEOUT_MS);

                // Reset jam detection for new cycle
                resetJamDetection();
            } else {
                // ================================================================
                // ALL CYCLES COMPLETE - WEAPON CHARGED
                // ================================================================
                qInfo() << "╔══════════════════════════════════════════════════════════════╗";
                qInfo() << "║           CHARGING COMPLETE - WEAPON READY                   ║";
                qInfo() << "║   Cycles completed:" << m_currentCycleCount << "/" << m_requiredCycles
                        << "                                   ║";
                qInfo() << "╚══════════════════════════════════════════════════════════════╝";

                // Notify GUI - weapon is now charged
                if (m_stateModel) {
                    m_stateModel->setAmmoFeedCycleInProgress(false);
                    m_stateModel->setAmmoLoaded(true);

                    // Update cycle count in state for OSD
                    SystemStateData data = m_stateModel->data();
                    data.chargeCyclesCompleted = m_currentCycleCount;
                    m_stateModel->updateData(data);
                }

                emit ammoFeedCycleCompleted();

                // ================================================================
                // CROWS M153: START 4-SECOND LOCKOUT
                // ================================================================
                startChargeLockout();
            }
        }
        break;

    // ========================================================================
    // JAM DETECTED STATE - Monitor backoff completion
    // ========================================================================
    case AmmoFeedState::JamDetected:
        if (posMM <= (FEED_RETRACT_POS + FEED_POSITION_TOLERANCE)) {
            // Backoff complete - enter fault state for operator acknowledgment
            m_feedTimer->stop();
            transitionFeedState(AmmoFeedState::Fault);

            qCritical() << "╔══════════════════════════════════════════════════════════════╗";
            qCritical() << "║     JAM RECOVERY COMPLETE - OPERATOR MUST CHECK WEAPON       ║";
            qCritical() << "╚══════════════════════════════════════════════════════════════╝";
        }
        break;

    case AmmoFeedState::Idle:
    case AmmoFeedState::Lockout:
    case AmmoFeedState::Fault:
    default:
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

void WeaponController::checkForJamCondition(const ServoActuatorData& data)
{
    // Skip if jam detection not yet initialized (first sample)
    if (!m_jamDetectionActive) {
        m_previousFeedbackPosition = data.position_mm;
        m_jamDetectionActive = true;
        m_jamDetectionCounter = 0;
        return;
    }
    
    // Calculate position delta since last sample
    double positionDelta = std::abs(data.position_mm - m_previousFeedbackPosition);
    
    // ========================================================================
    // JAM CONDITION: High torque + No movement = Mechanical obstruction
    // ========================================================================
    bool highTorque = (data.torque_percent > JAM_TORQUE_THRESHOLD_PERCENT);
    bool stalled = (positionDelta < POSITION_STALL_TOLERANCE_MM);
    
    if (highTorque && stalled) {
        m_jamDetectionCounter++;
        
        qWarning() << "[WeaponController] JAM WARNING:" 
                   << "Torque:" << QString::number(data.torque_percent, 'f', 1) << "%"
                   << "| Stalled at:" << QString::number(data.position_mm, 'f', 2) << "mm"
                   << "| Count:" << m_jamDetectionCounter << "/" << JAM_CONFIRM_SAMPLES;
        
        if (m_jamDetectionCounter >= JAM_CONFIRM_SAMPLES) {
            // CONFIRMED JAM - Execute emergency recovery
            executeJamRecovery();
        }
    } else {
        // Normal operation - reset counter
        if (m_jamDetectionCounter > 0) {
            qDebug() << "[WeaponController] Jam warning cleared - normal operation resumed";
        }
        m_jamDetectionCounter = 0;
    }
    
    // Update previous position for next comparison
    m_previousFeedbackPosition = data.position_mm;
}

void WeaponController::executeJamRecovery()
{
    qCritical() << "╔══════════════════════════════════════════════════════════════╗";
    qCritical() << "║           JAM DETECTED - EMERGENCY STOP INITIATED            ║";
    qCritical() << "╚══════════════════════════════════════════════════════════════╝";
    qCritical() << "[WeaponController] Position:" << m_previousFeedbackPosition << "mm"
                << "| State:" << feedStateName(m_feedState);
    
    // ========================================================================
    // STEP 1: IMMEDIATE STOP - Prevent further torque buildup
    // ========================================================================
    m_servoActuator->stopMove();
    m_feedTimer->stop();
    
    // ========================================================================
    // STEP 2: TRANSITION TO JAM STATE
    // ========================================================================
    transitionFeedState(AmmoFeedState::JamDetected);
    
    // ========================================================================
    // STEP 3: NOTIFY SYSTEM
    // ========================================================================
    if (m_stateModel) {
        m_stateModel->setAmmoFeedCycleInProgress(false);
    }
    emit ammoFeedCycleFaulted();
    
    // ========================================================================
    // STEP 4: DELAYED BACKOFF - Allow motor to stabilize before reverse
    // ========================================================================
    QTimer::singleShot(BACKOFF_STABILIZE_MS, this, [this]() {
        if (m_feedState != AmmoFeedState::JamDetected) {
            // State changed (operator reset?) - abort backoff
            return;
        }
        
        qDebug() << "[WeaponController] JAM RECOVERY: Initiating backoff to home position";
        m_servoActuator->moveToPosition(FEED_RETRACT_POS);
        
        // Start watchdog for backoff operation
        m_feedTimer->start(FEED_TIMEOUT_MS);
    });
    
    // Reset jam detection for next cycle
    resetJamDetection();
}

void WeaponController::resetJamDetection()
{
    m_jamDetectionCounter = 0;
    m_jamDetectionActive = false;
    m_previousFeedbackPosition = 0.0;
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
    case AmmoFeedState::Idle:        return "Idle";
    case AmmoFeedState::Extending:   return "Extending";
    case AmmoFeedState::Extended:    return "Extended";
    case AmmoFeedState::Retracting:  return "Retracting";
    case AmmoFeedState::Lockout:     return "Lockout";
    case AmmoFeedState::JamDetected: return "JamDetected";
    case AmmoFeedState::SafeRetract: return "SafeRetract";
    case AmmoFeedState::Fault:       return "Fault";
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
    // ========================================================================
    // EMERGENCY STOP CHECK (HIGHEST PRIORITY - FAIL-FAST)
    // ========================================================================
    if (m_stateModel && m_stateModel->data().emergencyStopActive) {
        qCritical() << "[WeaponController] FIRE BLOCKED: EMERGENCY STOP ACTIVE";
        return;
    }

    if (!m_systemArmed) {
        qDebug() << "[WeaponController] Cannot fire: system is not armed";
        return;
    }

    // ========================================================================
    // CROWS M153: DISABLE FIRE CIRCUIT DURING CHARGING
    // ========================================================================
    // Per CROWS spec: "During charging, the Fire Circuit is disabled"
    // This prevents accidental discharge while the cocking actuator is operating
    if (m_feedState != AmmoFeedState::Idle && m_feedState != AmmoFeedState::Lockout) {
        qWarning() << "[WeaponController] FIRE BLOCKED: Charging cycle in progress ("
                   << feedStateName(m_feedState) << "). Fire circuit disabled.";
        return;
    }

    // Safety: Prevent firing if current aim point is inside a No-Fire zone
    if (m_stateModel) {
        SystemStateData s = m_stateModel->data();
        if (m_stateModel->isPointInNoFireZone(s.gimbalAz, s.gimbalEl)) {
            qWarning() << "[WeaponController] FIRE BLOCKED: Aim point is inside a No-Fire zone. Solenoid NOT armed.";
            return;
        }

        // =====================================================================
        // CROWS/SARP LAC WORKFLOW - ENGAGE LAC ON FIRE TRIGGER
        // =====================================================================
        // Per CROWS doctrine: Lead is only applied when fire trigger is pressed.
        // If LAC is armed, engage it now to apply motion lead to CCIP.
        // =====================================================================
        /*if (s.lacArmed && !s.leadAngleCompensationActive) {
            qInfo() << "[CROWS] FIRE TRIGGER - Engaging LAC (lead now applied)";
            m_stateModel->engageLAC();
        }*/

        // =====================================================================
        // CROWS DEAD RECKONING (TM 9-1090-225-10-2 page 38)
        // =====================================================================
        // "When firing is initiated, CROWS aborts Target Tracking. Instead the
        //  system moves according to the speed and direction of the WS just
        //  prior to pulling the trigger. CROWS will not automatically compensate
        //  for changes in speed or direction of the tracked target during firing."
        // =====================================================================
        /*if (s.currentTrackingPhase == TrackingPhase::Tracking_ActiveLock ||
            s.currentTrackingPhase == TrackingPhase::Tracking_Coast) {
            qInfo() << "[CROWS] FIRING DURING TRACKING - Entering dead reckoning mode";
            m_stateModel->enterDeadReckoning(
                s.currentTargetAngularRateAz,
                s.currentTargetAngularRateEl
            );
        }*/
    }

    // All OK — send solenoid command
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
    // DEFAULT_LAC_RANGE: Used for motion lead calculation when LRF is cleared
    // but LAC is active. Professional FCS typically uses 500-1000m default.
    // This allows CCIP to function for moving targets even without LRF lock.
    // ========================================================================
    constexpr float DEFAULT_LAC_RANGE = 500.0f;  // meters

    bool applyDrop = (targetRange > 0.1f);  // Valid range threshold

    if (applyDrop) {
        LeadCalculationResult drop = m_ballisticsProcessor->calculateBallisticDrop(targetRange);

        SystemStateData updatedData = sData;
        updatedData.ballisticDropOffsetAz = drop.leadAzimuthDegrees;
        updatedData.ballisticDropOffsetEl = drop.leadElevationDegrees;
        updatedData.ballisticDropActive = true;
        m_stateModel->updateData(updatedData);
        sData = updatedData;

        /*qDebug() << "[WeaponController] AUTO DROP APPLIED:"
                 << "Range:" << targetRange << "m"
                 << "| Drop Az:" << drop.leadAzimuthDegrees << "deg (wind)"
                 << "| Drop El:" << drop.leadElevationDegrees << "deg (gravity)";*/
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

    // ========================================================================
    // BUG FIX: USE DEFAULT RANGE FOR MOTION LEAD WHEN LRF IS CLEARED
    // ========================================================================
    // Motion lead requires TOF which depends on range. When LRF is cleared:
    // - Use DEFAULT_LAC_RANGE (500m) for TOF calculation
    // - This allows CCIP to work for close-range moving targets
    // - Status will show "Lag" to indicate estimated range is used
    // ========================================================================
    float leadCalculationRange = (targetRange > 0.1f) ? targetRange : DEFAULT_LAC_RANGE;

    LeadCalculationResult lead = m_ballisticsProcessor->calculateMotionLead(
        leadCalculationRange,
        targetAngRateAz,
        targetAngRateEl,
        currentHFOV,
        currentVFOV
    );

    // If using default range (no LRF), force Lag status to indicate estimation
    if (targetRange <= 0.1f && lead.status == LeadAngleStatus::On) {
        lead.status = LeadAngleStatus::Lag;
    }

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
             << "| Range:" << leadCalculationRange << "m"
             << (targetRange <= 0.1f ? "(DEFAULT)" : "(LRF)")
             << "| Status:" << static_cast<int>(lead.status)
             << (lead.status == LeadAngleStatus::On ? "(On)" :
                 lead.status == LeadAngleStatus::Lag ? "(Lag)" :
                 lead.status == LeadAngleStatus::ZoomOut ? "(ZoomOut)" : "(Unknown)");
}

// ============================================================================
// CROWS M153 HELPER FUNCTIONS
// ============================================================================

int WeaponController::getRequiredCyclesForWeapon(WeaponType type) const
{
    switch (type) {
    case WeaponType::M2HB:
        // M2HB .50 cal - closed bolt weapon, requires 2 cycles to charge
        // Cycle 1: Pulls bolt to rear, picks up first round from belt
        // Cycle 2: Chambers the round, prepares weapon to fire
        return 2;

    case WeaponType::M240B:
    case WeaponType::M249:
    case WeaponType::MK19:
    case WeaponType::Unknown:
    default:
        // Open bolt weapons and grenade launchers require only 1 cycle
        return 1;
    }
}

void WeaponController::startChargeLockout()
{
    qInfo() << "[WeaponController] CROWS M153: Starting 4-second charge lockout";

    m_chargeLockoutActive = true;
    transitionFeedState(AmmoFeedState::Lockout);

    // Update state model for GUI feedback
    if (m_stateModel) {
        SystemStateData data = m_stateModel->data();
        data.chargeLockoutActive = true;
        m_stateModel->updateData(data);
    }

    // Start 4-second lockout timer
    m_lockoutTimer->start(CHARGE_LOCKOUT_MS);
}

void WeaponController::onChargeLockoutExpired()
{
    qInfo() << "[WeaponController] CROWS M153: 4-second lockout expired - charging now allowed";

    m_chargeLockoutActive = false;
    transitionFeedState(AmmoFeedState::Idle);

    // Update state model for GUI feedback
    if (m_stateModel) {
        SystemStateData data = m_stateModel->data();
        data.chargeLockoutActive = false;
        m_stateModel->updateData(data);
    }
}

void WeaponController::performStartupRetraction()
{
    if (!m_servoActuator) {
        return;
    }

    // Check current actuator position
    // If extended beyond threshold, automatically retract
    double currentPos = 0.0;
    if (m_stateModel) {
        currentPos = m_stateModel->data().actuatorPosition;
    }

    if (currentPos > ACTUATOR_RETRACTED_THRESHOLD) {
        qInfo() << "╔══════════════════════════════════════════════════════════════╗";
        qInfo() << "║  CROWS M153: STARTUP - ACTUATOR EXTENDED, AUTO-RETRACTING    ║";
        qInfo() << "║  Current position:" << currentPos << "mm                      ║";
        qInfo() << "╚══════════════════════════════════════════════════════════════╝";

        // Command retraction to home position
        m_servoActuator->moveToPosition(FEED_RETRACT_POS);

        // Set state to retracting but don't start watchdog timer for startup
        // (startup retraction is best-effort, not critical)
        transitionFeedState(AmmoFeedState::Retracting);

        if (m_stateModel) {
            m_stateModel->setAmmoFeedCycleInProgress(true);
        }

        // Start a shorter timeout for startup retraction
        m_feedTimer->start(FEED_TIMEOUT_MS / 2);
    } else {
        qDebug() << "[WeaponController] CROWS M153: Actuator already retracted at startup ("
                 << currentPos << "mm)";
    }
}

bool WeaponController::isChargingAllowed() const
{
    // Charging not allowed if:
    // 1. Lockout is active
    // 2. Cycle already in progress
    // 3. System is in fault state
    if (m_chargeLockoutActive) {
        return false;
    }
    if (m_feedState != AmmoFeedState::Idle) {
        return false;
    }
    return true;
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