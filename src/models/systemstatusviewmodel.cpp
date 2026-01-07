#include "systemstatusviewmodel.h"
#include <QDebug>

SystemStatusViewModel::SystemStatusViewModel(QObject* parent)
    : QObject(parent), m_azConnected(false), m_azPositionText("N/A"), m_azRpmText("N/A"),
      m_azTorqueText("N/A"), m_azMotorTempText("N/A"), m_azDriverTempText("N/A"), m_azFault(false),
      m_elConnected(false), m_elPositionText("N/A"), m_elRpmText("N/A"), m_elTorqueText("N/A"),
      m_elMotorTempText("N/A"), m_elDriverTempText("N/A"), m_elFault(false), m_imuConnected(false),
      m_imuRollText("N/A"), m_imuPitchText("N/A"), m_imuYawText("N/A"), m_imuTempText("N/A"),
      m_lrfConnected(false), m_lrfDistanceText("N/A"), m_lrfTempText("N/A"),
      m_lrfLaserCountText("N/A"), m_lrfRawStatusByteText("N/A"), m_lrfFault(false),
      m_lrfFaultText("No Faults"), m_dayCamConnected(false), m_dayCamActive(false),
      m_dayCamFovText("N/A"), m_dayCamZoomText("N/A"), m_dayCamFocusText("N/A"),
      m_dayCamAutofocus(false), m_dayCamError(false), m_nightCamConnected(false),
      m_nightCamActive(false), m_nightCamFovText("N/A"), m_nightCamZoomText("N/A"),
      m_nightCamTempText("N/A"), m_nightCamFfcInProgress(false), m_nightCamError(false),
      m_plc21Connected(false), m_plc42Connected(false), m_stationEnabled(false), m_gunArmed(false),
      m_actuatorConnected(false), m_actuatorPositionText("N/A"), m_actuatorVelocityText("N/A"),
      m_actuatorTempText("N/A"), m_actuatorVoltageText("N/A"), m_actuatorTorqueText("N/A"),
      m_actuatorMotorOff(false), m_actuatorFault(false), m_actuatorStatusText("N/A"),
      m_hasAlarms(false), m_visible(false), m_accentColor(QColor(70, 226, 165)) {}

void SystemStatusViewModel::setVisible(bool visible) {
    if (m_visible != visible) {
        m_visible = visible;
        emit visibleChanged();
    }
}

void SystemStatusViewModel::setAccentColor(const QColor& color) {
    if (m_accentColor != color) {
        m_accentColor = color;
        emit accentColorChanged();
    }
}

// ============================================================================
// AZIMUTH SERVO
// ============================================================================
void SystemStatusViewModel::updateAzimuthServo(bool connected, float position, float rpm,
                                               float torque, float motorTemp, float driverTemp,
                                               bool fault) {
    if (m_azConnected != connected) {
        m_azConnected = connected;
        emit azConnectedChanged();
    }

    QString newPos = QString::number(position, 'f', 2) + "°";
    if (m_azPositionText != newPos) {
        m_azPositionText = newPos;
        emit azPositionTextChanged();
    }

    QString newRpm = QString::number(rpm, 'f', 0);
    if (m_azRpmText != newRpm) {
        m_azRpmText = newRpm;
        emit azRpmTextChanged();
    }

    QString newTorque = QString::number(torque, 'f', 1) + "%";
    if (m_azTorqueText != newTorque) {
        m_azTorqueText = newTorque;
        emit azTorqueTextChanged();
    }

    QString newMotorTemp = QString::number(motorTemp, 'f', 1) + "°C";
    if (m_azMotorTempText != newMotorTemp) {
        m_azMotorTempText = newMotorTemp;
        emit azMotorTempTextChanged();
    }

    QString newDriverTemp = QString::number(driverTemp, 'f', 1) + "°C";
    if (m_azDriverTempText != newDriverTemp) {
        m_azDriverTempText = newDriverTemp;
        emit azDriverTempTextChanged();
    }

    if (m_azFault != fault) {
        m_azFault = fault;
        emit azFaultChanged();
    }

    QString statusText = connected ? (fault ? "⚠ FAULT" : "✓ OK") : "N/A";

    if (m_azStatusText != statusText) {
        m_azStatusText = statusText;
        emit azStatusTextChanged();
    }
}

