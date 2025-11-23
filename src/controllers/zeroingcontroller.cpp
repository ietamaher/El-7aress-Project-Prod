#include "zeroingcontroller.h"
#include "models/zeroingviewmodel.h"
#include "models/domain/systemstatemodel.h"
#include <QDebug>

ZeroingController::ZeroingController(QObject *parent)
    : QObject(parent)
    , m_viewModel(nullptr)
    , m_stateModel(nullptr)
    , m_currentState(ZeroingState::Idle)
{
}

void ZeroingController::initialize()
{
    /*m_viewModel = ServiceManager::instance()->get<ZeroingViewModel>(QString("ZeroingViewModel"));
    m_stateModel = ServiceManager::instance()->get<SystemStateModel>();*/

    Q_ASSERT(m_viewModel);
    Q_ASSERT(m_stateModel);

    // REPLACE THIS ENTIRE CONNECT BLOCK:
    // ✅ LATENCY FIX: Queued connection prevents menu processing from blocking device I/O
    connect(m_stateModel, &SystemStateModel::dataChanged,
            this, [this](const SystemStateData& data) {
                // OLD: if (!data.zeroingModeActive && m_currentState != ZeroingState::Idle && m_currentState != ZeroingState::Completed)

                // NEW: Only check for Instruct state specifically
                if (!data.zeroingModeActive && m_currentState == ZeroingState::Instruct_MoveReticleToImpact) {
                    qDebug() << "Zeroing cancelled externally during instruction phase";
                    hide();
                    emit zeroingFinished();
                }
            }, Qt::QueuedConnection);  // Non-blocking signal delivery
    connect(m_stateModel, &SystemStateModel::colorStyleChanged,
            this, &ZeroingController::onColorStyleChanged);
    
    // Set initial color
    const auto& data = m_stateModel->data();
    m_viewModel->setAccentColor(data.colorStyle);
}

void ZeroingController::show()
{
    qDebug() << "ZeroingController::show() called";
    m_stateModel->startZeroingProcedure();
    transitionToState(ZeroingState::Instruct_MoveReticleToImpact);
    m_viewModel->setVisible(true);
    qDebug() << "ZeroingController: Now in Instruct_MoveReticleToImpact state";
}

void ZeroingController::hide()
{
    qDebug() << "ZeroingController::hide() called";
    m_viewModel->setVisible(false);
    transitionToState(ZeroingState::Idle);
}

void ZeroingController::transitionToState(ZeroingState newState)
{
    qDebug() << "ZeroingController: State transition from" << static_cast<int>(m_currentState)
    << "to" << static_cast<int>(newState);
    m_currentState = newState;
    updateUI();
}

void ZeroingController::updateUI()
{
    qDebug() << "ZeroingController::updateUI() for state" << static_cast<int>(m_currentState);

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
        qDebug() << "ZeroingController: Completion screen should now be visible";
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

void ZeroingController::onSelectButtonPressed()
{
    qDebug() << "ZeroingController::onSelectButtonPressed() called";
    qDebug() << "ZeroingController: Current state =" << static_cast<int>(m_currentState);

    switch (m_currentState) {
    case ZeroingState::Instruct_MoveReticleToImpact:
        transitionToState(ZeroingState::Completed);  // ← STATE FIRST
        m_stateModel->finalizeZeroing();              // ← THEN FINALIZE
        break;

    case ZeroingState::Completed:
        hide();
        emit returnToMainMenu();
        emit zeroingFinished();
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
            qDebug() << "ZeroingController: Exiting UI, applied zeroing remains.";
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

void ZeroingController::onColorStyleChanged(const QColor& color)
{
    qDebug() << "ZeroingController: Color changed to" << color;
    m_viewModel->setAccentColor(color);
}

void ZeroingController::setViewModel(ZeroingViewModel* viewModel)
{
    m_viewModel = viewModel;
}

void ZeroingController::setStateModel(SystemStateModel* stateModel)
{
    m_stateModel = stateModel;
}