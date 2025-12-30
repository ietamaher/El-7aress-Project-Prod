#ifndef WEAPONCONTROLLER_H
#define WEAPONCONTROLLER_H

// ============================================================================
// Qt Framework
// ============================================================================
#include <QObject>
#include <QTimer>

// ============================================================================
// Project
// ============================================================================
#include "models/domain/systemstatemodel.h"
#include "../utils/ballisticsprocessorlut.h"

// ============================================================================
// Forward Declarations
// ============================================================================
class Plc42Device;
class ServoActuatorDevice;
class SafetyInterlock;
struct ServoActuatorData;

/**
 * @brief Weapon system controller
 *
 * This class manages weapon operations including:
 * - Fire control (arm/fire/stop)
 * - Charging cycle FSM (MIL-STD compliant, non-interruptible)
 * - Ballistic calculations (drop compensation, motion lead)
 * - Crosswind calculations from windage parameters
 *
 * Fire Control Solution (Kongsberg/Rafael approach):
 * - Ballistic drop: Auto-applied when LRF range valid
 * - Motion lead: Only when LAC toggle active
 *
 * Charging Cycle (CROWS M153 style - Cocking Actuator):
 * - Once started, cycle runs to completion (Extend -> Retract -> Idle)
 * - Operator button release during cycle is IGNORED
 * - Timeout supervision with Fault state and operator reset
 */
class WeaponController : public QObject
{
    Q_OBJECT

public:
    // ========================================================================
    // Constructor / Destructor
    // ========================================================================
    explicit WeaponController(SystemStateModel* stateModel,
                              ServoActuatorDevice* servoActuator,
                              Plc42Device* plc42,
                              SafetyInterlock* safetyInterlock,
                              QObject* parent = nullptr);
    ~WeaponController();

    /**
     * @brief Get the safety interlock instance
     * @return Pointer to the SafetyInterlock
     */
    SafetyInterlock* safetyInterlock() const { return m_safetyInterlock; }

    // ========================================================================
    // Weapon Control
    // ========================================================================
    void armWeapon(bool enable);
    void fireSingleShot();
    virtual void startFiring();
    virtual void stopFiring();

    // ========================================================================
    // Charging Control (legacy API - still supported)
    // ========================================================================
    virtual void unloadWeapon();

    // ========================================================================
    // Fire Control Solution
    // ========================================================================
    virtual void updateFireControlSolution();

public slots:
    // ========================================================================
    // Charging Cycle Control (Cocking Actuator FSM-based API)
    // ========================================================================

    /**
     * @brief Start charging cycle (operator CHG button press)
     *
     * This is the preferred entry point for initiating weapon charging.
     * Decoupled from UI toggle - only starts cycle, does not track button state.
     * If cycle already running, request is ignored.
     */
    void onOperatorRequestCharge();

    /**
     * @brief Reset from fault state (operator reset command)
     *
     * Attempts safe retraction after a fault (timeout/jam).
     * Only works when in Fault state.
     */
    void resetChargingFault();

signals:
    // ========================================================================
    // Weapon State Notifications
    // ========================================================================
    void weaponArmed(bool armed);
    void weaponFired();

    // ========================================================================
    // Charging Cycle Notifications
    // ========================================================================
    void chargeCycleStarted();
    void chargeCycleCompleted();
    void chargeCycleFaulted();

private slots:
    // ========================================================================
    // State Handlers
    // ========================================================================
    void onSystemStateChanged(const SystemStateData& newData);

    // ========================================================================
    // Charging FSM Handlers
    // ========================================================================
    void onChargingTimeout();
    void onActuatorFeedback(const ServoActuatorData& data);

    // ========================================================================
    // CROWS M153 Lockout Handler
    // ========================================================================
    void onChargeLockoutExpired();

private:
    // ========================================================================
    // Ballistics Calculations
    // ========================================================================

    /**
     * @brief Calculate crosswind component from wind direction and gimbal azimuth
     *
     * This function converts absolute wind (direction + speed) into the crosswind
     * component perpendicular to the firing direction. The crosswind varies with
     * gimbal azimuth - same wind conditions produce different crosswind based on
     * where the weapon is pointing.
     *
     * @param windSpeedMS Wind speed in meters per second
     * @param windDirectionDeg Wind direction in degrees (where wind is FROM)
     * @param gimbalAzimuthDeg Current gimbal azimuth (where weapon is pointing TO)
     * @return Crosswind component in m/s (positive = right deflection, negative = left)
     * 
     * Example:
     *   Wind from North (0°), firing North (0°)   -> crosswind = 0 m/s (headwind)
     *   Wind from North (0°), firing East (90°)   -> crosswind = +wind_speed (full right)
     *   Wind from North (0°), firing South (180°) -> crosswind = 0 m/s (tailwind)
     *   Wind from North (0°), firing West (270°)  -> crosswind = -wind_speed (full left)
     */
    float calculateCrosswindComponent(float windSpeedMS,
                                      float windDirectionDeg,
                                      float gimbalAzimuthDeg);

    // ========================================================================
    // Charging Cycle FSM (Cocking Actuator)
    // ========================================================================

