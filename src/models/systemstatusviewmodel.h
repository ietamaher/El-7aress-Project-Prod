#ifndef SYSTEMSTATUSVIEWMODEL_H
#define SYSTEMSTATUSVIEWMODEL_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QColor>

/**
 * @brief SystemStatusViewModel - Exposes comprehensive device health status to QML
 *
 * Displays real-time status of all hardware devices in a compact 3-column layout:
 * - Motion Systems (Azimuth/Elevation servos, Servo Actuator)
 * - Sensors (IMU, LRF)
 * - Cameras (Day/Night)
 * - Control Systems (PLCs)
 * - Active alarms
 *
 * All status text properties follow the pattern:
 * - "✓ OK" when connected and healthy
 * - "⚠ [ERROR DESCRIPTION]" when connected with errors
 * - "N/A" when disconnected
 */
class SystemStatusViewModel : public QObject
{
    Q_OBJECT

    // ============================================================================
    // AZIMUTH SERVO
    // ============================================================================
    Q_PROPERTY(bool azConnected READ azConnected NOTIFY azConnectedChanged)
    Q_PROPERTY(QString azPositionText READ azPositionText NOTIFY azPositionTextChanged)
    Q_PROPERTY(QString azRpmText READ azRpmText NOTIFY azRpmTextChanged)
    Q_PROPERTY(QString azTorqueText READ azTorqueText NOTIFY azTorqueTextChanged)
    Q_PROPERTY(QString azMotorTempText READ azMotorTempText NOTIFY azMotorTempTextChanged)
    Q_PROPERTY(QString azDriverTempText READ azDriverTempText NOTIFY azDriverTempTextChanged)
    Q_PROPERTY(bool azFault READ azFault NOTIFY azFaultChanged)
    Q_PROPERTY(QString azStatusText READ azStatusText NOTIFY azStatusTextChanged)

    // ============================================================================
    // ELEVATION SERVO
    // ============================================================================
    Q_PROPERTY(bool elConnected READ elConnected NOTIFY elConnectedChanged)
    Q_PROPERTY(QString elPositionText READ elPositionText NOTIFY elPositionTextChanged)
    Q_PROPERTY(QString elRpmText READ elRpmText NOTIFY elRpmTextChanged)
    Q_PROPERTY(QString elTorqueText READ elTorqueText NOTIFY elTorqueTextChanged)
    Q_PROPERTY(QString elMotorTempText READ elMotorTempText NOTIFY elMotorTempTextChanged)
    Q_PROPERTY(QString elDriverTempText READ elDriverTempText NOTIFY elDriverTempTextChanged)
    Q_PROPERTY(bool elFault READ elFault NOTIFY elFaultChanged)
    Q_PROPERTY(QString elStatusText READ elStatusText NOTIFY elStatusTextChanged)

    // ============================================================================
    // IMU
    // ============================================================================
    Q_PROPERTY(bool imuConnected READ imuConnected NOTIFY imuConnectedChanged)
    Q_PROPERTY(QString imuRollText READ imuRollText NOTIFY imuRollTextChanged)
    Q_PROPERTY(QString imuPitchText READ imuPitchText NOTIFY imuPitchTextChanged)
    Q_PROPERTY(QString imuYawText READ imuYawText NOTIFY imuYawTextChanged)
    Q_PROPERTY(QString imuTempText READ imuTempText NOTIFY imuTempTextChanged)
    Q_PROPERTY(QString imuStatusText READ imuStatusText NOTIFY imuStatusTextChanged)

    // ============================================================================
    // LRF
    // ============================================================================
    Q_PROPERTY(bool lrfConnected READ lrfConnected NOTIFY lrfConnectedChanged)
    Q_PROPERTY(QString lrfDistanceText READ lrfDistanceText NOTIFY lrfDistanceTextChanged)
    Q_PROPERTY(QString lrfTempText READ lrfTempText NOTIFY lrfTempTextChanged)
    Q_PROPERTY(QString lrfLaserCountText READ lrfLaserCountText NOTIFY lrfLaserCountTextChanged)
    Q_PROPERTY(QString lrfRawStatusByteText READ lrfRawStatusByteText NOTIFY lrfRawStatusByteTextChanged)
    Q_PROPERTY(bool lrfFault READ lrfFault NOTIFY lrfFaultChanged)
    Q_PROPERTY(QString lrfFaultText READ lrfFaultText NOTIFY lrfFaultTextChanged)

