#ifndef CHARGINGSTATEMACHINE_H
#define CHARGINGSTATEMACHINE_H

/**
 * @file chargingstatemachine.h
 * @brief Charging (cocking actuator) state machine extracted from WeaponController
 *
 * This class manages the weapon charging FSM:
 * - Cocking actuator extension/retraction cycle
 * - Multi-cycle support (M2HB requires 2 cycles, others 1)
 * - CROWS M153 4-second post-charge lockout
 * - Jam detection with automatic backoff recovery
 * - Timeout supervision with Fault state
 * - Emergency stop handling during charging
 *
 * DESIGN:
 * - Clear FSM with explicit states
 * - Signal-based completion notification
 * - Timeout protection (default 6 seconds per motion)
 * - Non-interruptible cycle (button release ignored mid-cycle)
 *
 * @date 2025-12-31
 * @version 1.0
 */

#include <QObject>
#include <QTimer>
#include "models/domain/systemstatedata.h"

// Forward declarations
class ServoActuatorDevice;
class SafetyInterlock;
struct ServoActuatorData;

/**
 * @class ChargingStateMachine
 * @brief Manages weapon charging (cocking actuator) FSM
 *
 * Extracted from WeaponController for:
 * - Improved testability
 * - Reduced WeaponController complexity
 * - Clear separation of concerns
 *
 * STATE DIAGRAM:
 * @code
 *   Idle --> Extending --> Extended --> Retracting --> Idle (or next cycle)
 *              |              |              |
 *              v              v              v
 *           [timeout]     [release]      [timeout]
 *              |              |              |
 *              +---> Fault <--+              |
 *                      |                     |
 *                      v                     |
 *              [operator reset]              |
 *                      |                     |
 *                      v                     v
 *                 Retracting             Lockout (4s)
 *                                           |
 *                                           v
 *                                         Idle
 * @endcode
 *
 * JAM DETECTION:
 * - High torque + no movement = mechanical obstruction
 * - Auto-backoff to home position
 * - Operator must acknowledge before retry
 *
 * USAGE:
 * @code
 * ChargingStateMachine charging(servoActuator, safetyInterlock, this);
 * connect(&charging, &ChargingStateMachine::cycleCompleted,
 *         this, &WeaponController::onChargeCycleCompleted);
 *
 * // In button handler:
 * charging.requestCharge(WeaponType::M2HB);
 *
 * // In actuator feedback handler:
 * charging.processActuatorFeedback(data);
 * @endcode
 */
class ChargingStateMachine : public QObject {
    Q_OBJECT

public:
    // ============================================================================
    // CONFIGURATION CONSTANTS
    // ============================================================================

    /**
     * @brief Cocking actuator positions (in millimeters)
     * CONVERSION NOTE: Hardware uses counts, but API uses mm
     * Original counts: EXTEND=63000, RETRACT=2048, TOLERANCE=200
     * Conversion: (counts - 1024) * 3.175mm / 1024counts
     */
    static constexpr double COCKING_EXTEND_POS = 190.6;         ///< Full extension (mm)
    static constexpr double COCKING_RETRACT_POS = 3.175;        ///< Home position (mm)
    static constexpr double COCKING_POSITION_TOLERANCE = 0.62;  ///< Position match tolerance (mm)

    /**
     * @brief Timeout configuration
     */
    static constexpr int COCKING_TIMEOUT_MS = 6000;  ///< Watchdog timeout per motion (ms)
    static constexpr int CHARGE_LOCKOUT_MS = 4000;   ///< CROWS post-charge lockout (ms)

    /**
     * @brief Jam detection parameters
     */
    static constexpr double JAM_TORQUE_THRESHOLD_PERCENT = 65.0;  ///< Trigger level (% of max)
    static constexpr double POSITION_STALL_TOLERANCE_MM = 1.0;    ///< Expected movement per sample
    static constexpr int JAM_CONFIRM_SAMPLES = 3;     ///< Consecutive samples to confirm
    static constexpr int BACKOFF_STABILIZE_MS = 150;  ///< Wait before backoff command

    /**
     * @brief Startup configuration
     */
    static constexpr double ACTUATOR_RETRACTED_THRESHOLD = 5.0;  ///< Threshold for "retracted" (mm)

    // ============================================================================
    // CONSTRUCTOR / DESTRUCTOR
    // ============================================================================

    /**
     * @brief Construct ChargingStateMachine
     * @param actuator ServoActuatorDevice for cocking actuator control
     * @param safetyInterlock SafetyInterlock for charge authorization
     * @param parent Parent QObject
     */
    explicit ChargingStateMachine(ServoActuatorDevice* actuator, SafetyInterlock* safetyInterlock,
                                  QObject* parent = nullptr);
    ~ChargingStateMachine();

    // ============================================================================
    // PUBLIC INTERFACE
    // ============================================================================

    /**
     * @brief Request charge cycle (operator CHG button press)
     *
     * Entry point for initiating weapon charging.
     * If cycle already running, request is ignored.
     * If in Fault state, this triggers fault reset.
     *
     * @param weaponType Current weapon type (determines required cycles)
     * @return true if charge cycle started, false if denied
     */
    bool requestCharge(WeaponType weaponType);

    /**
     * @brief Handle button release (for continuous hold mode)
     *
     * In continuous hold mode, button release in Extended state
     * triggers retraction.
     */
    void onButtonReleased();

