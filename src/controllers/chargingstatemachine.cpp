/**
 * @file chargingstatemachine.cpp
 * @brief Implementation of charging (cocking actuator) state machine
 */

#include "chargingstatemachine.h"
#include "hardware/devices/servoactuatordevice.h"
#include "safety/SafetyInterlock.h"
#include <QDebug>
#include <cmath>

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

ChargingStateMachine::ChargingStateMachine(ServoActuatorDevice* actuator,
                                           SafetyInterlock* safetyInterlock, QObject* parent)
    : QObject(parent), m_actuator(actuator), m_safetyInterlock(safetyInterlock) {
    // Initialize timeout timer
    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setSingleShot(true);
    connect(m_timeoutTimer, &QTimer::timeout, this, &ChargingStateMachine::onChargingTimeout);

    // Initialize lockout timer (CROWS M153 4-second post-charge lockout)
    m_lockoutTimer = new QTimer(this);
    m_lockoutTimer->setSingleShot(true);
    connect(m_lockoutTimer, &QTimer::timeout, this, &ChargingStateMachine::onLockoutExpired);

    qDebug() << "[ChargingStateMachine] Initialized (timeout:" << COCKING_TIMEOUT_MS << "ms)";
}

ChargingStateMachine::~ChargingStateMachine() {
    if (m_timeoutTimer) {
        m_timeoutTimer->stop();
    }
    if (m_lockoutTimer) {
        m_lockoutTimer->stop();
    }
    qDebug() << "[ChargingStateMachine] Destroyed";
}

// ============================================================================
// PUBLIC INTERFACE
// ============================================================================

bool ChargingStateMachine::requestCharge(WeaponType weaponType) {
    qDebug() << "[ChargingStateMachine] Charge request. Current state:" << stateName(m_currentState)
             << "| Lockout active:" << m_lockoutActive;

    // Handle FAULT state specially - operator presses button to reset
    if (m_currentState == ChargingState::Fault || m_currentState == ChargingState::JamDetected) {
        qDebug() << "[ChargingStateMachine] Button pressed in FAULT state - resetting fault";
        resetFault();
        return true;
    }

    // Check if charging is allowed via SafetyInterlock
    if (!isChargingAllowed()) {
        // Denial is already logged by SafetyInterlock with timestamp
        return false;
    }

    // Only allow start when idle
    if (m_currentState != ChargingState::Idle) {
        qDebug() << "[ChargingStateMachine] Charge request IGNORED - cycle already running";
        return false;
    }

    // Initialize multi-cycle charging
    m_currentCycleCount = 0;
    m_requiredCycles = getRequiredCyclesForWeapon(weaponType);
    m_buttonCurrentlyHeld = true;  // Assume button is held when request comes in

    qInfo() << "[ChargingStateMachine] Starting charge sequence:" << m_requiredCycles
            << "cycle(s) required for weapon type" << static_cast<int>(weaponType);

    startCycle();
    return true;
}

void ChargingStateMachine::onButtonReleased() {
    m_buttonCurrentlyHeld = false;

    if (m_currentState == ChargingState::Extended) {
        // CONTINUOUS HOLD MODE: Button released while in Extended state
        // This counts as ONE complete cycle
        m_currentCycleCount++;
        m_isShortPressCharge = false;  // Mark as continuous hold mode

        qInfo()
            << "[ChargingStateMachine] Button RELEASED in Extended state - initiating retraction"
            << "(Cycle" << m_currentCycleCount << "of" << m_requiredCycles << ")";

        transitionTo(ChargingState::Retracting);
        emit requestActuatorMove(COCKING_RETRACT_POS);
        m_timeoutTimer->start(COCKING_TIMEOUT_MS);
    } else if (m_currentState == ChargingState::Extending ||
               m_currentState == ChargingState::Retracting) {
        // Button released during extend/retract motion - logged but cycle continues
        qDebug() << "[ChargingStateMachine] Button released during" << stateName(m_currentState)
                 << "- cycle continues";
    }
}

void ChargingStateMachine::abort(const QString& reason) {
    if (m_currentState == ChargingState::Idle || m_currentState == ChargingState::Lockout) {
        qDebug() << "[ChargingStateMachine] Nothing to abort - not charging";
        return;
    }

    // Stop timers
    m_timeoutTimer->stop();

    // Stop actuator immediately
    emit requestActuatorStop();

    // Transition to Fault
    transitionTo(ChargingState::Fault);

    qCritical() << "[ChargingStateMachine] Charge cycle ABORTED:" << reason;

    // Reset multi-cycle state to prevent auto-restart
    m_isShortPressCharge = false;
    m_currentCycleCount = 0;

    emit cycleFaulted();
}

