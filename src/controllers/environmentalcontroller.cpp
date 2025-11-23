#include "environmentalcontroller.h"
#include "models/environmentalviewmodel.h"
#include "models/domain/systemstatemodel.h"
#include <QDebug>

EnvironmentalController::EnvironmentalController(QObject *parent)
    : QObject(parent)
    , m_viewModel(nullptr)
    , m_stateModel(nullptr)
    , m_currentState(EnvironmentalState::Idle)
    , m_currentTemperatureEdit(15.0f)
    , m_currentAltitudeEdit(0.0f)
{
}

void EnvironmentalController::initialize()
{
    Q_ASSERT(m_viewModel);
    Q_ASSERT(m_stateModel);

    // Connect to model changes
    // ✅ LATENCY FIX: Queued connection prevents menu processing from blocking device I/O
    connect(m_stateModel, &SystemStateModel::dataChanged,
            this, [this](std::shared_ptr<const SystemStateData> data) {
                // If environmental mode is externally cancelled
                if (!data->environmentalModeActive && m_currentState != EnvironmentalState::Idle) {
                    qDebug() << "Environmental mode became inactive externally.";
                }
            }, Qt::QueuedConnection);  // Non-blocking signal delivery

    connect(m_stateModel, &SystemStateModel::colorStyleChanged,
            this, &EnvironmentalController::onColorStyleChanged);

    // Set initial color
    const auto& data = m_stateModel->data();
    m_viewModel->setAccentColor(data.colorStyle);
}

void EnvironmentalController::show()
{
    m_stateModel->startEnvironmentalProcedure();
    const auto& data = m_stateModel->data();
    m_currentTemperatureEdit = data.environmentalTemperatureCelsius;
    m_currentAltitudeEdit = data.environmentalAltitudeMeters;
    // NOTE: Crosswind removed - use Windage menu for wind conditions
    transitionToState(EnvironmentalState::Set_Temperature);
    m_viewModel->setVisible(true);
}

void EnvironmentalController::hide()
{
    m_viewModel->setVisible(false);
    transitionToState(EnvironmentalState::Idle);
}

void EnvironmentalController::transitionToState(EnvironmentalState newState)
{
    m_currentState = newState;
    updateUI();
}

void EnvironmentalController::updateUI()
{
    switch (m_currentState) {
    case EnvironmentalState::Set_Temperature:
        m_viewModel->setTitle("Environment (1/2): Temperature");
        m_viewModel->setInstruction(
            "Set air temperature.\n"
            "Use UP/DOWN to adjust. Press SELECT to confirm."
        );
        m_viewModel->setTemperature(m_currentTemperatureEdit);
        m_viewModel->setShowParameters(true);
        m_viewModel->setParameterLabel(
            QString("Temperature: %1°C").arg(m_currentTemperatureEdit, 0, 'f', 1)
        );
        break;

    case EnvironmentalState::Set_Altitude:
        m_viewModel->setTitle("Environment (2/2): Altitude");
        m_viewModel->setInstruction(
            "Set altitude above sea level.\n"
            "Use UP/DOWN to adjust. Press SELECT to confirm.\n\n"
            "NOTE: For wind, use Windage menu."
        );
        m_viewModel->setAltitude(m_currentAltitudeEdit);
        m_viewModel->setShowParameters(true);
        m_viewModel->setParameterLabel(
            QString("Altitude: %1 m").arg(m_currentAltitudeEdit, 0, 'f', 0)
        );
        break;

    case EnvironmentalState::Completed:
    {
        const auto& data = m_stateModel->data();
        m_viewModel->setTitle("Environmental Settings Applied");
        m_viewModel->setInstruction(
            QString("Ballistic calculations updated.\n\n"
                    "Temp: %1°C | Alt: %2m\n\n"
                    "For wind conditions, use Windage menu.\n\n"
                    "Press SELECT to return.")
                .arg(data.environmentalTemperatureCelsius, 0, 'f', 1)
                .arg(data.environmentalAltitudeMeters, 0, 'f', 0)
        );
        m_viewModel->setShowParameters(true);
        m_viewModel->setParameterLabel(
            QString("Temp: %1°C | Alt: %2m (APPLIED)")
                .arg(data.environmentalTemperatureCelsius, 0, 'f', 1)
                .arg(data.environmentalAltitudeMeters, 0, 'f', 0)
        );
    }
        break;

    case EnvironmentalState::Idle:
    default:
        m_viewModel->setTitle("Environmental Settings");
        m_viewModel->setInstruction("");
        m_viewModel->setShowParameters(false);
        break;
    }
}

