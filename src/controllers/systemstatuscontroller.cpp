#include "systemstatuscontroller.h"
#include "models/systemstatusviewmodel.h"
#include "models/domain/systemstatemodel.h"
#include <QDebug>

SystemStatusController::SystemStatusController(QObject *parent)
    : QObject(parent)
    , m_viewModel(nullptr)
    , m_stateModel(nullptr)
{
}

void SystemStatusController::setViewModel(SystemStatusViewModel* viewModel)
{
    m_viewModel = viewModel;
    qDebug() << "SystemStatusController: ViewModel set";
}

void SystemStatusController::setStateModel(SystemStateModel* stateModel)
{
    m_stateModel = stateModel;
    qDebug() << "SystemStatusController: StateModel set";
}

void SystemStatusController::initialize()
{
    qDebug() << "SystemStatusController::initialize()";
    Q_ASSERT(m_viewModel);
    Q_ASSERT(m_stateModel);

    if (!m_viewModel) {
        qCritical() << "SystemStatusController: ViewModel is null!";
        return;
    }

    if (!m_stateModel) {
        qCritical() << "SystemStatusController: StateModel is null!";
        return;
    }

    // Connect to SystemStateModel updates
    // ✅ LATENCY FIX: Queued connection prevents status aggregation from blocking device I/O
    connect(m_stateModel, &SystemStateModel::dataChanged,
            this, &SystemStatusController::onSystemStateChanged,
            Qt::QueuedConnection);  // Non-blocking signal delivery

    // Connect ViewModel actions to controller
    connect(m_viewModel, &SystemStatusViewModel::clearAlarmsRequested,
            this, &SystemStatusController::onClearAlarmsRequested);

    // Connect to color style changes
    connect(m_stateModel, &SystemStateModel::colorStyleChanged,
            this, &SystemStatusController::onColorStyleChanged);

    // Set initial color
    const auto& data = m_stateModel->data();
    m_viewModel->setAccentColor(data.colorStyle);

    qDebug() << "SystemStatusController initialized successfully";
}

void SystemStatusController::show()
{
    if (m_viewModel) {
        m_viewModel->setVisible(true);
    }
}

void SystemStatusController::hide()
{
    if (m_viewModel) {
        m_viewModel->setVisible(false);
    }
}

void SystemStatusController::onSystemStateChanged(const SystemStateData& data)
{
    if (!m_viewModel) return;

    // Update Azimuth Servo
    m_viewModel->updateAzimuthServo(
        data.azServoConnected,
        data.gimbalAz,
        data.azRpm,
        data.azTorque,
        data.azMotorTemp,
        data.azDriverTemp,
        data.azFault
        );

    // Update Elevation Servo
    m_viewModel->updateElevationServo(
        data.elServoConnected,
        data.gimbalEl,
        data.elRpm,
        data.elTorque,
        data.elMotorTemp,
        data.elDriverTemp,
        data.elFault
        );

    // Update IMU
    m_viewModel->updateImu(
        data.imuConnected,
        data.imuRollDeg,
        data.imuPitchDeg,
        data.imuYawDeg,
        data.imuTemp
        );

    // Update LRF
    m_viewModel->updateLrf(
        data.lrfConnected,
        data.lrfDistance,
        data.lrfTemp,
        data.lrfLaserCount,
        data.lrfSystemStatus,
        data.lrfFault,
        data.lrfNoEcho,
        data.lrfLaserNotOut,
        data.lrfOverTemp
        );

    // Update Day Camera
    m_viewModel->updateDayCamera(
        data.dayCameraConnected,
        data.activeCameraIsDay,
        data.dayCurrentHFOV,
        data.dayZoomPosition,
        data.dayFocusPosition,
        data.dayAutofocusEnabled,
        data.dayCameraError,
        data.dayCameraStatus 
        );

    // Update Night Camera
    m_viewModel->updateNightCamera(
        data.nightCameraConnected,
        !data.activeCameraIsDay,
        data.nightCurrentHFOV,
        data.nightDigitalZoomLevel,
        data.nightFfcInProgress,
        data.nightCameraError,
        data.nightCameraStatus,
        data.nightVideoMode,
        data.nightFpaTemperature
        );

    // Update PLC Status
    m_viewModel->updatePlcStatus(
        data.plc21Connected,
        data.plc42Connected,
        data.stationEnabled,
        data.gunArmed
        );

    m_viewModel->updateServoActuator(
        data.actuatorConnected,
        data.actuatorPosition,
        data.actuatorVelocity,
        data.actuatorTemp,
        data.actuatorBusVoltage,
        data.actuatorTorque,
        data.actuatorMotorOff,
        data.actuatorFault
    );
    // Update Alarms
    QStringList alarms = buildAlarmsList(data);
    m_viewModel->updateAlarms(alarms);
}

