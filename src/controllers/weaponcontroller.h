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
struct ServoActuatorData;

/**
 * @brief Weapon system controller
 *
 * This class manages weapon operations including:
 * - Fire control (arm/fire/stop)
 * - Ammunition feed cycle FSM (MIL-STD compliant, non-interruptible)
 * - Ballistic calculations (drop compensation, motion lead)
 * - Crosswind calculations from windage parameters
 *
 * Fire Control Solution (Kongsberg/Rafael approach):
 * - Ballistic drop: Auto-applied when LRF range valid
 * - Motion lead: Only when LAC toggle active
 *
 * Ammo Feed Cycle (CROWS M153 style):
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
                              QObject* parent = nullptr);
    ~WeaponController();

    // ========================================================================
    // Weapon Control
    // ========================================================================
    void armWeapon(bool enable);
    void fireSingleShot();
    virtual void startFiring();
    virtual void stopFiring();

    // ========================================================================
    // Ammunition Control (legacy API - still supported)
    // ========================================================================
    virtual void unloadAmmo();

    // ========================================================================
    // Fire Control Solution
    // ========================================================================
    virtual void updateFireControlSolution();

public slots:
    // ========================================================================
    // Ammo Feed Cycle Control (new FSM-based API)
    // ========================================================================

    /**
     * @brief Start ammo feed cycle (operator button press)
     *
     * This is the preferred entry point for initiating ammo loading.
     * Decoupled from UI toggle - only starts cycle, does not track button state.
     * If cycle already running, request is ignored.
     */
    void onOperatorRequestLoad();

    /**
     * @brief Reset from fault state (operator reset command)
     *
     * Attempts safe retraction after a fault (timeout/jam).
     * Only works when in Fault state.
     */
    void resetFeedFault();

signals:
    // ========================================================================
    // Weapon State Notifications
    // ========================================================================
    void weaponArmed(bool armed);
    void weaponFired();

    // ========================================================================
    // Ammo Feed Cycle Notifications
    // ========================================================================
    void ammoFeedCycleStarted();
    void ammoFeedCycleCompleted();
    void ammoFeedCycleFaulted();

private slots:
    // ========================================================================
    // State Handlers
    // ========================================================================
    void onSystemStateChanged(const SystemStateData& newData);

    // ========================================================================
    // Ammo Feed FSM Handlers
    // ========================================================================
    void onFeedTimeout();
    void onActuatorFeedback(const ServoActuatorData& data);

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
    // Ammo Feed Cycle FSM
    // ========================================================================

    /**
     * @brief Ammo Feed State Machine States
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
    // Uses global AmmoFeedState enum from systemstatedata.h

    void startAmmoFeedCycle();
    void processActuatorPosition(double posMM);
    void transitionFeedState(AmmoFeedState newState);
    QString feedStateName(AmmoFeedState s) const;

    // ========================================================================
    // Ammo Feed Configuration (in millimeters - matches ServoActuatorData units)
    // ========================================================================
    // CONVERSION NOTE: Hardware uses counts, but API uses mm
    // Original counts: EXTEND=63000, RETRACT=2048, TOLERANCE=200
    // Conversion: (counts - 1024) * 3.175mm / 1024counts
    static constexpr double FEED_EXTEND_POS = 190.6; //192.11;       ///< Full extension (mm) [was 63000 counts]
    static constexpr double FEED_RETRACT_POS = 3.175;       ///< Home position (mm) [was 2048 counts]
    static constexpr double FEED_POSITION_TOLERANCE = 0.62; ///< Position match tolerance (mm) [was 200 counts]
    static constexpr int FEED_TIMEOUT_MS = 6000;            ///< Watchdog timeout (ms)

    // ========================================================================
    // Ammo Feed State
    // ========================================================================
    AmmoFeedState m_feedState = AmmoFeedState::Idle;
    QTimer* m_feedTimer = nullptr;

    // ========================================================================
    // Dependencies
    // ========================================================================
    SystemStateModel* m_stateModel = nullptr;
    ServoActuatorDevice* m_servoActuator = nullptr;
    Plc42Device* m_plc42 = nullptr;

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