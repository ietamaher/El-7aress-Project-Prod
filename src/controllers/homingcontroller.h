#ifndef HOMINGCONTROLLER_H
#define HOMINGCONTROLLER_H

/**
 * @file homingcontroller.h
 * @brief Homing sequence controller extracted from GimbalController
 *
 * This class manages the gimbal homing state machine:
 * - HOME button press detection
 * - Oriental Motor HOME command (via PLC42)
 * - HOME-END signal monitoring for both axes
 * - Timeout supervision
 * - Emergency stop handling during homing
 *
 * DESIGN:
 * - Clear FSM with explicit states
 * - Signal-based completion notification
 * - Timeout protection (default 30 seconds)
 * - Stores previous motion mode for restoration
 *
 * @date 2025-12-30
 * @version 1.0
 */

#include <QObject>
#include <QTimer>
#include "models/domain/systemstatedata.h"

// Forward declarations
class Plc42Device;

/**
 * @class HomingController
 * @brief Manages gimbal homing sequence FSM
 *
 * Extracted from GimbalController for:
 * - Improved testability
 * - Reduced GimbalController complexity
 * - Clear separation of concerns
 *
 * USAGE:
 * @code
 * HomingController homing(plc42, this);
 * connect(&homing, &HomingController::homingCompleted,
 *         this, &GimbalController::onHomingCompleted);
 * connect(&homing, &HomingController::homingFailed,
 *         this, &GimbalController::onHomingFailed);
 *
 * // In state change handler:
 * homing.process(newData, oldData);
 * @endcode
 */
class HomingController : public QObject
{
    Q_OBJECT

public:
    // ========================================================================
    // CONFIGURATION
    // ========================================================================

    /**
     * @brief Default homing timeout in milliseconds
     *
     * Oriental Motor homing typically completes in 5-15 seconds.
     * 30 seconds allows margin for slow movements or mechanical issues.
     */
    static constexpr int DEFAULT_HOMING_TIMEOUT_MS = 30000;

    // ========================================================================
    // CONSTRUCTOR / DESTRUCTOR
    // ========================================================================

    /**
     * @brief Construct HomingController
     * @param plc42 PLC42 device for sending HOME commands
     * @param parent Parent QObject
     */
    explicit HomingController(Plc42Device* plc42, QObject* parent = nullptr);
    ~HomingController();

    // ========================================================================
    // PUBLIC INTERFACE
    // ========================================================================

    /**
     * @brief Process homing state machine
     *
     * Call this on every system state change to update homing FSM.
     *
     * @param newData Current system state
     * @param oldData Previous system state (for edge detection)
     */
    void process(const SystemStateData& newData, const SystemStateData& oldData);

    /**
     * @brief Start homing sequence (programmatic trigger)
     *
     * This can be called directly instead of waiting for button press.
     * Stores current motion mode for restoration after homing.
     *
     * @param currentMotionMode Current motion mode to restore after homing
     */
    void start(MotionMode currentMotionMode);

    /**
     * @brief Abort homing sequence
     * @param reason Reason for abort (for logging/signals)
     */
    void abort(const QString& reason);

    /**
     * @brief Check if homing is currently in progress
     * @return true if homing is Requested or InProgress
     */
    bool isHomingInProgress() const;

    /**
     * @brief Get current homing state
     * @return Current HomingState
     */
    HomingState currentState() const { return m_currentHomingState; }

    /**
     * @brief Get motion mode to restore after homing
     * @return Motion mode that was active before homing started
     */
    MotionMode modeBeforeHoming() const { return m_modeBeforeHoming; }

    /**
     * @brief Set homing timeout
     * @param timeoutMs Timeout in milliseconds
     */
    void setHomingTimeout(int timeoutMs);

signals:
    // ========================================================================
    // SIGNALS
    // ========================================================================

    /**
     * @brief Emitted when homing sequence starts
     */
    void homingStarted();

    /**
     * @brief Emitted when homing completes successfully
     *
     * GimbalController should restore motion mode and update state.
     */
    void homingCompleted();

    /**
     * @brief Emitted when homing fails (timeout)
     * @param reason Description of failure
     */
    void homingFailed(const QString& reason);

    /**
     * @brief Emitted when homing is aborted (e.g., by emergency stop)
     * @param reason Description of abort reason
     */
    void homingAborted(const QString& reason);

    /**
     * @brief Emitted when homing state changes
     * @param newState New homing state
     */
    void homingStateChanged(HomingState newState);

private slots:
    /**
     * @brief Handle homing timeout
     */
    void onHomingTimeout();

private:
    // ========================================================================
    // FSM METHODS
    // ========================================================================

    /**
     * @brief Process HomingState::Idle
     */
    void processIdle(const SystemStateData& newData, const SystemStateData& oldData);

    /**
     * @brief Process HomingState::Requested
     */
    void processRequested(const SystemStateData& newData);

    /**
     * @brief Process HomingState::InProgress
     */
    void processInProgress(const SystemStateData& newData, const SystemStateData& oldData);

    /**
     * @brief Complete homing sequence successfully
     */
    void completeHoming();

    /**
     * @brief Fail homing sequence (timeout)
     */
    void failHoming(const QString& reason);

    /**
     * @brief Return PLC42 to manual mode
     */
    void returnToManualMode();

    /**
     * @brief Transition to new homing state
     * @param newState New homing state
     */
    void transitionTo(HomingState newState);

    // ========================================================================
    // DEPENDENCIES
    // ========================================================================
    Plc42Device* m_plc42 = nullptr;

    // ========================================================================
    // STATE
    // ========================================================================
    HomingState m_currentHomingState = HomingState::Idle;
    MotionMode m_modeBeforeHoming = MotionMode::Manual;

    // ========================================================================
    // TIMERS
    // ========================================================================
    QTimer* m_timeoutTimer = nullptr;
    int m_homingTimeoutMs = DEFAULT_HOMING_TIMEOUT_MS;
};

#endif // HOMINGCONTROLLER_H
