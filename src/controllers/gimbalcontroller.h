#ifndef GIMBALCONTROLLER_H
#define GIMBALCONTROLLER_H

// ============================================================================
// INCLUDES
// ============================================================================

// Qt Framework
#include <QObject>
#include <QTimer>
#include <QElapsedTimer>

// Standard Library
#include <memory>

// Project
#include "motion_modes/gimbalmotionmodebase.h"
#include "models/domain/systemstatemodel.h"

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

class ServoDriverDevice;
class Plc42Device;

// ============================================================================
// CLASS DEFINITION
// ============================================================================

/**
 * @brief Gimbal motion mode coordinator and servo controller
 *
 * This class manages gimbal motion by coordinating different motion modes
 * (Manual, AutoTrack, Scanning, etc.) and controlling azimuth/elevation servos
 * through a PLC42 device. It provides:
 * - Dynamic motion mode switching
 * - Thread-safe tracking target updates
 * - Periodic servo command updates
 * - Safety condition monitoring
 * - Alarm management
 *
 * Architecture:
 * - Strategy Pattern: Motion modes are pluggable GimbalMotionModeBase subclasses
 * - Observer Pattern: Reacts to SystemStateModel changes
 * - Timer-based update loop (50ms / 20Hz)
 */
class GimbalController : public QObject
{
    Q_OBJECT

public:
    // ========================================================================
    // PUBLIC INTERFACE
    // ========================================================================

    explicit GimbalController(ServoDriverDevice* azServo,
                              ServoDriverDevice* elServo,
                              Plc42Device* plc42,
                              SystemStateModel* stateModel,
                              QObject* parent = nullptr);
    ~GimbalController();

    // --- Motion Control ---
    void update();
    void setMotionMode(MotionMode newMode);
    MotionMode currentMotionModeType() const { return m_currentMotionModeType; }

    // --- Device Accessors ---
    ServoDriverDevice* azimuthServo() const { return m_azServo; }
    ServoDriverDevice* elevationServo() const { return m_elServo; }
    Plc42Device* plc42() const { return m_plc42; }
    SystemStateModel* systemStateModel() const { return m_stateModel; }

    // --- Alarm Management ---
    void readAlarms();
    void clearAlarms();

signals:
    // --- Tracking Updates ---
    // Thread-safe tracking target update signal (Qt::QueuedConnection)
    // Emitted from vision thread to motion mode slot, eliminating race conditions
    void trackingTargetUpdated(double azDeg, double elDeg,
                               double azVel_dps, double elVel_dps,
                               bool valid);

    // --- Alarm Notifications ---
    void azAlarmDetected(uint16_t alarmCode, const QString &description);
    void azAlarmCleared();
    void elAlarmDetected(uint16_t alarmCode, const QString &description);
    void elAlarmCleared();

private slots:
    // --- State Management ---
    void onSystemStateChanged(const SystemStateData &newData);

    // --- Alarm Handlers ---
    void onAzAlarmDetected(uint16_t alarmCode, const QString &description);
    void onAzAlarmCleared();
    void onElAlarmDetected(uint16_t alarmCode, const QString &description);
    void onElAlarmCleared();
    void onHomingTimeout();

private:
    // ========================================================================
    // PRIVATE METHODS
    // ========================================================================

    void shutdown();
    // ========================================================================
    // HOMING STATE MACHINE METHODS
    // ========================================================================
    void processHomingSequence(const SystemStateData& data);
    void startHomingSequence();
    void completeHomingSequence();
    void abortHomingSequence(const QString& reason);
    
    // ========================================================================
    // EMERGENCY STOP HANDLER
    // ========================================================================
    void processEmergencyStop(const SystemStateData& data);
    
    // ========================================================================
    // FREE MODE HANDLER
    // ========================================================================
    void processFreeMode(const SystemStateData& data);
    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================

    // --- Hardware Devices ---
    ServoDriverDevice* m_azServo = nullptr;
    ServoDriverDevice* m_elServo = nullptr;
    Plc42Device* m_plc42 = nullptr;

    // --- System State ---
    SystemStateModel* m_stateModel = nullptr;
    SystemStateData m_oldState;

    // --- Motion Mode Management ---
    std::unique_ptr<GimbalMotionModeBase> m_currentMode;
    MotionMode m_currentMotionModeType = MotionMode::Manual;

    // --- Update Timer ---
    QTimer* m_updateTimer = nullptr;

    // --- Centralized dt measurement timer (Expert Review Fix) ---
    QElapsedTimer m_velocityTimer;

    // ========================================================================
    // HOMING STATE MACHINE
    // ========================================================================
    QTimer* m_homingTimeoutTimer = nullptr;
    HomingState m_currentHomingState = HomingState::Idle;
    MotionMode m_modeBeforeHoming = MotionMode::Idle;  // Store mode to restore after homing
    
    static constexpr int HOMING_TIMEOUT_MS = 30000;  // 30 seconds timeout for homing
    
    // ========================================================================
    // STATE TRACKING
    // ========================================================================
    bool m_wasInFreeMode = false;
    bool m_wasInEmergencyStop = false;    
};

#endif // GIMBALCONTROLLER_H
