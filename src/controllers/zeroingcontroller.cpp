#include "zeroingcontroller.h"

// ============================================================================
// Project
// ============================================================================
#include "models/zeroingviewmodel.h"
#include "models/domain/systemstatemodel.h"

// ============================================================================
// Qt Framework
// ============================================================================
#include <QDebug>

// ============================================================================
// CONSTRUCTOR
// ============================================================================

ZeroingController::ZeroingController(QObject* parent)
    : QObject(parent)
{
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void ZeroingController::initialize()
{
    Q_ASSERT(m_viewModel);
    Q_ASSERT(m_stateModel);

    // LATENCY FIX: Use dedicated zeroingModeChanged signal to reduce event queue load
    // Only processes ~5 events per menu session instead of 1,200 events/min from dataChanged
    connect(m_stateModel, &SystemStateModel::zeroingModeChanged,
            this, [this](bool active) {
                if (!active && m_currentState == ZeroingState::Instruct_MoveReticleToImpact) {
                    qDebug() << "[ZeroingController] Zeroing cancelled externally during instruction phase";
                    hide();
                    emit zeroingFinished();
                }
            }, Qt::QueuedConnection);

    connect(m_stateModel, &SystemStateModel::colorStyleChanged,
            this, &ZeroingController::onColorStyleChanged);

    // Set initial color
    const auto& data = m_stateModel->data();
    m_viewModel->setAccentColor(data.colorStyle);
}

void ZeroingController::setViewModel(ZeroingViewModel* viewModel)
{
    m_viewModel = viewModel;
}

void ZeroingController::setStateModel(SystemStateModel* stateModel)
{
    m_stateModel = stateModel;
}

// ============================================================================
// UI CONTROL
// ============================================================================

void ZeroingController::show()
{
    qDebug() << "[ZeroingController] show() called";

    m_stateModel->startZeroingProcedure();
    transitionToState(ZeroingState::Instruct_MoveReticleToImpact);
    m_viewModel->setVisible(true);

    qDebug() << "[ZeroingController] Now in Instruct_MoveReticleToImpact state";
}

void ZeroingController::hide()
{
    qDebug() << "[ZeroingController] hide() called";

    m_viewModel->setVisible(false);
    transitionToState(ZeroingState::Idle);
}

// ============================================================================
// STATE MACHINE
// ============================================================================

void ZeroingController::transitionToState(ZeroingState newState)
{
    qDebug() << "[ZeroingController] State transition from"
             << static_cast<int>(m_currentState) << "to" << static_cast<int>(newState);

    m_currentState = newState;
    updateUI();
}

void ZeroingController::updateUI()
{
    qDebug() << "[ZeroingController] updateUI() for state" << static_cast<int>(m_currentState);

    switch (m_currentState) {
    case ZeroingState::Instruct_MoveReticleToImpact:
        m_viewModel->setTitle("Weapon Zeroing: Adjust");
        m_viewModel->setInstruction(
            "ZEROING\n\n"
            "1. (Fire weapon at a fixed target)\n"
            "2. Observe impact point.\n"
            "3. Use JOYSTICK to move main RETICLE to the ACTUAL IMPACT POINT.\n\n"
            "Press MENU/VAL to apply this as the new zero."
        );
        m_viewModel->setStatus("ADJUSTING RETICLE TO IMPACT");
        m_viewModel->setShowOffsets(false);
        break;

    case ZeroingState::Completed:
        m_viewModel->setTitle("Zeroing Applied");
        m_viewModel->setInstruction(
            "Zeroing Adjustment Applied!\n"
            "'Z' will display on OSD when active.\n\n"
            "Press MENU/VAL to return to Main Menu."
        );
        m_viewModel->setStatus(QString("FINAL OFFSETS: Az %1, El %2")
            .arg(m_stateModel->data().zeroingAzimuthOffset, 0, 'f', 2)
            .arg(m_stateModel->data().zeroingElevationOffset, 0, 'f', 2));
        m_viewModel->setShowOffsets(true);
        m_viewModel->setAzimuthOffset(m_stateModel->data().zeroingAzimuthOffset);
        m_viewModel->setElevationOffset(m_stateModel->data().zeroingElevationOffset);
        qDebug() << "[ZeroingController] Completion screen should now be visible";
        break;

    case ZeroingState::Idle:
    default:
        m_viewModel->setTitle("Weapon Zeroing");
        m_viewModel->setInstruction("Zeroing Standby.");
        m_viewModel->setStatus("");
        m_viewModel->setShowOffsets(false);
        break;
    }
}

// ============================================================================
// BUTTON HANDLERS
// ============================================================================

void ZeroingController::onSelectButtonPressed()
{
    qDebug() << "[ZeroingController] onSelectButtonPressed() - Current state:"
             << static_cast<int>(m_currentState);

    switch (m_currentState) {
    case ZeroingState::Instruct_MoveReticleToImpact:
        // ========================================================================
        // BUG FIX: FINALIZE ZEROING BEFORE TRANSITIONING TO COMPLETED STATE
        // ========================================================================
        // Previous code called transitionToState() FIRST, which updated the UI
        // with OLD offset values. Now we finalize zeroing FIRST so the UI
        // displays the correct new offset values.
        // ========================================================================
        m_stateModel->finalizeZeroing();
        transitionToState(ZeroingState::Completed);
        break;

    case ZeroingState::Completed:
        hide();
        emit returnToMainMenu();
        emit zeroingFinished();
        break;

    default:
        break;
    }
}

void ZeroingController::onBackButtonPressed()
{
    // NOTE: This method exists for compatibility but won't be called
    // since there's no physical BACK button anymore
    // Only MENU/VAL acts as SELECT in procedures

    SystemStateData currentData = m_stateModel->data();

    if (currentData.zeroingModeActive) {
        if (!currentData.zeroingAppliedToBallistics && m_currentState != ZeroingState::Completed) {
            m_stateModel->clearZeroing();
        } else {
            SystemStateData updatedData = currentData;
            updatedData.zeroingModeActive = false;
            m_stateModel->updateData(updatedData);
            qDebug() << "[ZeroingController] Exiting UI, applied zeroing remains.";
        }
    }

    hide();
    emit returnToMainMenu();
    emit zeroingFinished();
}

void ZeroingController::onUpButtonPressed()
{
    // TODO: Implement fine-tuning if needed
}

void ZeroingController::onDownButtonPressed()
{
    // TODO: Implement fine-tuning if needed
}

// ============================================================================
// COLOR STYLE HANDLER
// ============================================================================

void ZeroingController::onColorStyleChanged(const QColor& color)
{
    qDebug() << "[ZeroingController] Color changed to" << color;
    m_viewModel->setAccentColor(color);
}