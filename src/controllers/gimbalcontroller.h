#ifndef GIMBALCONTROLLER_H
#define GIMBALCONTROLLER_H

// ============================================================================
// Qt Framework
// ============================================================================
#include <QObject>
#include <QTimer>
#include <QElapsedTimer>

// ============================================================================
// Standard Library
// ============================================================================
#include <memory>

// ============================================================================
// Project
// ============================================================================
#include "motion_modes/gimbalmotionmodebase.h"
#include "models/domain/systemstatemodel.h"
#include "homingcontroller.h"

// ============================================================================
// Forward Declarations
// ============================================================================
class ServoDriverDevice;
class Plc42Device;
class SafetyInterlock;

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
class GimbalController : public QObject {
    Q_OBJECT

public:
    // ============================================================================
    // Constructor / Destructor
    // ============================================================================
    explicit GimbalController(ServoDriverDevice* azServo, ServoDriverDevice* elServo,
                              Plc42Device* plc42, SystemStateModel* stateModel,
                              SafetyInterlock* safetyInterlock, QObject* parent = nullptr);
    ~GimbalController();

    /**
     * @brief Get the safety interlock instance
     * @return Pointer to the SafetyInterlock
     */
    SafetyInterlock* safetyInterlock() const {
        return m_safetyInterlock;
    }

    // ============================================================================
    // Motion Control
    // ============================================================================
    void update();
    void setMotionMode(MotionMode newMode);
    MotionMode currentMotionModeType() const {
        return m_currentMotionModeType;
    }

    // ============================================================================
    // Device Accessors
    // ============================================================================
    ServoDriverDevice* azimuthServo() const {
        return m_azServo;
    }
    ServoDriverDevice* elevationServo() const {
        return m_elServo;
    }
    Plc42Device* plc42() const {
        return m_plc42;
    }
    SystemStateModel* systemStateModel() const {
        return m_stateModel;
    }

    // ============================================================================
    // Alarm Management
    // ============================================================================
    void readAlarms();
    void clearAlarms();

signals:
    // ============================================================================
    // Tracking Updates
    // Thread-safe tracking target update signal (Qt::QueuedConnection)
    // Emitted from vision thread to motion mode slot, eliminating race conditions
    // ============================================================================
    void trackingTargetUpdated(double azDeg, double elDeg, double azVel_dps, double elVel_dps,
                               bool valid);

    // ============================================================================
    // Alarm Notifications
    // ============================================================================
    void azAlarmDetected(uint16_t alarmCode, const QString& description);
    void azAlarmCleared();
    void elAlarmDetected(uint16_t alarmCode, const QString& description);
    void elAlarmCleared();

private slots:
    // ============================================================================
    // State Management
    // ============================================================================
    void onSystemStateChanged(const SystemStateData& newData);

    // ============================================================================
    // Alarm Handlers
    // ============================================================================
    void onAzAlarmDetected(uint16_t alarmCode, const QString& description);
    void onAzAlarmCleared();
    void onElAlarmDetected(uint16_t alarmCode, const QString& description);
    void onElAlarmCleared();

    // ============================================================================
    // HomingController Handlers
    // ============================================================================
    void onHomingCompleted();
    void onHomingFailed(const QString& reason);
    void onHomingAborted(const QString& reason);

private:
    // ============================================================================
    // Private Methods
    // ============================================================================
    void shutdown();

    // ============================================================================
    // HomingController Delegation
    // ============================================================================
    // NOTE: Homing FSM extracted to HomingController class for testability
    // GimbalController delegates homing to m_homingController
    // ============================================================================

    // ============================================================================
    // Emergency Stop Handler
    // ============================================================================
    void processEmergencyStop(const SystemStateData& data);

    // ============================================================================
    // Free Mode Handler
    // ============================================================================
    void processFreeMode(const SystemStateData& data);

    // ============================================================================
    // Hardware Devices
    // ============================================================================
    ServoDriverDevice* m_azServo = nullptr;
    ServoDriverDevice* m_elServo = nullptr;
    Plc42Device* m_plc42 = nullptr;

    // ============================================================================
    // System State
    // ============================================================================
    SystemStateModel* m_stateModel = nullptr;
    SafetyInterlock* m_safetyInterlock = nullptr;
    SystemStateData m_oldState;

    // ============================================================================
    // Motion Mode Management (Strategy Pattern)
    // ============================================================================
    std::unique_ptr<GimbalMotionModeBase> m_currentMode;
    MotionMode m_currentMotionModeType = MotionMode::Manual;

    // ============================================================================
    // Timers
    // ============================================================================
    QTimer* m_updateTimer = nullptr;

    // Centralized dt measurement timer (Expert Review Fix)
    QElapsedTimer m_velocityTimer;

    // ============================================================================
    // HomingController (extracted FSM)
    // ============================================================================
    HomingController* m_homingController = nullptr;

    // ============================================================================
    // State Tracking Flags
    // ============================================================================
    bool m_wasInFreeMode = false;
    bool m_wasInEmergencyStop = false;

    // ============================================================================
    // Manual Mode LAC Support (CROWS-like behavior)
    // Tracks previous gimbal positions to calculate angular velocity for LAC
    // when operator is manually tracking a moving target with joystick.
    // See: CROWS TM 9-1090-225-10-2, page 2-26
    // "The computer is constantly monitoring the change in rotation of the
    //  elevation and azimuth axes and measuring the speed."
    // ============================================================================
    double m_prevGimbalAz = 0.0;
    double m_prevGimbalEl = 0.0;
    bool m_gimbalVelocityInitialized = false;
};

#endif  // GIMBALCONTROLLER_H