    /**
     * @brief Abort charging cycle (e.g., emergency stop)
     * @param reason Reason for abort (for logging/signals)
     */
    void abort(const QString& reason);

    /**
     * @brief Reset from fault state (operator reset command)
     *
     * Attempts safe retraction after a fault (timeout/jam).
     * Only works when in Fault state.
     */
    void resetFault();

    /**
     * @brief Process actuator position feedback
     *
     * Called on every actuator feedback to update FSM.
     * Handles jam detection and state transitions.
     *
     * @param data Actuator feedback data (position, torque, etc.)
     */
    void processActuatorFeedback(const ServoActuatorData& data);

    /**
     * @brief Perform startup retraction if actuator is extended
     *
     * Per CROWS spec: Upon powering on, system automatically
     * retracts the Cocking Actuator if extended.
     *
     * @param currentPosition Current actuator position in mm
     */
    void performStartupRetraction(double currentPosition);

    // ============================================================================
    // STATE QUERIES
    // ============================================================================

    /**
     * @brief Get current charging state
     * @return Current ChargingState
     */
    ChargingState currentState() const {
        return m_currentState;
    }

    /**
     * @brief Check if charging is currently in progress
     * @return true if in Extending, Extended, Retracting, SafeRetract, or JamDetected
     */
    bool isChargingInProgress() const;

    /**
     * @brief Check if post-charge lockout is active
     * @return true if in Lockout state
     */
    bool isLockoutActive() const {
        return m_lockoutActive;
    }

    /**
     * @brief Check if charging is allowed (safety checks)
     * @return true if safe to start charging
     */
    bool isChargingAllowed() const;

    /**
     * @brief Get current cycle number (1-indexed)
     * @return Current cycle count
     */
    int currentCycle() const {
        return m_currentCycleCount;
    }

    /**
     * @brief Get required cycles for current weapon
     * @return Number of cycles required
     */
    int requiredCycles() const {
        return m_requiredCycles;
    }

    /**
     * @brief Get state name for logging
     * @param state ChargingState to convert
     * @return Human-readable state name
     */
    static QString stateName(ChargingState state);

signals:
    // ============================================================================
    // SIGNALS
    // ============================================================================

    /**
     * @brief Emitted when charging state changes
     * @param newState New charging state
     */
    void stateChanged(ChargingState newState);

    /**
     * @brief Emitted when charge cycle starts
     */
    void cycleStarted();

    /**
     * @brief Emitted when charge cycle completes successfully
     */
    void cycleCompleted();

    /**
     * @brief Emitted when charge cycle faults (timeout or jam)
     */
    void cycleFaulted();

    /**
     * @brief Emitted when jam is detected
     */
    void jamDetected();

    /**
     * @brief Emitted when lockout period ends
     */
    void lockoutExpired();

    /**
     * @brief Request actuator movement
     * @param positionMM Target position in millimeters
     */
    void requestActuatorMove(double positionMM);

    /**
     * @brief Request actuator stop (for jam recovery)
     */
    void requestActuatorStop();

private slots:
    /**
     * @brief Handle charging timeout
     */
    void onChargingTimeout();

    /**
     * @brief Handle lockout timer expiry
     */
    void onLockoutExpired();

private:
    // ============================================================================
    // FSM METHODS
    // ============================================================================

    /**
     * @brief Start a single charge cycle
     */
    void startCycle();

    /**
     * @brief Process actuator position for state transitions
     * @param positionMM Current position in millimeters
     */
    void processPosition(double positionMM);

    /**
     * @brief Transition to new charging state
     * @param newState New charging state
     */
    void transitionTo(ChargingState newState);

    /**
     * @brief Get required cycles for weapon type
     * @param type Weapon type
     * @return Number of cycles required
     */
    static int getRequiredCyclesForWeapon(WeaponType type);

    /**
     * @brief Start 4-second post-charge lockout
     */
    void startLockout();

    // ============================================================================
    // JAM DETECTION
    // ============================================================================

    /**
     * @brief Check for jam condition in actuator feedback
     * @param data Actuator feedback data
     */
    void checkForJam(const ServoActuatorData& data);

    /**
     * @brief Execute jam recovery sequence
     */
    void executeJamRecovery();

    /**
     * @brief Reset jam detection state
     */
    void resetJamDetection();

    // ============================================================================
    // DEPENDENCIES
    // ============================================================================
    ServoActuatorDevice* m_actuator = nullptr;
    SafetyInterlock* m_safetyInterlock = nullptr;

    // ============================================================================
    // STATE
    // ============================================================================
    ChargingState m_currentState = ChargingState::Idle;
    int m_currentCycleCount = 0;
    int m_requiredCycles = 1;
    bool m_isShortPressCharge = false;  ///< true = auto-cycle, false = continuous hold
    bool m_lockoutActive = false;
    bool m_buttonCurrentlyHeld = false;  ///< Tracks button state for Extended handling

    // ============================================================================
    // TIMERS
    // ============================================================================
    QTimer* m_timeoutTimer = nullptr;
    QTimer* m_lockoutTimer = nullptr;

    // ============================================================================
    // JAM DETECTION STATE
    // ============================================================================
    int m_jamDetectionCounter = 0;
    double m_previousFeedbackPosition = 0.0;
    bool m_jamDetectionActive = false;
};

#endif  // CHARGINGSTATEMACHINE_H