QStringList SystemStatusController::buildAlarmsList(const SystemStateData& data)
{
    QStringList alarms;

    // Emergency Stop
    if (data.emergencyStopActive) {
        alarms.append("⚠ EMERGENCY STOP ACTIVE");
    }

    // Temperature alarms
    if (data.azDriverTemp > 70.0) {
        alarms.append("⚠ Az Driver Temp High");
    }
    if (data.azMotorTemp > 70.0) {
        alarms.append("⚠ Az Motor Temp High");
    }
    if (data.elDriverTemp > 70.0) {
        alarms.append("⚠ El Driver Temp High");
    }
    if (data.elMotorTemp > 70.0) {
        alarms.append("⚠ El Motor Temp High");
    }

    // Servo faults
    if (data.azFault) {
        alarms.append("⚠ Azimuth Servo Fault");
    }
    if (data.elFault) {
        alarms.append("⚠ Elevation Servo Fault");
    }

    // Connection alarms
    if (!data.azServoConnected) {
        alarms.append("⚠ Azimuth Servo Disconnected");
    }
    if (!data.elServoConnected) {
        alarms.append("⚠ Elevation Servo Disconnected");
    }
    if (!data.imuConnected) {
        alarms.append("⚠ IMU Disconnected");
    }
    if (!data.lrfConnected) {
        alarms.append("⚠ LRF Disconnected");
    }
    if (!data.dayCameraConnected) {
        alarms.append("⚠ Day Camera Disconnected");
    }
    if (!data.nightCameraConnected) {
        alarms.append("⚠ Night Camera Disconnected");
    }
    if (!data.plc21Connected) {
        alarms.append("⚠ PLC21 Disconnected");
    }
    if (!data.plc42Connected) {
        alarms.append("⚠ PLC42 Disconnected");
    }
    if (!data.actuatorConnected) {
        alarms.append("⚠ Servo Actuator Disconnected");
    }

    // LRF faults
    if (data.lrfFault) {
        alarms.append("⚠ LRF Fault Detected");
    }
    if (data.lrfOverTemp) {
        alarms.append("⚠ LRF Over Temperature");
    }

    // Camera errors
    if (data.dayCameraError) {
        alarms.append("⚠ Day Camera Error");
    }
    if (data.nightCameraError) {
        alarms.append("⚠ Night Camera Error");
    }

    // System status
    if (!data.stationEnabled) {
        alarms.append("ℹ Station Disabled");
    }

    // If no alarms, add success message
    if (alarms.isEmpty()) {
        alarms.append("✓ All Systems Nominal");
    }

    return alarms;
}

void SystemStatusController::onClearAlarmsRequested()
{
    qDebug() << "SystemStatusController: Clear alarms requested";
    emit clearAlarmsSignal();
}

void SystemStatusController::onSelectButtonPressed()
{
    hide();
    emit returnToMainMenu();
    emit menuFinished();
}

void SystemStatusController::onBackButtonPressed()
{
    hide();
    emit returnToMainMenu();
    emit menuFinished();
}

void SystemStatusController::onUpButtonPressed()
{
    // Could be used for scrolling through sections if needed
}

void SystemStatusController::onDownButtonPressed()
{
    // Could be used for scrolling through sections if needed
}

void SystemStatusController::onColorStyleChanged(const QColor& color)
{
    if (m_viewModel) {
        m_viewModel->setAccentColor(color);
    }
}