    // ============================================================================
    // DAY CAMERA
    // ============================================================================
    Q_PROPERTY(bool dayCamConnected READ dayCamConnected NOTIFY dayCamConnectedChanged)
    Q_PROPERTY(bool dayCamActive READ dayCamActive NOTIFY dayCamActiveChanged)
    Q_PROPERTY(QString dayCamFovText READ dayCamFovText NOTIFY dayCamFovTextChanged)
    Q_PROPERTY(QString dayCamZoomText READ dayCamZoomText NOTIFY dayCamZoomTextChanged)
    Q_PROPERTY(QString dayCamFocusText READ dayCamFocusText NOTIFY dayCamFocusTextChanged)
    Q_PROPERTY(bool dayCamAutofocus READ dayCamAutofocus NOTIFY dayCamAutofocusChanged)
    Q_PROPERTY(bool dayCamError READ dayCamError NOTIFY dayCamErrorChanged)
    Q_PROPERTY(QString dayCamStatusText READ dayCamStatusText NOTIFY dayCamStatusTextChanged)

    // ============================================================================
    // NIGHT CAMERA
    // ============================================================================
    Q_PROPERTY(bool nightCamConnected READ nightCamConnected NOTIFY nightCamConnectedChanged)
    Q_PROPERTY(bool nightCamActive READ nightCamActive NOTIFY nightCamActiveChanged)
    Q_PROPERTY(QString nightCamFovText READ nightCamFovText NOTIFY nightCamFovTextChanged)
    Q_PROPERTY(QString nightCamZoomText READ nightCamZoomText NOTIFY nightCamZoomTextChanged)
    Q_PROPERTY(QString nightCamTempText READ nightCamTempText NOTIFY nightCamTempTextChanged)
    Q_PROPERTY(bool nightCamFfcInProgress READ nightCamFfcInProgress NOTIFY nightCamFfcInProgressChanged)
    Q_PROPERTY(bool nightCamError READ nightCamError NOTIFY nightCamErrorChanged)
    Q_PROPERTY(QString nightCamVideoModeText READ nightCamVideoModeText NOTIFY nightCamVideoModeTextChanged)
    Q_PROPERTY(QString nightCamStatusText READ nightCamStatusText NOTIFY nightCamStatusTextChanged)

    // ============================================================================
    // PLC STATUS
    // ============================================================================
    Q_PROPERTY(bool plc21Connected READ plc21Connected NOTIFY plc21ConnectedChanged)
    Q_PROPERTY(bool plc42Connected READ plc42Connected NOTIFY plc42ConnectedChanged)
    Q_PROPERTY(bool stationEnabled READ stationEnabled NOTIFY stationEnabledChanged)
    Q_PROPERTY(bool gunArmed READ gunArmed NOTIFY gunArmedChanged)
    Q_PROPERTY(QString plc21StatusText READ plc21StatusText NOTIFY plc21StatusTextChanged)
    Q_PROPERTY(QString plc42StatusText READ plc42StatusText NOTIFY plc42StatusTextChanged)

    // ============================================================================
    // SERVO ACTUATOR
    // ============================================================================
    Q_PROPERTY(bool actuatorConnected READ actuatorConnected NOTIFY actuatorConnectedChanged)
    Q_PROPERTY(QString actuatorPositionText READ actuatorPositionText NOTIFY actuatorPositionTextChanged)
    Q_PROPERTY(QString actuatorVelocityText READ actuatorVelocityText NOTIFY actuatorVelocityTextChanged)
    Q_PROPERTY(QString actuatorTempText READ actuatorTempText NOTIFY actuatorTempTextChanged)
    Q_PROPERTY(QString actuatorVoltageText READ actuatorVoltageText NOTIFY actuatorVoltageTextChanged)
    Q_PROPERTY(QString actuatorTorqueText READ actuatorTorqueText NOTIFY actuatorTorqueTextChanged)
    Q_PROPERTY(bool actuatorMotorOff READ actuatorMotorOff NOTIFY actuatorMotorOffChanged)
    Q_PROPERTY(bool actuatorFault READ actuatorFault NOTIFY actuatorFaultChanged)
    Q_PROPERTY(QString actuatorStatusText READ actuatorStatusText NOTIFY actuatorStatusTextChanged)

    // ============================================================================
    // ALARMS
    // ============================================================================
    Q_PROPERTY(QStringList alarmsList READ alarmsList NOTIFY alarmsListChanged)
    Q_PROPERTY(bool hasAlarms READ hasAlarms NOTIFY hasAlarmsChanged)