void ChargingStateMachine::resetFault() {
    if (m_currentState != ChargingState::Fault && m_currentState != ChargingState::JamDetected) {
        qDebug() << "[ChargingStateMachine] Fault reset IGNORED - not in FAULT state";
        return;
    }

    qDebug() << "[ChargingStateMachine] Operator reset - attempting safe retraction";

    // Reset multi-cycle state
    m_isShortPressCharge = false;
    m_currentCycleCount = 0;

    // Use SafeRetract state for recovery (goes directly to Idle, no auto-cycle)
    transitionTo(ChargingState::SafeRetract);

    // Attempt safe retraction
    emit requestActuatorMove(COCKING_RETRACT_POS);
    m_timeoutTimer->start(COCKING_TIMEOUT_MS);
}

void ChargingStateMachine::processActuatorFeedback(const ServoActuatorData& data) {
    // Jam detection during motion states
    if (m_currentState == ChargingState::Extending || m_currentState == ChargingState::Retracting ||
        m_currentState == ChargingState::SafeRetract) {
        checkForJam(data);
    }

    // Process position for FSM state transitions
    processPosition(data.position_mm);
}

void ChargingStateMachine::performStartupRetraction(double currentPosition) {
    if (currentPosition > ACTUATOR_RETRACTED_THRESHOLD) {
        qInfo() << "========================================";
        qInfo() << "  CROWS M153: STARTUP - ACTUATOR EXTENDED, AUTO-RETRACTING";
        qInfo() << "  Current position:" << currentPosition << "mm";
        qInfo() << "========================================";

        // Command retraction to home position
        emit requestActuatorMove(COCKING_RETRACT_POS);

        // Set state to retracting
        transitionTo(ChargingState::Retracting);

        // Start a shorter timeout for startup retraction (best-effort)
        m_timeoutTimer->start(COCKING_TIMEOUT_MS / 2);
    } else {
        qDebug() << "[ChargingStateMachine] Actuator already retracted at startup ("
                 << currentPosition << "mm)";
    }
}

// ============================================================================
// STATE QUERIES
// ============================================================================

bool ChargingStateMachine::isChargingInProgress() const {
    return (m_currentState == ChargingState::Extending ||
            m_currentState == ChargingState::Extended ||
            m_currentState == ChargingState::Retracting ||
            m_currentState == ChargingState::SafeRetract ||
            m_currentState == ChargingState::JamDetected);
}

bool ChargingStateMachine::isChargingAllowed() const {
    // Delegate safety decision to SafetyInterlock
    if (m_safetyInterlock) {
        SafetyDenialReason reason;
        bool allowed = m_safetyInterlock->canCharge(&reason);
        if (!allowed) {
            qDebug() << "[ChargingStateMachine] Charging denied by SafetyInterlock:"
                     << SafetyInterlock::denialReasonToString(reason);
        }
        return allowed;
    }

    // Legacy fallback if SafetyInterlock not available
    if (m_lockoutActive) {
        return false;
    }
    if (m_currentState != ChargingState::Idle) {
        return false;
    }
    return true;
}

QString ChargingStateMachine::stateName(ChargingState state) {
    switch (state) {
    case ChargingState::Idle:
        return "Idle";
    case ChargingState::Extending:
        return "Extending";
    case ChargingState::Extended:
        return "Extended";
    case ChargingState::Retracting:
        return "Retracting";
    case ChargingState::Lockout:
        return "Lockout";
    case ChargingState::JamDetected:
        return "JamDetected";
    case ChargingState::SafeRetract:
        return "SafeRetract";
    case ChargingState::Fault:
        return "Fault";
    }
    return "Unknown";
}

// ============================================================================
// PRIVATE SLOTS
// ============================================================================

void ChargingStateMachine::onChargingTimeout() {
    qWarning() << "[ChargingStateMachine] CHARGING TIMEOUT in state:" << stateName(m_currentState)
               << "- actuator did not reach expected position within" << COCKING_TIMEOUT_MS << "ms";

    // Enter fault state
    transitionTo(ChargingState::Fault);

    emit cycleFaulted();

    // NOTE: We do NOT automatically retract - operator must clear jam first
    // and then use resetFault() to attempt recovery
}