void EnvironmentalController::onSelectButtonPressed()
{
    switch (m_currentState) {
    case EnvironmentalState::Set_Temperature:
        m_stateModel->setEnvironmentalTemperature(m_currentTemperatureEdit);
        transitionToState(EnvironmentalState::Set_Altitude);
        break;

    case EnvironmentalState::Set_Altitude:
        m_stateModel->setEnvironmentalAltitude(m_currentAltitudeEdit);
        m_stateModel->finalizeEnvironmental();
        qDebug() << "Environmental settings finalized - Temp:" << m_currentTemperatureEdit
                 << "°C, Alt:" << m_currentAltitudeEdit << "m"
                 << "| Use Windage menu for wind conditions";
        transitionToState(EnvironmentalState::Completed);
        break;

    case EnvironmentalState::Completed:
        hide();
        emit returnToMainMenu();
        emit environmentalFinished();
        break;

    default:
        break;
    }
}

void EnvironmentalController::onBackButtonPressed()
{
    SystemStateData currentData = m_stateModel->data();

    if (currentData.environmentalModeActive) {
        if (!currentData.environmentalAppliedToBallistics && m_currentState != EnvironmentalState::Completed) {
            m_stateModel->clearEnvironmental();
        } else {
            SystemStateData updatedData = currentData;
            updatedData.environmentalModeActive = false;
            m_stateModel->updateData(updatedData);
            qDebug() << "EnvironmentalController: Exiting UI, applied settings remain.";
        }
    }

    hide();
    emit returnToMainMenu();
    emit environmentalFinished();
}

void EnvironmentalController::onUpButtonPressed()
{
    switch (m_currentState) {
    case EnvironmentalState::Set_Temperature:
        m_currentTemperatureEdit += 1.0f;
        if (m_currentTemperatureEdit > 60.0f) m_currentTemperatureEdit = 60.0f;
        updateUI();
        break;

    case EnvironmentalState::Set_Altitude:
        m_currentAltitudeEdit += 50.0f;
        if (m_currentAltitudeEdit > 5000.0f) m_currentAltitudeEdit = 5000.0f;
        updateUI();
        break;

    default:
        break;
    }
}

void EnvironmentalController::onDownButtonPressed()
{
    switch (m_currentState) {
    case EnvironmentalState::Set_Temperature:
        m_currentTemperatureEdit -= 1.0f;
        if (m_currentTemperatureEdit < -40.0f) m_currentTemperatureEdit = -40.0f;
        updateUI();
        break;

    case EnvironmentalState::Set_Altitude:
        m_currentAltitudeEdit -= 50.0f;
        if (m_currentAltitudeEdit < -100.0f) m_currentAltitudeEdit = -100.0f;
        updateUI();
        break;

    default:
        break;
    }
}

void EnvironmentalController::onColorStyleChanged(const QColor& color)
{
    qDebug() << "EnvironmentalController: Color changed to" << color;
    m_viewModel->setAccentColor(color);
}

void EnvironmentalController::setViewModel(EnvironmentalViewModel* viewModel)
{
    m_viewModel = viewModel;
}

void EnvironmentalController::setStateModel(SystemStateModel* stateModel)
{
    m_stateModel = stateModel;
}