    // ============================================================================
    // VISIBILITY & STYLE
    // ============================================================================
    Q_PROPERTY(bool visible READ visible WRITE setVisible NOTIFY visibleChanged)
    Q_PROPERTY(QColor accentColor READ accentColor NOTIFY accentColorChanged)

public:
    explicit SystemStatusViewModel(QObject *parent = nullptr);

    // ============================================================================
    // GETTERS - AZIMUTH SERVO
    // ============================================================================
    bool azConnected() const { return m_azConnected; }
    QString azPositionText() const { return m_azPositionText; }
    QString azRpmText() const { return m_azRpmText; }
    QString azTorqueText() const { return m_azTorqueText; }
    QString azMotorTempText() const { return m_azMotorTempText; }
    QString azDriverTempText() const { return m_azDriverTempText; }
    bool azFault() const { return m_azFault; }
    QString azStatusText() const { return m_azStatusText; }

    // ============================================================================
    // GETTERS - ELEVATION SERVO
    // ============================================================================
    bool elConnected() const { return m_elConnected; }
    QString elPositionText() const { return m_elPositionText; }
    QString elRpmText() const { return m_elRpmText; }
    QString elTorqueText() const { return m_elTorqueText; }
    QString elMotorTempText() const { return m_elMotorTempText; }
    QString elDriverTempText() const { return m_elDriverTempText; }
    bool elFault() const { return m_elFault; }
    QString elStatusText() const { return m_elStatusText; }

    // ============================================================================
    // GETTERS - IMU
    // ============================================================================
    bool imuConnected() const { return m_imuConnected; }
    QString imuRollText() const { return m_imuRollText; }
    QString imuPitchText() const { return m_imuPitchText; }
    QString imuYawText() const { return m_imuYawText; }
    QString imuTempText() const { return m_imuTempText; }
    QString imuStatusText() const { return m_imuStatusText; }

    // ============================================================================
    // GETTERS - LRF
    // ============================================================================
    bool lrfConnected() const { return m_lrfConnected; }
    QString lrfDistanceText() const { return m_lrfDistanceText; }
    QString lrfTempText() const { return m_lrfTempText; }
    QString lrfLaserCountText() const { return m_lrfLaserCountText; }
    QString lrfRawStatusByteText() const { return m_lrfRawStatusByteText; }
    bool lrfFault() const { return m_lrfFault; }
    QString lrfFaultText() const { return m_lrfFaultText; }

    // ============================================================================
    // GETTERS - DAY CAMERA
    // ============================================================================
    bool dayCamConnected() const { return m_dayCamConnected; }
    bool dayCamActive() const { return m_dayCamActive; }
    QString dayCamFovText() const { return m_dayCamFovText; }
    QString dayCamZoomText() const { return m_dayCamZoomText; }
    QString dayCamFocusText() const { return m_dayCamFocusText; }
    bool dayCamAutofocus() const { return m_dayCamAutofocus; }
    bool dayCamError() const { return m_dayCamError; }
    QString dayCamStatusText() const { return m_dayCamStatusText; }

    // ============================================================================
    // GETTERS - NIGHT CAMERA
    // ============================================================================
    bool nightCamConnected() const { return m_nightCamConnected; }
    bool nightCamActive() const { return m_nightCamActive; }
    QString nightCamFovText() const { return m_nightCamFovText; }
    QString nightCamZoomText() const { return m_nightCamZoomText; }
    QString nightCamTempText() const { return m_nightCamTempText; }
    bool nightCamFfcInProgress() const { return m_nightCamFfcInProgress; }
    bool nightCamError() const { return m_nightCamError; }
    QString nightCamVideoModeText() const { return m_nightCamVideoModeText; }
    QString nightCamStatusText() const { return m_nightCamStatusText; }

    // ============================================================================
    // GETTERS - PLC
    // ============================================================================
    bool plc21Connected() const { return m_plc21Connected; }
    bool plc42Connected() const { return m_plc42Connected; }
    bool stationEnabled() const { return m_stationEnabled; }
    bool gunArmed() const { return m_gunArmed; }
    QString plc21StatusText() const { return m_plc21StatusText; }
    QString plc42StatusText() const { return m_plc42StatusText; }

    // ============================================================================
    // GETTERS - SERVO ACTUATOR
    // ============================================================================
    bool actuatorConnected() const { return m_actuatorConnected; }
    QString actuatorPositionText() const { return m_actuatorPositionText; }
    QString actuatorVelocityText() const { return m_actuatorVelocityText; }
    QString actuatorTempText() const { return m_actuatorTempText; }
    QString actuatorVoltageText() const { return m_actuatorVoltageText; }
    QString actuatorTorqueText() const { return m_actuatorTorqueText; }
    bool actuatorMotorOff() const { return m_actuatorMotorOff; }
    bool actuatorFault() const { return m_actuatorFault; }
    QString actuatorStatusText() const { return m_actuatorStatusText; }

