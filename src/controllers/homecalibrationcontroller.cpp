#include "homecalibrationcontroller.h"

/**
 * @file HomeCalibrationController.cpp
 * @brief Implementation of HomeCalibrationController
 *
 * @author Claude Code
 * @date December 2025
 */

// ============================================================================
// Project
// ============================================================================
#include "models/homecalibrationviewmodel.h"
#include "models/domain/systemstatemodel.h"

// ============================================================================
// Qt Framework
// ============================================================================
#include <QDebug>

// ============================================================================
// CONSTRUCTOR
// ============================================================================

HomeCalibrationController::HomeCalibrationController(QObject* parent)
    : QObject(parent)
{
    // Create position update timer
    m_updateTimer = new QTimer(this);
    m_updateTimer->setInterval(POSITION_UPDATE_INTERVAL_MS);
    connect(m_updateTimer, &QTimer::timeout, this, &HomeCalibrationController::onPositionUpdate);
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void HomeCalibrationController::initialize()
{
    Q_ASSERT(m_viewModel);
    Q_ASSERT(m_stateModel);

    // Connect to color style changes
    connect(m_stateModel, &SystemStateModel::colorStyleChanged,
            this, &HomeCalibrationController::onColorStyleChanged);

    // Connect to home calibration mode changes for external cancellation
    connect(m_stateModel, &SystemStateModel::homeCalibrationModeChanged,
            this, [this](bool active) {
                if (!active && m_currentState != CalibrationState::Idle &&
                    m_currentState != CalibrationState::Completed) {
                    qDebug() << "[HomeCalibrationController] Calibration cancelled externally";
                    hide();
                    emit calibrationFinished();
                }
            }, Qt::QueuedConnection);

    // Set initial color
    const auto& data = m_stateModel->data();
    m_viewModel->setAccentColor(data.colorStyle);
}

void HomeCalibrationController::setViewModel(HomeCalibrationViewModel* viewModel)
{
    m_viewModel = viewModel;
}

void HomeCalibrationController::setStateModel(SystemStateModel* stateModel)
{
    m_stateModel = stateModel;
}

// ============================================================================
// UI CONTROL
// ============================================================================

void HomeCalibrationController::show()
{
    qDebug() << "[HomeCalibrationController] show() called";

    m_stateModel->startHomeCalibrationProcedure();
    transitionToState(CalibrationState::Instruct_GoToEncoderHome);
    m_viewModel->setVisible(true);

    // Start position update timer
    m_updateTimer->start();

    qDebug() << "[HomeCalibrationController] Now in Instruct_GoToEncoderHome state";
}

void HomeCalibrationController::hide()
{
    qDebug() << "[HomeCalibrationController] hide() called";

    // Stop position update timer
    m_updateTimer->stop();

    m_viewModel->setVisible(false);
    transitionToState(CalibrationState::Idle);
}

// ============================================================================
// STATE MACHINE
// ============================================================================

void HomeCalibrationController::transitionToState(CalibrationState newState)
{
    qDebug() << "[HomeCalibrationController] State transition from"
             << static_cast<int>(m_currentState) << "to" << static_cast<int>(newState);

    m_currentState = newState;
    updateUI();
}

void HomeCalibrationController::updateUI()
{
    qDebug() << "[HomeCalibrationController] updateUI() for state" << static_cast<int>(m_currentState);

    // Update position data
    const auto& data = m_stateModel->data();
    m_viewModel->setCurrentEncoderSteps(m_stateModel->getCurrentEncoderPositionSteps());
    m_viewModel->setCurrentOffsetSteps(data.azHomeOffsetSteps);
    m_viewModel->setCurrentAzimuthDeg(data.gimbalAz);
    m_viewModel->setOffsetApplied(data.azHomeOffsetApplied);

    switch (m_currentState) {
    case CalibrationState::Instruct_GoToEncoderHome:
        m_viewModel->setTitle("Az Home Calibration: Step 1");
        m_viewModel->setInstruction(
            "AZIMUTH HOME DRIFT DETECTED?\n\n"
            "1. Observe the current gimbal position.\n"
            "2. If gimbal is NOT at the true mechanical home,\n"
            "   this indicates encoder drift has occurred.\n\n"
            "Press MENU/VAL to proceed to manual alignment."
        );
        m_viewModel->setStatus("OBSERVING ENCODER REFERENCE POSITION");
        break;

    case CalibrationState::Instruct_SlewToTrueHome:
        m_viewModel->setTitle("Az Home Calibration: Step 2");
        m_viewModel->setInstruction(
            "MANUAL ALIGNMENT\n\n"
            "1. Use JOYSTICK to slew gimbal to the\n"
            "   TRUE MECHANICAL HOME position.\n"
            "2. Align with visual reference mark.\n"
            "3. Encoder position shown below will become\n"
            "   the new zero reference.\n\n"
            "Press MENU/VAL when aligned to SET TRUE HOME."
        );
        m_viewModel->setStatus("SLEWING TO TRUE HOME - USE JOYSTICK");
        break;

    case CalibrationState::Completed:
        m_viewModel->setTitle("Az Home Calibration: Complete");
        m_viewModel->setInstruction(
            "HOME OFFSET CALIBRATION APPLIED!\n\n"
            "The azimuth position display now reads\n"
            "relative to the true mechanical home.\n\n"
            "Calibration saved to persistent storage.\n"
            "'H' will display on OSD when offset is active.\n\n"
            "Press MENU/VAL to return to Main Menu."
        );
        m_viewModel->setStatus(QString("OFFSET APPLIED: %1 steps")
            .arg(data.azHomeOffsetSteps, 0, 'f', 0));
        break;

    case CalibrationState::Idle:
    default:
        m_viewModel->setTitle("Azimuth Home Calibration");
        m_viewModel->setInstruction("Calibration Standby.");
        m_viewModel->setStatus("");
        break;
    }
}

// ============================================================================
// BUTTON HANDLERS
// ============================================================================

void HomeCalibrationController::onSelectButtonPressed()
{
    qDebug() << "[HomeCalibrationController] onSelectButtonPressed() - Current state:"
             << static_cast<int>(m_currentState);

    switch (m_currentState) {
    case CalibrationState::Instruct_GoToEncoderHome:
        // Proceed to manual alignment step
        transitionToState(CalibrationState::Instruct_SlewToTrueHome);
        break;

    case CalibrationState::Instruct_SlewToTrueHome:
        // Capture current position as true home
        m_stateModel->captureAzHomeOffset();
        m_stateModel->finalizeHomeCalibration();
        transitionToState(CalibrationState::Completed);
        break;

    case CalibrationState::Completed:
        hide();
        emit returnToMainMenu();
        emit calibrationFinished();
        break;

    default:
        break;
    }
}

void HomeCalibrationController::onBackButtonPressed()
{
    // NOTE: This method exists for compatibility
    // BACK could be used to cancel calibration without saving

    qDebug() << "[HomeCalibrationController] onBackButtonPressed() - cancelling";

    // If calibration was in progress but not completed, don't save
    if (m_currentState == CalibrationState::Instruct_GoToEncoderHome ||
        m_currentState == CalibrationState::Instruct_SlewToTrueHome) {
        qDebug() << "[HomeCalibrationController] Calibration cancelled, offset not saved";
        // Don't call finalizeHomeCalibration - offset is not saved
        // Reset the mode flag
        auto data = m_stateModel->data();
        data.homeCalibrationModeActive = false;
        m_stateModel->updateData(data);
    }

    hide();
    emit returnToMainMenu();
    emit calibrationFinished();
}

void HomeCalibrationController::onUpButtonPressed()
{
    // Reserved for future use (e.g., fine-tune offset value)
}

void HomeCalibrationController::onDownButtonPressed()
{
    // Reserved for future use (e.g., fine-tune offset value)
}

// ============================================================================
// POSITION UPDATE HANDLER
// ============================================================================

void HomeCalibrationController::onPositionUpdate()
{
    // Update ViewModel with current position data while calibrating
    if (m_currentState != CalibrationState::Idle) {
        const auto& data = m_stateModel->data();
        m_viewModel->setCurrentEncoderSteps(m_stateModel->getCurrentEncoderPositionSteps());
        m_viewModel->setCurrentOffsetSteps(data.azHomeOffsetSteps);
        m_viewModel->setCurrentAzimuthDeg(data.gimbalAz);
        m_viewModel->setOffsetApplied(data.azHomeOffsetApplied);
    }
}

// ============================================================================
// COLOR STYLE HANDLER
// ============================================================================

void HomeCalibrationController::onColorStyleChanged(const QColor& color)
{
    qDebug() << "[HomeCalibrationController] Color changed to" << color;
    m_viewModel->setAccentColor(color);
}
