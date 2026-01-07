/**
 * @file homingcontroller.cpp
 * @brief Implementation of homing sequence controller
 */

#include "homingcontroller.h"
#include "hardware/devices/plc42device.h"
#include <QDebug>

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

HomingController::HomingController(Plc42Device* plc42, QObject* parent)
    : QObject(parent), m_plc42(plc42) {
    // Initialize timeout timer
    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setSingleShot(true);
    connect(m_timeoutTimer, &QTimer::timeout, this, &HomingController::onHomingTimeout);

    qDebug() << "[HomingController] Initialized (timeout:" << m_homingTimeoutMs << "ms)";
}

HomingController::~HomingController() {
    if (m_timeoutTimer) {
        m_timeoutTimer->stop();
    }
    qDebug() << "[HomingController] Destroyed";
}

// ============================================================================
// PUBLIC INTERFACE
// ============================================================================

void HomingController::process(const SystemStateData& newData, const SystemStateData& oldData) {
    switch (newData.homingState) {
    case HomingState::Idle:
        processIdle(newData, oldData);
        break;

    case HomingState::Requested:
        processRequested(newData);
        break;

    case HomingState::InProgress:
        processInProgress(newData, oldData);
        break;

    case HomingState::Completed:
        // ⭐ BUG FIX: SystemStateModel may have transitioned to Completed before
        // HomingController had a chance to see the HOME-END signals in InProgress.
        // If we're still in InProgress, complete our own state machine now.
        if (m_currentHomingState == HomingState::InProgress) {
            qDebug() << "[HomingController] SystemStateModel completed homing - syncing our state";
            completeHoming();  // Stop timer, emit homingCompleted signal
        }
        // Auto-clear completed state after one cycle
        if (m_currentHomingState == HomingState::Completed) {
            qDebug() << "[HomingController] Clearing completed state";
            transitionTo(HomingState::Idle);
        }
        break;

    case HomingState::Failed:
    case HomingState::Aborted:
        // Auto-clear failed/aborted state after one cycle
        if (m_currentHomingState != HomingState::Idle) {
            qDebug() << "[HomingController] Clearing failed/aborted state";
            transitionTo(HomingState::Idle);
        }
        break;
    }
}

void HomingController::start(MotionMode currentMotionMode) {
    if (m_currentHomingState != HomingState::Idle) {
        qWarning() << "[HomingController] Cannot start - homing already in progress";
        return;
    }

    // Store current mode for restoration
    m_modeBeforeHoming = currentMotionMode;
    qDebug() << "[HomingController] Stored mode before homing:"
             << static_cast<int>(m_modeBeforeHoming);

    // Transition to Requested
    transitionTo(HomingState::Requested);

    qInfo() << "[HomingController] Homing sequence initiated";
    qInfo() << "[HomingController] -> Will send HOME command to Oriental Motor";

    emit homingStarted();
}

void HomingController::abort(const QString& reason) {
    if (m_currentHomingState == HomingState::Idle) {
        qDebug() << "[HomingController] Nothing to abort - not homing";
        return;
    }

    // Stop timeout timer
    m_timeoutTimer->stop();

    // Return PLC42 to manual mode
    returnToManualMode();

    // Transition to Aborted
    transitionTo(HomingState::Aborted);

    qCritical() << "[HomingController] HOME sequence aborted:" << reason;
    qWarning() << "[HomingController] Gimbal position may be uncertain";
    qWarning() << "[HomingController] Will restore motion mode:"
               << static_cast<int>(m_modeBeforeHoming);

    emit homingAborted(reason);
}

bool HomingController::isHomingInProgress() const {
    return (m_currentHomingState == HomingState::Requested ||
            m_currentHomingState == HomingState::InProgress);
}

void HomingController::setHomingTimeout(int timeoutMs) {
    m_homingTimeoutMs = timeoutMs;
    qDebug() << "[HomingController] Timeout set to" << timeoutMs << "ms";
}

// ============================================================================
// PRIVATE SLOTS
// ============================================================================

void HomingController::onHomingTimeout() {
    qCritical() << "[HomingController] HOME sequence TIMEOUT after" << m_homingTimeoutMs << "ms";
    qCritical() << "[HomingController] HOME-END signal was NOT received";
    qCritical() << "[HomingController] Possible causes:";
    qCritical() << "[HomingController]   - Wiring issue (I0_7 not connected)";
    qCritical() << "[HomingController]   - Oriental Motor fault";
    qCritical() << "[HomingController]   - Mechanical obstruction";

    failHoming("Timeout - HOME-END not received");
}