void ChargingStateMachine::onLockoutExpired() {
    qInfo() << "[ChargingStateMachine] CROWS M153: 4-second lockout expired - charging now allowed";

    m_lockoutActive = false;
    transitionTo(ChargingState::Idle);

    emit lockoutExpired();
}

// ============================================================================
// FSM METHODS
// ============================================================================

void ChargingStateMachine::startCycle() {
    qInfo() << "========================================";
    qInfo() << "  CHARGE CYCLE STARTING";
    qInfo() << "  Cycle:" << (m_currentCycleCount + 1) << "of" << m_requiredCycles;
    qInfo() << "========================================";

    // Reset jam detection for new cycle
    resetJamDetection();

    transitionTo(ChargingState::Extending);

    emit cycleStarted();

    // Command full extension
    emit requestActuatorMove(COCKING_EXTEND_POS);

    // Start watchdog
    m_timeoutTimer->start(COCKING_TIMEOUT_MS);
}

void ChargingStateMachine::processPosition(double positionMM) {
    switch (m_currentState) {
    case ChargingState::Extending:
        if (positionMM >= (COCKING_EXTEND_POS - COCKING_POSITION_TOLERANCE)) {
            // Extension reached
            m_timeoutTimer->stop();

            if (m_buttonCurrentlyHeld) {
                // CONTINUOUS HOLD MODE: Button still held - hold position
                m_isShortPressCharge = false;
                transitionTo(ChargingState::Extended);
                qDebug()
                    << "[ChargingStateMachine] Extension complete - HOLDING (button still pressed)";
                // No watchdog in Extended state - operator controls timing
            } else {
                // SHORT PRESS MODE: Button already released - auto-cycle
                m_isShortPressCharge = true;
                m_currentCycleCount++;

                qInfo() << "[ChargingStateMachine] Cycle" << m_currentCycleCount << "of"
                        << m_requiredCycles << "complete - retracting";

                // Retract after extension
                transitionTo(ChargingState::Retracting);
                emit requestActuatorMove(COCKING_RETRACT_POS);
                m_timeoutTimer->start(COCKING_TIMEOUT_MS);
            }
        }
        break;

    case ChargingState::Extended:
        // In Extended state, we just hold position
        // Transition to Retracting happens via onButtonReleased()
        break;

    case ChargingState::Retracting:
        if (positionMM <= (COCKING_RETRACT_POS + COCKING_POSITION_TOLERANCE)) {
            m_timeoutTimer->stop();

            // Check if more cycles needed (SHORT PRESS MODE ONLY)
            if (m_isShortPressCharge && m_currentCycleCount < m_requiredCycles) {
                // More cycles needed - start next extension
                qInfo() << "[ChargingStateMachine] Starting cycle" << (m_currentCycleCount + 1)
                        << "of" << m_requiredCycles;

                transitionTo(ChargingState::Extending);
                emit requestActuatorMove(COCKING_EXTEND_POS);
                m_timeoutTimer->start(COCKING_TIMEOUT_MS);

                // Reset jam detection for new cycle
                resetJamDetection();
            } else {
                // ALL CYCLES COMPLETE - WEAPON CHARGED
                qInfo() << "========================================";
                qInfo() << "  CHARGING COMPLETE - WEAPON READY";
                qInfo() << "  Cycles completed:" << m_currentCycleCount << "/" << m_requiredCycles;
                qInfo() << "========================================";

                emit cycleCompleted();

                // Start 4-second lockout
                startLockout();
            }
        }
        break;

    case ChargingState::JamDetected:
        if (positionMM <= (COCKING_RETRACT_POS + COCKING_POSITION_TOLERANCE)) {
            // Backoff complete - enter fault state for operator acknowledgment
            m_timeoutTimer->stop();
            transitionTo(ChargingState::Fault);

            qCritical() << "========================================";
            qCritical() << "  JAM RECOVERY COMPLETE - OPERATOR MUST CHECK WEAPON";
            qCritical() << "========================================";
        }
        break;

    case ChargingState::SafeRetract:
        if (positionMM <= (COCKING_RETRACT_POS + COCKING_POSITION_TOLERANCE)) {
            m_timeoutTimer->stop();

            qInfo() << "========================================";
            qInfo() << "  SAFE RETRACTION COMPLETE - ACTUATOR AT HOME";
            qInfo() << "========================================";

            // Go directly to Idle - NO automatic charging cycle
            transitionTo(ChargingState::Idle);

            qInfo() << "[ChargingStateMachine] System ready - operator must manually charge weapon";
        }
        break;

    case ChargingState::Idle:
    case ChargingState::Lockout:
    case ChargingState::Fault:
    default:
        break;
    }
}

