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



// ============================================================================
// AMMUNITION FEED FSM - TORQUE-AWARE WITH SAFE FAULT HANDLING
// ============================================================================
// Design Philosophy (Military Standard):
// - Detect jam BEFORE fuse blows (proactive, not reactive)
// - On jam: STOP immediately, RETRACT to home, enter FAULT state
// - NO automatic retry - operator must physically verify gun is clear
// - Fault state requires deliberate operator action to clear
// ============================================================================


// ============================================================================
// ULTRAMOTION A2 TORQUE/CURRENT CONFIGURATION
// ============================================================================
// TQ command returns: -32768 to 32767 (signed 16-bit)
// This represents "transformed three phase current feedback"
// 
// Calibration approach:
// - Run normal feed cycle, record max TQ value → NOMINAL_TORQUE_RAW
// - Jam threshold = NOMINAL × 1.5 (50% above normal)
// - Critical threshold = Calculate from fuse rating
//
// Your setup: 180W motor, 24V supply, 5A fuse
// - Max motor current: 180W / 24V = 7.5A
// - Fuse trips at: 5A
// - Safe operating limit: ~4A (80% of fuse) = 53% of motor max
// - If TQ at 7.5A = 32767, then 4A ≈ 17,476 raw counts
// ============================================================================

struct JamDetectionConfig {
    // === TORQUE THRESHOLDS (percentage) ===
    float torqueNominal = 40.0f;       // Expected max during normal operation (CALIBRATE!)
    float torqueWarning = 50.0f;       // Start monitoring closely
    float torqueJamThreshold = 55.0f;  // Definite jam - initiate safe retract
    float torqueCritical = 62.0f;      // Emergency stop - approaching fuse limit (67%)

    // === STALL DETECTION ===
    float stallPositionTolerance = 0.00f;  // mm
    int stallTimeMs = 150;


    // === SAFE RETRACT PARAMETERS ===
    int16_t safeRetractTorqueLimit = 10000; // Reduced torque for retraction
    int safeRetractTimeoutMs = 3000;        // Max time for safe retract

    // === TIMING ===
    int torqueSampleIntervalMs = 25;   // 40Hz torque monitoring
    int positionSampleIntervalMs = 25; // 40Hz position monitoring
};

// ============================================================================
// JAM EVENT DATA (for diagnostics and post-mission analysis)
// ============================================================================
struct JamEventData {
    qint64 timestamp;
    AmmoFeedState stateWhenDetected;
    float positionWhenDetected;         // mm
    float torquePercentWhenDetected;    // was torqueRawWhenDetected
    QString detectionReason;        // "torque_limit", "stall", "critical", "timeout"
    bool safeRetractSuccessful;
};

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
    void unloadAmmo();
 

    // ========================================================================
    // Fire Control Solution
    // ========================================================================
    virtual void updateFireControlSolution();


        // === FAULT HANDLING API ===
    void resetFeedFault();      // Operator-initiated fault reset (after physical check)
    void abortFeedCycle();      // Emergency abort
    
    // === CONFIGURATION ===
    void setJamDetectionConfig(const JamDetectionConfig& config);
    JamDetectionConfig jamDetectionConfig() const { return m_jamConfig; }
    
    // === CALIBRATION HELPERS ===
    float lastTorquePercent() const { return m_lastTorquePercent; }      // was lastRawTorque()
    float peakTorqueThisCycle() const { return m_peakTorqueThisCycle; }  // was int16_t
    void resetPeakTorque() { m_peakTorqueThisCycle = 0; }
    
    // === DIAGNOSTICS ===
    AmmoFeedState currentFeedState() const { return m_feedState; }
    QList<JamEventData> jamHistory() const { return m_jamHistory; }
    void clearJamHistory() { m_jamHistory.clear(); }

signals:
    // ========================================================================
    // Weapon State Notifications
    // ========================================================================
    void weaponArmed(bool armed);
    void weaponFired();

    // ========================================================================
    // Ammo Feed Cycle Notifications
    // ========================================================================
    // === FEED CYCLE SIGNALS ===
    void ammoFeedCycleStarted();
    void ammoFeedCycleCompleted();
    void ammoFeedCycleFaulted(const QString& reason);
    void ammoFeedCycleAborted();
    
    // === JAM DETECTION SIGNALS ===
    void jamDetected(const JamEventData& event);
    void torqueWarning(int16_t currentTorque, int16_t threshold);
    void safeRetractStarted();
    void safeRetractCompleted(bool success);
    
    // === CALIBRATION SIGNAL ===
    void torqueUpdated(int16_t rawTorque);  // For calibration UI

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
    void onSafeRetractTimeout();
    // === JAM MONITORING ===
    void onJamMonitorTimer();
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


    void processActuatorPosition(double posMM);

    // === INITIALIZATION ===
    void setupTimers();
    void setupConnections();
    
    // === FEED CYCLE FSM ===
    void onOperatorRequestLoad();
    void startAmmoFeedCycle();
    void transitionFeedState(AmmoFeedState newState);
    QString feedStateName(AmmoFeedState s) const;
    
    // === JAM DETECTION ===
    void startJamMonitoring();
    void stopJamMonitoring();
    void processJamMonitoring();
    void handleJamDetected(const QString& reason);
    void startSafeRetract();
    void handleSafeRetractComplete(bool success);
    
    // === MOTION COMMANDS ===
    void commandExtend();
    void commandRetract();
    void commandStop();
    void setTorqueLimit(int16_t rawLimit);
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
    // === FEED CYCLE FSM ===
    AmmoFeedState m_feedState = AmmoFeedState::Idle;
    QTimer* m_feedTimer = nullptr;          // Overall cycle timeout
    QTimer* m_safeRetractTimer = nullptr;   // Safe retract timeout
    QTimer* m_jamMonitorTimer = nullptr;    // High-frequency jam monitoring

    // === JAM DETECTION STATE ===
    JamDetectionConfig m_jamConfig;
    float m_lastTorquePercent = 0.0f;
    float m_peakTorqueThisCycle = 0.0f;
    float m_lastKnownPosition = 0.0f;       // inches
    qint64 m_lastPositionChangeTime = 0;    // For stall detection
    QElapsedTimer m_cycleTimer;             // Timing for diagnostics
    
    // === JAM HISTORY ===
    QList<JamEventData> m_jamHistory;
    static constexpr int MAX_JAM_HISTORY = 50;

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
};

#endif // WEAPONCONTROLLER_H
