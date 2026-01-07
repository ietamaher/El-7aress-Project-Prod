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
#include "../utils/firecontrolcomputation.h"
#include "chargingstatemachine.h"

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
class WeaponController : public QObject {
    Q_OBJECT

public:
    // ============================================================================
    // Constructor / Destructor
    // ============================================================================
    explicit WeaponController(SystemStateModel* stateModel, ServoActuatorDevice* servoActuator,
                              Plc42Device* plc42, SafetyInterlock* safetyInterlock,
                              QObject* parent = nullptr);
    ~WeaponController();

    /**
     * @brief Get the safety interlock instance
     * @return Pointer to the SafetyInterlock
     */
    SafetyInterlock* safetyInterlock() const {
        return m_safetyInterlock;
    }

    // ============================================================================
    // Weapon Control
    // ============================================================================
    void armWeapon(bool enable);
    void fireSingleShot();
    virtual void startFiring();
    virtual void stopFiring();

    // ============================================================================
    // Charging Control (legacy API - still supported)
    // ============================================================================
    virtual void unloadWeapon();

    // ============================================================================
    // Fire Control Solution
    // ============================================================================
    virtual void updateFireControlSolution();

public slots:
    // ============================================================================
    // Charging Cycle Control (Cocking Actuator FSM-based API)
    // ============================================================================

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
    // ============================================================================
    // Weapon State Notifications
    // ============================================================================
    void weaponArmed(bool armed);
    void weaponFired();

    // ============================================================================
    // Charging Cycle Notifications
    // ============================================================================
    void chargeCycleStarted();
    void chargeCycleCompleted();
    void chargeCycleFaulted();

private slots:
    // ============================================================================
    // State Handlers
    // ============================================================================
    void onSystemStateChanged(const SystemStateData& newData);

    // ============================================================================
    // ChargingStateMachine Signal Handlers
    // ============================================================================
    void onChargingStateChanged(ChargingState newState);
    void onChargeCycleStarted();
    void onChargeCycleCompleted();
    void onChargeCycleFaulted();
    void onChargeLockoutExpired();

    // ============================================================================
    // Actuator Feedback (forwarded to ChargingStateMachine)
    // ============================================================================
    void onActuatorFeedback(const ServoActuatorData& data);

private:
    // ============================================================================
    // Fire Control Computation (extracted for unit-testability)
    // ============================================================================
    FireControlComputation m_fireControlComputation;  ///< Fire control calculator
    FireControlResult m_previousFireControlResult;    ///< Previous result for change detection

    /**
     * @brief Build FireControlInput from current SystemStateData
     * @param data Current system state
     * @return Input struct for fire control computation
     */
    FireControlInput buildFireControlInput(const SystemStateData& data) const;

    /**
     * @brief Apply FireControlResult to SystemStateData
     * @param result Computed fire control result
     * @param data System state to update
     */
    void applyFireControlResult(const FireControlResult& result, SystemStateData& data);

    // ============================================================================
    // ChargingStateMachine (extracted FSM)
    // ============================================================================
    // NOTE: Charging FSM has been extracted to ChargingStateMachine class for
    // improved testability and reduced complexity.
    // See: chargingstatemachine.h for state diagram and documentation.
    // ============================================================================
    ChargingStateMachine* m_chargingStateMachine = nullptr;

    /**
     * @brief Handle actuator move request from ChargingStateMachine
     * @param positionMM Target position in millimeters
     */
    void onActuatorMoveRequested(double positionMM);

    /**
     * @brief Handle actuator stop request from ChargingStateMachine
     */
    void onActuatorStopRequested();

    // ============================================================================
    // Dependencies
    // ============================================================================
    SystemStateModel* m_stateModel = nullptr;
    ServoActuatorDevice* m_servoActuator = nullptr;
    Plc42Device* m_plc42 = nullptr;
    SafetyInterlock* m_safetyInterlock = nullptr;

    // ============================================================================
    // Ballistics
    // ============================================================================
    BallisticsProcessorLUT* m_ballisticsProcessor = nullptr;

    // ============================================================================
    // State Tracking
    // ============================================================================
    SystemStateData m_oldState;

    // ============================================================================
    // Weapon State
    // ============================================================================
    bool m_systemArmed = false;
    bool m_fireReady = false;

    // ============================================================================
    // EMERGENCY STOP STATE (Military-Grade Safety)
    // ============================================================================
    bool m_wasInEmergencyStop = false;  ///< Edge detection for emergency stop
};


#endif  // WEAPONCONTROLLER_H