void ChargingStateMachine::transitionTo(ChargingState newState) {
    if (m_currentState == newState) {
        return;
    }

    qDebug() << "[ChargingStateMachine] State transition:" << stateName(m_currentState) << "->"
             << stateName(newState);

    m_currentState = newState;
    emit stateChanged(newState);
}

int ChargingStateMachine::getRequiredCyclesForWeapon(WeaponType type) {
    switch (type) {
    case WeaponType::M2HB:
        // M2HB .50 cal - closed bolt weapon, requires 2 cycles
        // Cycle 1: Pulls bolt to rear, picks up first round
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

void ChargingStateMachine::startLockout() {
    qInfo() << "[ChargingStateMachine] CROWS M153: Starting 4-second charge lockout";

    m_lockoutActive = true;
    transitionTo(ChargingState::Lockout);

    // Start 4-second lockout timer
    m_lockoutTimer->start(CHARGE_LOCKOUT_MS);
}

// ============================================================================
// JAM DETECTION
// ============================================================================

void ChargingStateMachine::checkForJam(const ServoActuatorData& data) {
    // Skip if jam detection not yet initialized (first sample)
    if (!m_jamDetectionActive) {
        m_previousFeedbackPosition = data.position_mm;
        m_jamDetectionActive = true;
        m_jamDetectionCounter = 0;
        return;
    }

    // Calculate position delta since last sample
    double positionDelta = std::abs(data.position_mm - m_previousFeedbackPosition);

    // JAM CONDITION: High torque + No movement = Mechanical obstruction
    bool highTorque = (data.torque_percent > JAM_TORQUE_THRESHOLD_PERCENT);
    bool stalled = (positionDelta < POSITION_STALL_TOLERANCE_MM);

    if (highTorque && stalled) {
        m_jamDetectionCounter++;

        qWarning() << "[ChargingStateMachine] JAM WARNING:" << "Torque:"
                   << QString::number(data.torque_percent, 'f', 1) << "%"
                   << "| Stalled at:" << QString::number(data.position_mm, 'f', 2) << "mm"
                   << "| Count:" << m_jamDetectionCounter << "/" << JAM_CONFIRM_SAMPLES;

        if (m_jamDetectionCounter >= JAM_CONFIRM_SAMPLES) {
            // CONFIRMED JAM - Execute emergency recovery
            executeJamRecovery();
        }
    } else {
        // Normal operation - reset counter
        if (m_jamDetectionCounter > 0) {
            qDebug() << "[ChargingStateMachine] Jam warning cleared - normal operation resumed";
        }
        m_jamDetectionCounter = 0;
    }

    // Update previous position for next comparison
    m_previousFeedbackPosition = data.position_mm;
}

void ChargingStateMachine::executeJamRecovery() {
    qCritical() << "========================================";
    qCritical() << "  JAM DETECTED - EMERGENCY STOP INITIATED";
    qCritical() << "========================================";
    qCritical() << "[ChargingStateMachine] Position:" << m_previousFeedbackPosition << "mm"
                << "| State:" << stateName(m_currentState);

    // STEP 1: IMMEDIATE STOP - Prevent further torque buildup
    emit requestActuatorStop();
    m_timeoutTimer->stop();

    // STEP 2: TRANSITION TO JAM STATE
    transitionTo(ChargingState::JamDetected);

    // STEP 3: NOTIFY
    emit jamDetected();
    emit cycleFaulted();

    // STEP 4: DELAYED BACKOFF - Allow motor to stabilize before reverse
    QTimer::singleShot(BACKOFF_STABILIZE_MS, this, [this]() {
        if (m_currentState != ChargingState::JamDetected) {
            // State changed (operator reset?) - abort backoff
            return;
        }

        qDebug() << "[ChargingStateMachine] JAM RECOVERY: Initiating backoff to home position";
        emit requestActuatorMove(COCKING_RETRACT_POS);

        // Start watchdog for backoff operation
        m_timeoutTimer->start(COCKING_TIMEOUT_MS);
    });

    // Reset jam detection for next cycle
    resetJamDetection();
}

void ChargingStateMachine::resetJamDetection() {
    m_jamDetectionCounter = 0;
    m_jamDetectionActive = false;
    m_previousFeedbackPosition = 0.0;
}