// ============================================================================
// FSM PROCESSING
// ============================================================================

void HomingController::processIdle(const SystemStateData& newData, const SystemStateData& oldData) {
    // Check for home button press (rising edge detection)
    if (newData.gotoHomePosition && !oldData.gotoHomePosition) {
        qInfo() << "[HomingController] Home button pressed";
        start(newData.motionMode);
    }
}

void HomingController::processRequested(const SystemStateData& newData) {
    // This state is processed once to send HOME command
    if (m_currentHomingState == HomingState::Requested) {
        // Already processed - wait for state model to transition
        return;
    }

    // Send HOME command to PLC42
    if (m_plc42 && m_plc42->data()->isConnected) {
        qInfo() << "[HomingController] Starting HOME sequence";

        // Store current mode to restore after homing completes
        m_modeBeforeHoming = newData.motionMode;
        qDebug() << "[HomingController] Stored current mode:"
                 << static_cast<int>(m_modeBeforeHoming);

        // Send HOME command to Oriental Motor via PLC42
        m_plc42->setHomePosition();  // Sets gimbalOpMode = 3 -> Q1_1 HIGH (ZHOME)

        // Transition to InProgress
        transitionTo(HomingState::InProgress);

        // Start timeout timer
        m_timeoutTimer->start(m_homingTimeoutMs);

        qInfo() << "[HomingController] HOME command sent, waiting for HOME-END signals...";
    } else {
        qCritical() << "[HomingController] Cannot start homing - PLC42 not connected";
        failHoming("PLC42 not connected");
    }
}

void HomingController::processInProgress(const SystemStateData& newData,
                                         const SystemStateData& oldData) {
    // Check if BOTH HOME-END signals received
    bool azHomeDone = newData.azimuthHomeComplete;
    bool elHomeDone = newData.elevationHomeComplete;

    // Log individual axis completion (rising edge detection)
    if (azHomeDone && !oldData.azimuthHomeComplete) {
        qInfo() << "[HomingController] Azimuth axis homed (HOME-END received)";
    }
    if (elHomeDone && !oldData.elevationHomeComplete) {
        qInfo() << "[HomingController] Elevation axis homed (HOME-END received)";
    }

    // Complete homing only when BOTH axes report HOME-END
    if (azHomeDone && elHomeDone &&
        (!oldData.azimuthHomeComplete || !oldData.elevationHomeComplete)) {
        qInfo() << "[HomingController] BOTH axes at home position";
        completeHoming();
    }

    // Check for emergency stop during homing
    if (newData.emergencyStopActive) {
        qWarning() << "[HomingController] Homing aborted by emergency stop";
        abort("Emergency stop activated");
    }
}

void HomingController::completeHoming() {
    // Stop timeout timer
    m_timeoutTimer->stop();

    // Return PLC42 to manual mode
    returnToManualMode();

    // Transition to Completed
    transitionTo(HomingState::Completed);

    qInfo() << "[HomingController] HOME sequence completed successfully";
    qInfo() << "[HomingController] Gimbal at home position, ready for operation";
    qInfo() << "[HomingController] Will restore motion mode:"
            << static_cast<int>(m_modeBeforeHoming);

    emit homingCompleted();
}

void HomingController::failHoming(const QString& reason) {
    // Stop timeout timer
    m_timeoutTimer->stop();

    // Return PLC42 to manual mode
    returnToManualMode();

    // Transition to Failed
    transitionTo(HomingState::Failed);

    emit homingFailed(reason);
}

void HomingController::returnToManualMode() {
    if (m_plc42) {
        m_plc42->setManualMode();  // Clears HOME output (Q1_1 LOW)
        qDebug() << "[HomingController] PLC42 returned to MANUAL mode";
    }
}

void HomingController::transitionTo(HomingState newState) {
    if (m_currentHomingState == newState) {
        return;
    }

    HomingState oldState = m_currentHomingState;
    m_currentHomingState = newState;

    qDebug() << "[HomingController] State transition:" << static_cast<int>(oldState) << "->"
             << static_cast<int>(newState);

    emit homingStateChanged(newState);
}
