#pragma once

// ============================================================================
// INCLUDES
// ============================================================================
/**
 * @file PresetHomePositionController.h
 * @brief Controller for the Preset Home Position procedure
 *
 * This controller manages the workflow for setting the current gimbal position
 * as the home (zero) reference point for the Oriental Motor AZD-KD servo.
 *
 * Procedure:
 * 1. State: Instruct_PositionGimbal
 *    - System displays instruction to position gimbal at desired home position
 *    - Operator manually slews gimbal to the desired home location using joystick
 *    - Current gimbal position displayed in real-time
 *    - Press MENU/VAL to proceed
 *
 * 2. State: Confirm_SetHome
 *    - System asks for confirmation to set current position as home
 *    - Shows current azimuth/elevation values
 *    - Press MENU/VAL to confirm and set home, or wait to cancel
 *
 * 3. State: Completed
 *    - Shows success message
 *    - HR10 pulse sent to PLC42 to set home reference
 *    - Press MENU/VAL to return to main menu
 *
 * @author Claude Code
 * @date December 2025
 */

#include <QObject>
#include <QColor>
#include <QTimer>

// Forward Declarations
class PresetHomePositionViewModel;
class SystemStateModel;
class Plc42Device;

class PresetHomePositionController : public QObject {
    Q_OBJECT

public:
    explicit PresetHomePositionController(QObject* parent = nullptr);

    // ============================================================================
    // Initialization
    // ============================================================================
    void initialize();
    void setViewModel(PresetHomePositionViewModel* viewModel);
    void setStateModel(SystemStateModel* stateModel);
    void setPlc42Device(Plc42Device* plc42);

public slots:
    // ============================================================================
    // UI Control
    // ============================================================================
    void show();
    void hide();

    // ============================================================================
    // Button Handlers
    // ============================================================================
    void onSelectButtonPressed();
    void onBackButtonPressed();
    void onUpButtonPressed();
    void onDownButtonPressed();

signals:
    // ============================================================================
    // Navigation Signals
    // ============================================================================
    void procedureFinished();
    void returnToMainMenu();

private slots:
    // ============================================================================
    // State Handlers
    // ============================================================================
    void onColorStyleChanged(const QColor& color);
    void onPositionUpdate();

private:
    // ============================================================================
    // State Machine
    // ============================================================================
    enum class ProcedureState { Idle, Instruct_PositionGimbal, Confirm_SetHome, Completed };

    void transitionToState(ProcedureState newState);
    void updateUI();
    void executeSetPresetHome();

    // ============================================================================
    // Dependencies
    // ============================================================================
    PresetHomePositionViewModel* m_viewModel = nullptr;
    SystemStateModel* m_stateModel = nullptr;
    Plc42Device* m_plc42 = nullptr;

    // ============================================================================
    // State
    // ============================================================================
    ProcedureState m_currentState = ProcedureState::Idle;

    // Timer for updating position display during procedure
    QTimer* m_updateTimer = nullptr;
    static constexpr int POSITION_UPDATE_INTERVAL_MS = 100;
};


