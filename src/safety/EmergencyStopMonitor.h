#pragma once

/**
 * @file EmergencyStopMonitor.h
 * @brief Centralized emergency stop monitoring and response coordination
 *
 * This class provides a single authority for emergency stop state management:
 * - Monitors emergency stop signal from hardware (PLC42)
 * - Provides deterministic edge detection (activation/deactivation)
 * - Emits signals for coordinated system response
 * - Tracks timing for audit/logging purposes
 * - Enforces minimum activation duration (debounce)
 *
 * DESIGN PRINCIPLES:
 * - Single source of truth for E-stop state
 * - Deterministic behavior (no race conditions)
 * - Audit trail with timestamps
 * - Fail-safe defaults (assume active on communication loss)
 *
 * SAFETY HIERARCHY (per MIL-STD / CROWS):
 * 1. Emergency Stop - HIGHEST PRIORITY
 * 2. Hardware interlocks (limit switches, etc.)
 * 3. Software safety zones
 * 4. Operator controls
 *
 * USAGE:
 * @code
 * EmergencyStopMonitor eStopMonitor;
 * connect(&eStopMonitor, &EmergencyStopMonitor::activated,
 *         this, &MyController::onEmergencyStopActivated);
 * connect(&eStopMonitor, &EmergencyStopMonitor::deactivated,
 *         this, &MyController::onEmergencyStopCleared);
 *
 * // In state update handler:
 * eStopMonitor.updateState(newData.emergencyStopActive);
 * @endcode
 *
 * @date 2025-12-31
 * @version 1.0
 */

// ============================================================================
// INCLUDES
// ============================================================================
#include <QObject>
#include <QDateTime>
#include <QElapsedTimer>

#include <vector>

/**
 * @brief Emergency stop event information
 */
struct EmergencyStopEvent {
    QDateTime timestamp;         ///< When the event occurred
    bool wasActivation = false;  ///< true = activated, false = deactivated
    qint64 durationMs = 0;       ///< Duration of previous state (ms)
    QString source;              ///< Source identifier (for multi-source systems)
};

/**
 * @class EmergencyStopMonitor
 * @brief Centralized emergency stop state management
 *
 * Provides:
 * - Deterministic edge detection
 * - Debounce filtering (prevents spurious triggers)
 * - Activation/deactivation timestamps
 * - Event history for audit
 * - Recovery coordination signals
 */
class EmergencyStopMonitor : public QObject {
    Q_OBJECT

public:
    // ============================================================================
    // CONFIGURATION CONSTANTS
    // ============================================================================

    /**
     * @brief Minimum activation duration to consider valid (debounce)
     * Prevents spurious triggers from electrical noise
     */
    static constexpr int DEBOUNCE_MS = 50;

    /**
     * @brief Minimum time before allowing recovery after deactivation
     * Ensures system has time to stabilize
     */
    static constexpr int RECOVERY_DELAY_MS = 500;

    /**
     * @brief Maximum events to keep in history
     */
    static constexpr int MAX_EVENT_HISTORY = 100;

    // ============================================================================
    // CONSTRUCTOR / DESTRUCTOR
    // ============================================================================

    explicit EmergencyStopMonitor(QObject* parent = nullptr);
    ~EmergencyStopMonitor() = default;

    // ============================================================================
    // STATE UPDATE
    // ============================================================================

    /**
     * @brief Update emergency stop state from hardware
     *
     * Call this whenever the emergency stop signal changes.
     * Handles edge detection and debounce internally.
     *
     * @param isActive Current state of emergency stop (true = active)
     * @param source Optional source identifier for multi-source systems
     */
    void updateState(bool isActive, const QString& source = "PLC42");

    /**
     * @brief Force emergency stop state (for software-triggered stops)
     *
     * Use this for software-initiated emergency stops (e.g., safety zone violation).
     * Bypasses debounce since it's a deliberate action.
     *
     * @param reason Reason for the forced stop
     */
    void forceActivate(const QString& reason);