// ============================================================================
// ELEVATION SERVO
// ============================================================================
void SystemStatusViewModel::updateElevationServo(bool connected, float position, float rpm,
                                                 float torque, float motorTemp, float driverTemp,
                                                 bool fault) {
    if (m_elConnected != connected) {
        m_elConnected = connected;
        emit elConnectedChanged();
    }

    QString newPos = QString::number(position, 'f', 2) + "°";
    if (m_elPositionText != newPos) {
        m_elPositionText = newPos;
        emit elPositionTextChanged();
    }

    QString newRpm = QString::number(rpm, 'f', 0);
    if (m_elRpmText != newRpm) {
        m_elRpmText = newRpm;
        emit elRpmTextChanged();
    }

    QString newTorque = QString::number(torque, 'f', 1) + "%";
    if (m_elTorqueText != newTorque) {
        m_elTorqueText = newTorque;
        emit elTorqueTextChanged();
    }

    QString newMotorTemp = QString::number(motorTemp, 'f', 1) + "°C";
    if (m_elMotorTempText != newMotorTemp) {
        m_elMotorTempText = newMotorTemp;
        emit elMotorTempTextChanged();
    }

    QString newDriverTemp = QString::number(driverTemp, 'f', 1) + "°C";
    if (m_elDriverTempText != newDriverTemp) {
        m_elDriverTempText = newDriverTemp;
        emit elDriverTempTextChanged();
    }

    if (m_elFault != fault) {
        m_elFault = fault;
        emit elFaultChanged();
    }

    QString statusText = connected ? (fault ? "⚠ FAULT" : "✓ OK") : "N/A";

    if (m_azStatusText != statusText) {
        m_azStatusText = statusText;
        emit azStatusTextChanged();
    }
}

// ============================================================================
// IMU
// ============================================================================
void SystemStatusViewModel::updateImu(bool connected, double roll, double pitch, double yaw,
                                      double temp) {
    if (m_imuConnected != connected) {
        m_imuConnected = connected;
        emit imuConnectedChanged();
    }

    QString newRoll = QString::number(roll, 'f', 2) + "°";
    if (m_imuRollText != newRoll) {
        m_imuRollText = newRoll;
        emit imuRollTextChanged();
    }

    QString newPitch = QString::number(pitch, 'f', 2) + "°";
    if (m_imuPitchText != newPitch) {
        m_imuPitchText = newPitch;
        emit imuPitchTextChanged();
    }

    QString newYaw = QString::number(yaw, 'f', 2) + "°";
    if (m_imuYawText != newYaw) {
        m_imuYawText = newYaw;
        emit imuYawTextChanged();
    }

    QString newTemp = QString::number(temp, 'f', 1) + "°C";
    if (m_imuTempText != newTemp) {
        m_imuTempText = newTemp;
        emit imuTempTextChanged();
    }

    QString statusText = connected ? "✓ OK" : "N/A";

    if (m_imuStatusText != statusText) {
        m_imuStatusText = statusText;
        emit imuStatusTextChanged();
    }
}

// ============================================================================
// LRF
// ============================================================================
void SystemStatusViewModel::updateLrf(bool connected, float distance, float temp,
                                      quint32 laserCount, quint8 rawStatusByte, bool fault,
                                      bool noEcho, bool laserNotOut, bool overTemp) {
    if (m_lrfConnected != connected) {
        m_lrfConnected = connected;
        emit lrfConnectedChanged();
    }

    QString newDist = QString::number(distance, 'f', 1) + "m";
    if (m_lrfDistanceText != newDist) {
        m_lrfDistanceText = newDist;
        emit lrfDistanceTextChanged();
    }

    QString newTemp = QString::number(temp, 'f', 1) + "°C";
    if (m_lrfTempText != newTemp) {
        m_lrfTempText = newTemp;
        emit lrfTempTextChanged();
    }

    QString newCount = QString::number(laserCount);
    if (m_lrfLaserCountText != newCount) {
        m_lrfLaserCountText = newCount;
        emit lrfLaserCountTextChanged();
    }

    QString newRawStatusByte = QString::number(rawStatusByte);
    if (m_lrfRawStatusByteText != newRawStatusByte) {
        m_lrfRawStatusByteText = newRawStatusByte;
        emit lrfRawStatusByteTextChanged();
    }

    if (m_lrfFault != fault) {
        m_lrfFault = fault;
        emit lrfFaultChanged();
    }


    QString newFaultText;
    if (connected) {
        if (!fault && !noEcho && !laserNotOut && !overTemp) {
            newFaultText = "✓ OK";
        } else {
            QStringList faults;
            if (fault)
                faults.append("General Fault");
            if (noEcho)
                faults.append("No Echo");
            if (laserNotOut)
                faults.append("Laser Not Out");
            if (overTemp)
                faults.append("Over Temp");
            newFaultText = "⚠ " + faults.join(", ");
        }
    } else {
        newFaultText = "N/A";
    }

    if (m_lrfFaultText != newFaultText) {
        m_lrfFaultText = newFaultText;
        emit lrfFaultTextChanged();
    }
}

