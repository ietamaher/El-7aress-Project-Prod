#include "presethomepositioncontroller.h"

/**
 * @file PresetHomePositionController.cpp
 * @brief Implementation of PresetHomePositionController
 *
 * @author Claude Code
 * @date December 2025
 */

// ============================================================================
// Project
// ============================================================================
#include "models/presethomepositionviewmodel.h"
#include "models/domain/systemstatemodel.h"
#include "hardware/devices/plc42device.h"

// ============================================================================
// Qt Framework
// ============================================================================
#include <QDebug>

// ============================================================================
// CONSTRUCTOR
// ============================================================================

PresetHomePositionController::PresetHomePositionController(QObject* parent) : QObject(parent) {
    // Create position update timer
    m_updateTimer = new QTimer(this);
    m_updateTimer->setInterval(POSITION_UPDATE_INTERVAL_MS);
    connect(m_updateTimer, &QTimer::timeout, this, &PresetHomePositionController::onPositionUpdate);
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void PresetHomePositionController::initialize() {
    Q_ASSERT(m_viewModel);
    Q_ASSERT(m_stateModel);
    // m_plc42 can be null during testing

    // Connect to color style changes
    connect(m_stateModel, &SystemStateModel::colorStyleChanged, this,
            &PresetHomePositionController::onColorStyleChanged);

    // Set initial color
    const auto& data = m_stateModel->data();
    m_viewModel->setAccentColor(data.colorStyle);
}

void PresetHomePositionController::setViewModel(PresetHomePositionViewModel* viewModel) {
    m_viewModel = viewModel;
}

void PresetHomePositionController::setStateModel(SystemStateModel* stateModel) {
    m_stateModel = stateModel;
}

void PresetHomePositionController::setPlc42Device(Plc42Device* plc42) {
    m_plc42 = plc42;
}

// ============================================================================
// UI CONTROL
// ============================================================================

void PresetHomePositionController::show() {
    qDebug() << "[PresetHomePositionController] show() called";

    transitionToState(ProcedureState::Instruct_PositionGimbal);
    m_viewModel->setVisible(true);

    // Start position update timer
    m_updateTimer->start();

    qDebug() << "[PresetHomePositionController] Now in Instruct_PositionGimbal state";
}

void PresetHomePositionController::hide() {
    qDebug() << "[PresetHomePositionController] hide() called";

    // Stop position update timer
    m_updateTimer->stop();

    m_viewModel->setVisible(false);
    transitionToState(ProcedureState::Idle);
}

// ============================================================================
// STATE MACHINE
// ============================================================================

void PresetHomePositionController::transitionToState(ProcedureState newState) {
    qDebug() << "[PresetHomePositionController] State transition from"
             << static_cast<int>(m_currentState) << "to" << static_cast<int>(newState);

    m_currentState = newState;
    updateUI();
}

void PresetHomePositionController::updateUI() {
    qDebug() << "[PresetHomePositionController] updateUI() for state"
             << static_cast<int>(m_currentState);

    // Update position data
    const auto& data = m_stateModel->data();
    m_viewModel->setCurrentAzimuthDeg(data.gimbalAz);
    m_viewModel->setCurrentElevationDeg(data.gimbalEl);

    switch (m_currentState) {
    case ProcedureState::Instruct_PositionGimbal:
        m_viewModel->setTitle("Set Preset Home Position: Step 1");
        m_viewModel->setInstruction("POSITION GIMBAL TO HOME LOCATION\n\n"
                                    "1. Use the JOYSTICK to slew the gimbal to the\n"
                                    "   desired HOME position.\n"
                                    "2. Align with your visual/mechanical reference.\n"
                                    "3. Current position is displayed below.\n\n"
                                    "Press MENU/VAL when positioned correctly.");
        m_viewModel->setStatus("POSITIONING GIMBAL - USE JOYSTICK");
        break;

    case ProcedureState::Confirm_SetHome:
        m_viewModel->setTitle("Set Preset Home Position: Step 2");
        m_viewModel->setInstruction("CONFIRM HOME POSITION\n\n"
                                    "You are about to set the current gimbal position\n"
                                    "as the HOME reference point.\n\n"
                                    "The motor controller will store this position\n"
                                    "as the zero reference for future homing operations.\n\n"
                                    "Press MENU/VAL to CONFIRM and set home.");
        m_viewModel->setStatus(QString("AZ: %1 deg  |  EL: %2 deg")
                                   .arg(data.gimbalAz, 0, 'f', 2)
                                   .arg(data.gimbalEl, 0, 'f', 2));
        break;

    case ProcedureState::Completed:
        m_viewModel->setTitle("Set Preset Home Position: Complete");
        m_viewModel->setInstruction("PRESET HOME POSITION SET!\n\n"
                                    "The current position has been stored as the\n"
                                    "home reference point in the motor controller.\n\n"
                                    "Future HOME operations will return the gimbal\n"
                                    "to this position.\n\n"
                                    "Press MENU/VAL to return to Main Menu.");
        m_viewModel->setStatus("HOME POSITION SAVED SUCCESSFULLY");
        break;

    case ProcedureState::Idle:
    default:
        m_viewModel->setTitle("Preset Home Position");
        m_viewModel->setInstruction("Procedure Standby.");
        m_viewModel->setStatus("");
        break;
    }
}

// ============================================================================
// BUTTON HANDLERS
// ============================================================================

void PresetHomePositionController::onSelectButtonPressed() {
    qDebug() << "[PresetHomePositionController] onSelectButtonPressed() - Current state:"
             << static_cast<int>(m_currentState);

    switch (m_currentState) {
    case ProcedureState::Instruct_PositionGimbal:
        // Proceed to confirmation step
        transitionToState(ProcedureState::Confirm_SetHome);
        break;

    case ProcedureState::Confirm_SetHome:
        // Execute the preset home command
        executeSetPresetHome();
        transitionToState(ProcedureState::Completed);
        break;

    case ProcedureState::Completed:
        hide();
        emit returnToMainMenu();
        emit procedureFinished();
        break;

    default:
        break;
    }
}

void PresetHomePositionController::onBackButtonPressed() {
    qDebug() << "[PresetHomePositionController] onBackButtonPressed() - cancelling";

    hide();
    emit returnToMainMenu();
    emit procedureFinished();
}

void PresetHomePositionController::onUpButtonPressed() {
    // Reserved for future use
}

void PresetHomePositionController::onDownButtonPressed() {
    // Reserved for future use
}

// ============================================================================
// EXECUTE SET PRESET HOME
// ============================================================================

void PresetHomePositionController::executeSetPresetHome() {
    qDebug() << "[PresetHomePositionController] Executing setPresetHomePosition...";

    if (m_plc42) {
        // Send the HR10 command to set current position as home
        m_plc42->setPresetHomePosition();
        qInfo() << "[PresetHomePositionController] Preset home position command sent to PLC42";
    } else {
        qWarning() << "[PresetHomePositionController] PLC42 device not available!";
    }
}

// ============================================================================
// POSITION UPDATE HANDLER
// ============================================================================

void PresetHomePositionController::onPositionUpdate() {
    // Update ViewModel with current position data while procedure is active
    if (m_currentState != ProcedureState::Idle) {
        const auto& data = m_stateModel->data();
        m_viewModel->setCurrentAzimuthDeg(data.gimbalAz);
        m_viewModel->setCurrentElevationDeg(data.gimbalEl);
    }
}

// ============================================================================
// COLOR STYLE HANDLER
// ============================================================================

void PresetHomePositionController::onColorStyleChanged(const QColor& color) {
    qDebug() << "[PresetHomePositionController] Color changed to" << color;
    m_viewModel->setAccentColor(color);
}