    // ============================================================================
    // STATE QUERIES
    // ============================================================================

    /**
     * @brief Check if emergency stop is currently active
     * @return true if emergency stop is active
     */
    bool isActive() const {
        return m_isActive;
    }

    /**
     * @brief Check if system is in recovery period after E-stop
     * @return true if recently deactivated, still in recovery window
     */
    bool isInRecovery() const;

    /**
     * @brief Check if E-stop was recently activated (within debounce window)
     * @return true if still within debounce period
     */
    bool isDebouncing() const;

    /**
     * @brief Get time since last state change
     * @return Milliseconds since last activation or deactivation
     */
    qint64 timeSinceLastChange() const;

    /**
     * @brief Get time since activation (if currently active)
     * @return Milliseconds since activation, or -1 if not active
     */
    qint64 activeDuration() const;

    /**
     * @brief Get timestamp of last activation
     * @return QDateTime of last activation (invalid if never activated)
     */
    QDateTime lastActivationTime() const {
        return m_lastActivationTime;
    }

    /**
     * @brief Get timestamp of last deactivation
     * @return QDateTime of last deactivation (invalid if never deactivated)
     */
    QDateTime lastDeactivationTime() const {
        return m_lastDeactivationTime;
    }

    /**
     * @brief Get total activation count since startup
     * @return Number of times E-stop has been activated
     */
    int activationCount() const {
        return m_activationCount;
    }

    // ============================================================================
    // EVENT HISTORY
    // ============================================================================

    /**
     * @brief Get recent event history
     * @return Vector of recent EmergencyStopEvent records
     */
    const std::vector<EmergencyStopEvent>& eventHistory() const {
        return m_eventHistory;
    }

    /**
     * @brief Clear event history
     */
    void clearHistory();

signals:
    // ============================================================================
    // STATE CHANGE SIGNALS
    // ============================================================================

    /**
     * @brief Emitted when emergency stop is activated
     * @param event Event details including timestamp and source
     */
    void activated(const EmergencyStopEvent& event);

    /**
     * @brief Emitted when emergency stop is deactivated
     * @param event Event details including duration of activation
     */
    void deactivated(const EmergencyStopEvent& event);

    /**
     * @brief Emitted when recovery period begins (after deactivation)
     *
     * Controllers should prepare for normal operation but not
     * resume until recoveryComplete is emitted.
     */
    void recoveryStarted();

    /**
     * @brief Emitted when recovery period ends
     *
     * Controllers can now resume normal operation.
     */
    void recoveryComplete();

    /**
     * @brief Emitted on any state change (for logging/UI)
     * @param isActive New state
     */
    void stateChanged(bool isActive);

private:
    // ============================================================================
    // STATE
    // ============================================================================
    bool m_isActive = false;      ///< Current E-stop state
    bool m_pendingState = false;  ///< State being debounced
    bool m_isDebouncing = false;  ///< Currently in debounce period

    // ============================================================================
    // TIMING
    // ============================================================================
    QElapsedTimer m_stateTimer;        ///< Time since last state change
    QElapsedTimer m_debounceTimer;     ///< Time since debounce started
    QDateTime m_lastActivationTime;    ///< Timestamp of last activation
    QDateTime m_lastDeactivationTime;  ///< Timestamp of last deactivation

    // ============================================================================
    // STATISTICS
    // ============================================================================
    int m_activationCount = 0;  ///< Total activations since startup

    // ============================================================================
    // EVENT HISTORY
    // ============================================================================
    std::vector<EmergencyStopEvent> m_eventHistory;

    // ============================================================================
    // INTERNAL METHODS
    // ============================================================================

    /**
     * @brief Process confirmed state change (after debounce)
     * @param newState New emergency stop state
     * @param source Source of the change
     */
    void processStateChange(bool newState, const QString& source);

    /**
     * @brief Add event to history
     * @param event Event to record
     */
    void recordEvent(const EmergencyStopEvent& event);
};