// ============================================================================
// DAY CAMERA
// ============================================================================
void SystemStatusViewModel::updateDayCamera(bool connected, bool isActive, float fov, quint16 zoom,
                                            quint16 focus, bool autofocus, bool error,
                                            quint8 errorCode) {
    if (m_dayCamConnected != connected) {
        m_dayCamConnected = connected;
        emit dayCamConnectedChanged();
    }

    if (m_dayCamActive != isActive) {
        m_dayCamActive = isActive;
        emit dayCamActiveChanged();
    }

    QString newFov = QString::number(fov, 'f', 1) + "°";
    if (m_dayCamFovText != newFov) {
        m_dayCamFovText = newFov;
        emit dayCamFovTextChanged();
    }

    // Convert raw zoom position (0-16384) to zoom multiplier (1x-30x)
    // Camera has 30X optical zoom: 0 = 1x (wide), 16384 = 30x (tele)
    const double MAX_ZOOM = 16384.0;
    const double ZOOM_RANGE = 29.0;  // 30x - 1x = 29x range
    double zoomMultiplier = 1.0 + (zoom / MAX_ZOOM) * ZOOM_RANGE;
    QString newZoom = QString::number(zoomMultiplier, 'f', 1) + "x";
    if (m_dayCamZoomText != newZoom) {
        m_dayCamZoomText = newZoom;
        emit dayCamZoomTextChanged();
    }

    QString newFocus = QString::number(focus);
    if (m_dayCamFocusText != newFocus) {
        m_dayCamFocusText = newFocus;
        emit dayCamFocusTextChanged();
    }

    if (m_dayCamAutofocus != autofocus) {
        m_dayCamAutofocus = autofocus;
        emit dayCamAutofocusChanged();
    }

    if (m_dayCamError != error) {
        m_dayCamError = error;
        emit dayCamErrorChanged();
    }
    QString newStatusText =
        connected ? (error ? getDayCameraErrorDescription(errorCode) : "✓ OK") : "N/A";
    if (m_dayCamStatusText != newStatusText) {
        m_dayCamStatusText = newStatusText;
        emit dayCamStatusTextChanged();
    }
}

// ============================================================================
// NIGHT CAMERA
// ============================================================================
void SystemStatusViewModel::updateNightCamera(bool connected, bool isActive, float fov,
                                              quint8 digitalZoom, bool ffcInProgress, bool error,
                                              quint8 errorCode, quint16 videoMode, qint16 fpaTemp) {
    if (m_nightCamConnected != connected) {
        m_nightCamConnected = connected;
        emit nightCamConnectedChanged();
    }

    if (m_nightCamActive != isActive) {
        m_nightCamActive = isActive;
        emit nightCamActiveChanged();
    }

    QString newFov = QString::number(fov, 'f', 1) + "°";
    if (m_nightCamFovText != newFov) {
        m_nightCamFovText = newFov;
        emit nightCamFovTextChanged();
    }

    QString newZoom = QString::number(digitalZoom) + "x";
    if (m_nightCamZoomText != newZoom) {
        m_nightCamZoomText = newZoom;
        emit nightCamZoomTextChanged();
    }

    // Update temperature (fpaTemp is in Celsius × 10, e.g., 325 = 32.5°C)
    QString newTemp = connected ? QString::number(fpaTemp / 10.0, 'f', 1) + "°C" : "N/A";
    if (m_nightCamTempText != newTemp) {
        m_nightCamTempText = newTemp;
        emit nightCamTempTextChanged();
    }

    QString newVideoMode = QString("LUT %1").arg(videoMode);
    if (m_nightCamVideoModeText != newVideoMode) {
        m_nightCamVideoModeText = newVideoMode;
        emit nightCamVideoModeTextChanged();
    }

    if (m_nightCamFfcInProgress != ffcInProgress) {
        m_nightCamFfcInProgress = ffcInProgress;
        emit nightCamFfcInProgressChanged();
    }

    if (m_nightCamError != error) {
        m_nightCamError = error;
        emit nightCamErrorChanged();
    }

    QString newStatusText =
        connected ? (error ? getNightCameraErrorDescription(errorCode) : "✓ OK") : "N/A";
    if (m_nightCamStatusText != newStatusText) {
        m_nightCamStatusText = newStatusText;
        emit nightCamStatusTextChanged();
    }
}