    // ============================================================================
    // GETTERS - ALARMS
    // ============================================================================
    QStringList alarmsList() const { return m_alarmsList; }
    bool hasAlarms() const { return m_hasAlarms; }

    // ============================================================================
    // GETTERS - VISIBILITY
    // ============================================================================
    bool visible() const { return m_visible; }
    QColor accentColor() const { return m_accentColor; }

    void setVisible(bool visible);
    void setAccentColor(const QColor& color);

public slots:
    // ============================================================================
    // UPDATE METHODS (called by controller)
    // ============================================================================
    void updateAzimuthServo(bool connected, float position, float rpm, float torque,
                            float motorTemp, float driverTemp, bool fault);

    void updateElevationServo(bool connected, float position, float rpm, float torque,
                              float motorTemp, float driverTemp, bool fault);

    void updateImu(bool connected, double roll, double pitch, double yaw, double temp);

    void updateLrf(bool connected, float distance, float temp, quint32 laserCount,
                   quint8 rawStatusByte, bool fault, bool noEcho, bool laserNotOut, bool overTemp);

    void updateDayCamera(bool connected, bool isActive, float fov, quint16 zoom,
                         quint16 focus, bool autofocus, bool error, quint8 errorCode);

    void updateNightCamera(bool connected, bool isActive, float fov, quint8 digitalZoom,
                           bool ffcInProgress, bool error, quint8 errorCode, quint16 videoMode, qint16 fpaTemp);

    void updatePlcStatus(bool plc21Conn, bool plc42Conn, bool stationEn, bool gunArm);

    void updateServoActuator(bool connected, double position, double velocity,
                             double temp, double voltage, double torque,
                             bool motorOff, bool fault);

    void updateAlarms(const QStringList& alarms);

signals:
    // ============================================================================
    // SIGNALS - AZIMUTH SERVO
    // ============================================================================
    void azConnectedChanged();
    void azPositionTextChanged();
    void azRpmTextChanged();
    void azTorqueTextChanged();
    void azMotorTempTextChanged();
    void azDriverTempTextChanged();
    void azFaultChanged();
    void azStatusTextChanged();

    // ============================================================================
    // SIGNALS - ELEVATION SERVO
    // ============================================================================
    void elConnectedChanged();
    void elPositionTextChanged();
    void elRpmTextChanged();
    void elTorqueTextChanged();
    void elMotorTempTextChanged();
    void elDriverTempTextChanged();
    void elFaultChanged();
    void elStatusTextChanged();

    // ============================================================================
    // SIGNALS - IMU
    // ============================================================================
    void imuConnectedChanged();
    void imuRollTextChanged();
    void imuPitchTextChanged();
    void imuYawTextChanged();
    void imuTempTextChanged();
    void imuStatusTextChanged();

    // ============================================================================
    // SIGNALS - LRF
    // ============================================================================
    void lrfConnectedChanged();
    void lrfDistanceTextChanged();
    void lrfTempTextChanged();
    void lrfLaserCountTextChanged();
    void lrfRawStatusByteTextChanged();
    void lrfFaultChanged();
    void lrfFaultTextChanged();

    // ============================================================================
    // SIGNALS - DAY CAMERA
    // ============================================================================
    void dayCamConnectedChanged();
    void dayCamActiveChanged();
    void dayCamFovTextChanged();
    void dayCamZoomTextChanged();
    void dayCamFocusTextChanged();
    void dayCamAutofocusChanged();
    void dayCamErrorChanged();
    void dayCamStatusTextChanged();

    // ============================================================================
    // SIGNALS - NIGHT CAMERA
    // ============================================================================
    void nightCamConnectedChanged();
    void nightCamActiveChanged();
    void nightCamFovTextChanged();
    void nightCamZoomTextChanged();
    void nightCamTempTextChanged();
    void nightCamFfcInProgressChanged();
    void nightCamErrorChanged();
    void nightCamVideoModeTextChanged();
    void nightCamStatusTextChanged();

    // ============================================================================
    // SIGNALS - PLC
    // ============================================================================
    void plc21ConnectedChanged();
    void plc42ConnectedChanged();
    void stationEnabledChanged();
    void gunArmedChanged();
    void plc21StatusTextChanged();
    void plc42StatusTextChanged();

