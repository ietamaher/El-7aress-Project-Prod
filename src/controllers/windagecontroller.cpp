#include "windagecontroller.h"
#include "models/windageviewmodel.h"
#include "models/domain/systemstatemodel.h"
#include <QDebug>

WindageController::WindageController(QObject *parent)
    : QObject(parent)
    , m_viewModel(nullptr)
    , m_stateModel(nullptr)
    , m_currentState(WindageState::Idle)
    , m_currentWindSpeedEdit(0.0f)
{
}

void WindageController::initialize()
{
    Q_ASSERT(m_viewModel);
    Q_ASSERT(m_stateModel);

    // Connect to model changes
    // ✅ LATENCY FIX: Queued connection prevents menu processing from blocking device I/O
    connect(m_stateModel, &SystemStateModel::dataChanged,
            this, [this](std::shared_ptr<const SystemStateData> data) {
                // If windage is externally cancelled
                if (!data->windageModeActive && m_currentState != WindageState::Idle) {
                    qDebug() << "Windage mode became inactive externally.";
                }

                // Note: We do NOT sync m_currentWindSpeedEdit from the model during editing.
                // The controller maintains its own edit value independently until SELECT confirms it.
            }, Qt::QueuedConnection);  // Non-blocking signal delivery

    connect(m_stateModel, &SystemStateModel::colorStyleChanged,
            this, &WindageController::onColorStyleChanged);
    
    // Set initial color
    const auto& data = m_stateModel->data();
    m_viewModel->setAccentColor(data.colorStyle);
}

void WindageController::show()
{
    m_stateModel->startWindageProcedure();
    m_currentWindSpeedEdit = m_stateModel->data().windageSpeedKnots;
    transitionToState(WindageState::Instruct_AlignToWind);
    m_viewModel->setVisible(true);
}

void WindageController::hide()
{
    m_viewModel->setVisible(false);
    transitionToState(WindageState::Idle);
}

void WindageController::transitionToState(WindageState newState)
{
    m_currentState = newState;
    updateUI();
}

void WindageController::updateUI()
{
    switch (m_currentState) {
    case WindageState::Instruct_AlignToWind:
        m_viewModel->setTitle("Windage (1/2): Alignment");
        m_viewModel->setInstruction(
            "Align Weapon Station TOWARDS THE WIND using joystick.\n\n"
            "Press SELECT when aligned."
            );
        m_viewModel->setShowWindSpeed(false);
        m_viewModel->setShowWindDirection(false);
        break;

    case WindageState::Set_WindSpeed:
    {
        const SystemStateData& data = m_stateModel->data();
        m_viewModel->setTitle("Windage (2/2): Speed");
        m_viewModel->setInstruction(
            "Set HEADWIND speed.\n"
            "Use UP/DOWN to adjust. Press SELECT to confirm."
            );
        m_viewModel->setWindSpeed(m_currentWindSpeedEdit);
        m_viewModel->setShowWindSpeed(true);

        // Always update the label for editing state (no "APPLIED" suffix)
        m_viewModel->setWindSpeedLabel(
            QString("Headwind: %1 knots").arg(m_currentWindSpeedEdit, 0, 'f', 0)
            );

        // Show captured wind direction
        if (data.windageDirectionCaptured) {
            m_viewModel->setWindDirection(data.windageDirectionDegrees);
            m_viewModel->setWindDirectionLabel(
                QString("Wind FROM: %1°").arg(data.windageDirectionDegrees, 0, 'f', 0)
                );
            m_viewModel->setShowWindDirection(true);
        }
        break;
    }

    case WindageState::Completed:
    {
        const SystemStateData& data = m_stateModel->data();
        m_viewModel->setTitle("Windage Set");
        m_viewModel->setInstruction(
            QString("Windage set to %1° @ %2 knots and applied.\n"
                    "'W' will display on OSD.\n\n"
                    "Press SELECT to return.")
                .arg(data.windageDirectionDegrees, 0, 'f', 0)
                .arg(data.windageSpeedKnots, 0, 'f', 0)
            );
        m_viewModel->setWindSpeed(data.windageSpeedKnots);
        m_viewModel->setShowWindSpeed(true);
        m_viewModel->setWindSpeedLabel(
            QString("Headwind: %1 knots (APPLIED)")
                .arg(data.windageSpeedKnots, 0, 'f', 0)
            );

        // Show applied wind direction
        m_viewModel->setWindDirection(data.windageDirectionDegrees);
        m_viewModel->setWindDirectionLabel(
            QString("Wind FROM: %1° (APPLIED)").arg(data.windageDirectionDegrees, 0, 'f', 0)
            );
        m_viewModel->setShowWindDirection(true);
        break;
    }

    case WindageState::Idle:
    default:
        m_viewModel->setTitle("Windage Setting");
        m_viewModel->setInstruction("");
        m_viewModel->setShowWindSpeed(false);
        m_viewModel->setShowWindDirection(false);
        break;
    }
}