    /**
     * @brief Charging State Machine States
     *
     * State diagram:
     *   Idle --> Extending --> Retracting --> Idle
     *              |              |
     *              v              v
     *           [timeout]     [timeout]
     *              |              |
     *              +---> Fault <--+
     *                      |
     *                      v
     *              [operator reset]
     *                      |
     *                      v
     *                 Retracting
     */
    // Uses global ChargingState enum from systemstatedata.h

    void startChargeCycle();
    void processActuatorPosition(double posMM);
    void transitionChargingState(ChargingState newState);
    QString chargingStateName(ChargingState s) const;

    // ========================================================================
    // CROWS M153 Charging Helpers
    // ========================================================================
    int getRequiredCyclesForWeapon(WeaponType type) const;
    void startChargeLockout();
    void performStartupRetraction();
    bool isChargingAllowed() const;

    // ========================================================================
    // Cocking Actuator Configuration (in millimeters - matches ServoActuatorData units)
    // ========================================================================
    // CONVERSION NOTE: Hardware uses counts, but API uses mm
    // Original counts: EXTEND=63000, RETRACT=2048, TOLERANCE=200
    // Conversion: (counts - 1024) * 3.175mm / 1024counts
    static constexpr double COCKING_EXTEND_POS = 190.6;         ///< Full extension (mm) [was 63000 counts]
    static constexpr double COCKING_RETRACT_POS = 3.175;        ///< Home position (mm) [was 2048 counts]
    static constexpr double COCKING_POSITION_TOLERANCE = 0.62;  ///< Position match tolerance (mm) [was 200 counts]
    static constexpr int COCKING_TIMEOUT_MS = 6000;             ///< Watchdog timeout (ms)

    // ========================================================================
    // CROWS M153 Charging Configuration
    // ========================================================================
    // Reference: TM 9-1090-225-10-2 (CROWS Technical Manual)
    // - CHG button activates Cocking Actuator (CA)
    // - During charging, Fire Circuit is disabled
    // - After charging completes, additional charging prevented for 4 seconds
    // - M2HB requires 2 cycles (closed bolt), M240B/M249 require 1 cycle (open bolt)
    static constexpr int CHARGE_LOCKOUT_MS = 4000;          ///< Post-charge lockout duration (ms)
    static constexpr double ACTUATOR_RETRACTED_THRESHOLD = 5.0; ///< Threshold for "retracted" on startup (mm)

    // ========================================================================
    // Charging State (Cocking Actuator FSM)
    // ========================================================================
    ChargingState m_chargingState = ChargingState::Idle;
    QTimer* m_chargingTimer = nullptr;

    // ========================================================================
    // CROWS M153 Multi-Cycle Charging State
    // ========================================================================
    int m_currentCycleCount = 0;            ///< Current cycle within charge sequence
    int m_requiredCycles = 2;               ///< Cycles required for current weapon (M2HB=2, others=1)
    bool m_isShortPressCharge = false;      ///< True if this is an automatic multi-cycle charge
    QTimer* m_lockoutTimer = nullptr;       ///< 4-second post-charge lockout timer
    bool m_chargeLockoutActive = false;     ///< Lockout is active, charging prevented

    // ========================================================================
    // Dependencies
    // ========================================================================
    SystemStateModel* m_stateModel = nullptr;
    ServoActuatorDevice* m_servoActuator = nullptr;
    Plc42Device* m_plc42 = nullptr;
    SafetyInterlock* m_safetyInterlock = nullptr;

    // ========================================================================
    // Ballistics
    // ========================================================================
    BallisticsProcessorLUT* m_ballisticsProcessor = nullptr;

    // ========================================================================
    // State Tracking
    // ========================================================================
    SystemStateData m_oldState;

    // ========================================================================
    // Weapon State
    // ========================================================================
    bool m_systemArmed = false;
    bool m_fireReady = false;

    // ========================================================================
    // EMERGENCY STOP STATE (Military-Grade Safety)
    // ========================================================================
    bool m_wasInEmergencyStop = false;  ///< Edge detection for emergency stop


    // ============================================================================
// JAM DETECTION PARAMETERS
// ============================================================================
// Tunable thresholds - adjust based on field testing
static constexpr double JAM_TORQUE_THRESHOLD_PERCENT = 65.0;  // % of max - trigger level
static constexpr double POSITION_STALL_TOLERANCE_MM = 1.0;    // Expected movement per sample
static constexpr int JAM_CONFIRM_SAMPLES = 3;                 // Consecutive samples to confirm
static constexpr int BACKOFF_STABILIZE_MS = 150;              // Wait before backoff command

// ============================================================================
// JAM DETECTION STATE
// ============================================================================
int m_jamDetectionCounter = 0;
double m_previousFeedbackPosition = 0.0;
bool m_jamDetectionActive = false;

// ============================================================================
// PRIVATE METHODS - JAM HANDLING
// ============================================================================
void checkForJamCondition(const ServoActuatorData& data);
void executeJamRecovery();
void resetJamDetection();

};

#endif // WEAPONCONTROLLER_H