    // ============================================================================
    // SIGNALS - SERVO ACTUATOR
    // ============================================================================
    void actuatorConnectedChanged();
    void actuatorPositionTextChanged();
    void actuatorVelocityTextChanged();
    void actuatorTempTextChanged();
    void actuatorVoltageTextChanged();
    void actuatorTorqueTextChanged();
    void actuatorMotorOffChanged();
    void actuatorFaultChanged();
    void actuatorStatusTextChanged();

    // ============================================================================
    // SIGNALS - ALARMS
    // ============================================================================
    void alarmsListChanged();
    void hasAlarmsChanged();

    // ============================================================================
    // SIGNALS - VISIBILITY
    // ============================================================================
    void visibleChanged();
    void accentColorChanged();

    // ============================================================================
    // ACTION SIGNALS
    // ============================================================================
    void clearAlarmsRequested();

private:
    // ============================================================================
    // HELPER FUNCTIONS
    // ============================================================================
    QString getDayCameraErrorDescription(quint8 errorCode) const;
    QString getNightCameraErrorDescription(quint8 errorCode) const;

    // ============================================================================
    // PRIVATE MEMBERS - AZIMUTH SERVO
    // ============================================================================
    bool m_azConnected;
    QString m_azPositionText;
    QString m_azRpmText;
    QString m_azTorqueText;
    QString m_azMotorTempText;
    QString m_azDriverTempText;
    bool m_azFault;
    QString m_azStatusText;

    // ============================================================================
    // PRIVATE MEMBERS - ELEVATION SERVO
    // ============================================================================
    bool m_elConnected;
    QString m_elPositionText;
    QString m_elRpmText;
    QString m_elTorqueText;
    QString m_elMotorTempText;
    QString m_elDriverTempText;
    bool m_elFault;
    QString m_elStatusText;

    // ============================================================================
    // PRIVATE MEMBERS - IMU
    // ============================================================================
    bool m_imuConnected;
    QString m_imuRollText;
    QString m_imuPitchText;
    QString m_imuYawText;
    QString m_imuTempText;
    QString m_imuStatusText;

    // ============================================================================
    // PRIVATE MEMBERS - LRF
    // ============================================================================
    bool m_lrfConnected;
    QString m_lrfDistanceText;
    QString m_lrfTempText;
    QString m_lrfLaserCountText;
    QString m_lrfRawStatusByteText;
    bool m_lrfFault;
    QString m_lrfFaultText;

    // ============================================================================
    // PRIVATE MEMBERS - DAY CAMERA
    // ============================================================================
    bool m_dayCamConnected;
    bool m_dayCamActive;
    QString m_dayCamFovText;
    QString m_dayCamZoomText;
    QString m_dayCamFocusText;
    bool m_dayCamAutofocus;
    bool m_dayCamError;
    QString m_dayCamStatusText;

    // ============================================================================
    // PRIVATE MEMBERS - NIGHT CAMERA
    // ============================================================================
    bool m_nightCamConnected;
    bool m_nightCamActive;
    QString m_nightCamFovText;
    QString m_nightCamZoomText;
    QString m_nightCamTempText;
    bool m_nightCamFfcInProgress;
    bool m_nightCamError;  // ✅ FIXED: Changed from quint8 to bool
    QString m_nightCamVideoModeText;
    QString m_nightCamStatusText;

    // ============================================================================
    // PRIVATE MEMBERS - PLC
    // ============================================================================
    bool m_plc21Connected;
    bool m_plc42Connected;
    bool m_stationEnabled;
    bool m_gunArmed;
    QString m_plc21StatusText;
    QString m_plc42StatusText;

    // ============================================================================
    // PRIVATE MEMBERS - SERVO ACTUATOR
    // ============================================================================
    bool m_actuatorConnected;
    QString m_actuatorPositionText;
    QString m_actuatorVelocityText;
    QString m_actuatorTempText;
    QString m_actuatorVoltageText;
    QString m_actuatorTorqueText;
    bool m_actuatorMotorOff;
    bool m_actuatorFault;
    QString m_actuatorStatusText;

    // ============================================================================
    // PRIVATE MEMBERS - ALARMS
    // ============================================================================
    QStringList m_alarmsList;
    bool m_hasAlarms;

    // ============================================================================
    // PRIVATE MEMBERS - VISIBILITY
    // ============================================================================
    bool m_visible;
    QColor m_accentColor;
};

#endif // SYSTEMSTATUSVIEWMODEL_H