void WindageController::onSelectButtonPressed()
{
    switch (m_currentState) {
    case WindageState::Instruct_AlignToWind:
    {
        // Capture wind direction from current absolute weapon bearing
        // Wind direction is ABSOLUTE (relative to true North), not relative to vehicle!
        // Therefore we must use: Absolute bearing = IMU yaw + Station azimuth
        const SystemStateData& data = m_stateModel->data();

        float absoluteBearing;

        // Check if IMU is connected
        if (!data.imuConnected) {
            // ⚠️ WARNING: IMU not connected! Fallback to station azimuth only
            // This is NOT correct for moving vehicles, but better than nothing
            absoluteBearing = data.azimuthDirection;
            qCritical() << "⚠️ WARNING: IMU NOT CONNECTED! Wind direction captured as station azimuth only:" << absoluteBearing << "°";
            qCritical() << "Wind direction will be INCORRECT if vehicle rotates!";
            qCritical() << "Please connect IMU for accurate windage.";
        } else {
            // ✅ IMU connected: Calculate correct absolute gimbal bearing
            absoluteBearing = static_cast<float>(data.imuYawDeg) + data.azimuthDirection;

            // Normalize to 0-360 range
            while (absoluteBearing >= 360.0f) absoluteBearing -= 360.0f;
            while (absoluteBearing < 0.0f) absoluteBearing += 360.0f;

            qDebug() << "Wind direction captured - Vehicle Heading (IMU):" << data.imuYawDeg << "°"
                     << "Station Az:" << data.azimuthDirection << "°"
                     << "Absolute Bearing:" << absoluteBearing << "degrees (wind FROM this direction)";
        }

        m_stateModel->captureWindageDirection(absoluteBearing);

        // Load current wind speed for editing
        m_currentWindSpeedEdit = data.windageSpeedKnots;
        transitionToState(WindageState::Set_WindSpeed);
    }
    break;

    case WindageState::Set_WindSpeed:
    {
        // Set wind speed and finalize
        m_stateModel->setWindageSpeed(m_currentWindSpeedEdit);
        m_stateModel->finalizeWindage();

        const SystemStateData& data = m_stateModel->data();
        qDebug() << "Windage finalized - Direction:" << data.windageDirectionDegrees
                 << "degrees, Speed:" << m_currentWindSpeedEdit << "knots";

        transitionToState(WindageState::Completed);
    }
    break;

    case WindageState::Completed:
        hide();
        emit returnToMainMenu();
        emit windageFinished();
        break;

    default:
        break;
    }
}

void WindageController::onBackButtonPressed()
{
    SystemStateData currentData = m_stateModel->data();

    if (currentData.windageModeActive) {
        if (!currentData.windageAppliedToBallistics && m_currentState != WindageState::Completed) {
            m_stateModel->clearWindage();
        } else {
            SystemStateData updatedData = currentData;
            updatedData.windageModeActive = false;
            m_stateModel->updateData(updatedData);
            qDebug() << "WindageController: Exiting UI, applied windage remains.";
        }
    }

    hide();
    emit returnToMainMenu();
    emit windageFinished();
}

void WindageController::onUpButtonPressed()
{
    if (m_currentState == WindageState::Set_WindSpeed) {
        m_currentWindSpeedEdit += 1.0f;
        if (m_currentWindSpeedEdit > 50) m_currentWindSpeedEdit = 50;
        updateUI();
    }
}

void WindageController::onDownButtonPressed()
{
    if (m_currentState == WindageState::Set_WindSpeed) {
        m_currentWindSpeedEdit -= 1.0f;
        if (m_currentWindSpeedEdit < 0) m_currentWindSpeedEdit = 0;
        updateUI();
    }
}

void WindageController::onColorStyleChanged(const QColor& color)
{
    qDebug() << "WindageController: Color changed to" << color;
    m_viewModel->setAccentColor(color);
}

void WindageController::setViewModel(WindageViewModel* viewModel)
{
    m_viewModel = viewModel;
}

void WindageController::setStateModel(SystemStateModel* stateModel)
{
    m_stateModel = stateModel;
}