// ============================================================================
// PLC
// ============================================================================
void SystemStatusViewModel::updatePlcStatus(bool plc21Conn, bool plc42Conn, bool stationEn,
                                            bool gunArm) {
    if (m_plc21Connected != plc21Conn) {
        m_plc21Connected = plc21Conn;
        emit plc21ConnectedChanged();
    }

    if (m_plc42Connected != plc42Conn) {
        m_plc42Connected = plc42Conn;
        emit plc42ConnectedChanged();
    }

    if (m_stationEnabled != stationEn) {
        m_stationEnabled = stationEn;
        emit stationEnabledChanged();
    }

    if (m_gunArmed != gunArm) {
        m_gunArmed = gunArm;
        emit gunArmedChanged();
    }
    QString plc21Status = plc21Conn ? "✓ OK" : "N/A";
    QString plc42Status = plc42Conn ? "✓ OK" : "N/A";

    if (m_plc21StatusText != plc21Status) {
        m_plc21StatusText = plc21Status;
        emit plc21StatusTextChanged();
    }

    if (m_plc42StatusText != plc42Status) {
        m_plc42StatusText = plc42Status;
        emit plc42StatusTextChanged();
    }
}

// ============================================================================
// Servo Actuator
// ============================================================================
void SystemStatusViewModel::updateServoActuator(bool connected, double position, double velocity,
                                                double temp, double voltage, double torque,
                                                bool motorOff, bool fault) {
    if (m_actuatorConnected != connected) {
        m_actuatorConnected = connected;
        emit actuatorConnectedChanged();
    }

    QString newPos = QString::number(position, 'f', 2) + "mm";
    if (m_actuatorPositionText != newPos) {
        m_actuatorPositionText = newPos;
        emit actuatorPositionTextChanged();
    }

    QString newVel = QString::number(velocity, 'f', 1) + "mm/s";
    if (m_actuatorVelocityText != newVel) {
        m_actuatorVelocityText = newVel;
        emit actuatorVelocityTextChanged();
    }

    QString newTemp = QString::number(temp, 'f', 1) + "°C";
    if (m_actuatorTempText != newTemp) {
        m_actuatorTempText = newTemp;
        emit actuatorTempTextChanged();
    }

    QString newVoltage = QString::number(voltage, 'f', 2) + "V";
    if (m_actuatorVoltageText != newVoltage) {
        m_actuatorVoltageText = newVoltage;
        emit actuatorVoltageTextChanged();
    }

    QString newTorque = QString::number(torque, 'f', 1) + "%";
    if (m_actuatorTorqueText != newTorque) {
        m_actuatorTorqueText = newTorque;
        emit actuatorTorqueTextChanged();
    }

    if (m_actuatorMotorOff != motorOff) {
        m_actuatorMotorOff = motorOff;
        emit actuatorMotorOffChanged();
    }

    if (m_actuatorFault != fault) {
        m_actuatorFault = fault;
        emit actuatorFaultChanged();
    }

    QString statusText;
    if (connected) {
        if (motorOff)
            statusText = "⚠ MOTOR OFF";
        else if (fault)
            statusText = "⚠ FAULT";
        else
            statusText = "✓ OK";
    } else {
        statusText = "N/A";
    }

    if (m_actuatorStatusText != statusText) {
        m_actuatorStatusText = statusText;
        emit actuatorStatusTextChanged();
    }
}

// ============================================================================
// ALARMS
// ============================================================================
void SystemStatusViewModel::updateAlarms(const QStringList& alarms) {
    if (m_alarmsList != alarms) {
        m_alarmsList = alarms;
        emit alarmsListChanged();

        bool newHasAlarms = !alarms.isEmpty();
        if (m_hasAlarms != newHasAlarms) {
            m_hasAlarms = newHasAlarms;
            emit hasAlarmsChanged();
        }
    }
}


QString SystemStatusViewModel::getNightCameraErrorDescription(quint8 errorCode) const {
    switch (errorCode) {
    case 0x01:
        return "⚠ Camera Busy";
    case 0x02:
        return "⚠ Not Ready";
    case 0x03:
        return "⚠ Data Out of Range";
    case 0x04:
        return "⚠ Checksum Error";
    case 0x05:
        return "⚠ Undefined Process";
    case 0x06:
        return "⚠ Undefined Function";
    case 0x07:
        return "⚠ Timeout";
    case 0x09:
        return "⚠ Byte Count Mismatch";
    case 0x0A:
        return "⚠ Feature Not Enabled";
    default:
        return QString("⚠ Error 0x%1").arg(errorCode, 2, 16, QChar('0')).toUpper();
    }
}

QString SystemStatusViewModel::getDayCameraErrorDescription(quint8 errorCode) const {
    // Add VISCA-specific error codes
    switch (errorCode) {
    case 0x01:
        return "⚠ Message Length Error";
    case 0x02:
        return "⚠ Syntax Error";
    case 0x03:
        return "⚠ Command Buffer Full";
    case 0x04:
        return "⚠ Command Canceled";
    case 0x05:
        return "⚠ No Socket";
    case 0x41:
        return "⚠ Command Not Executable";
    default:
        return QString("⚠ Error 0x%1").arg(errorCode, 2, 16, QChar('0')).toUpper();
    }
}
