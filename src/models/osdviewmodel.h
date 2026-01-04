#ifndef OSDVIEWMODEL_H
#define OSDVIEWMODEL_H

#include <QObject>
#include <QColor>
#include <QRectF>
#include <QString>
#include <QVariantList>
#include "models/domain/systemstatedata.h" // For enums
#include "utils/inference.h" // For YoloDetection

class OsdViewModel : public QObject
{
    Q_OBJECT

    // ========================================================================
    // CORE DISPLAY PROPERTIES
    // ========================================================================
    Q_PROPERTY(QColor accentColor READ accentColor NOTIFY accentColorChanged)
    Q_PROPERTY(QString modeText READ modeText NOTIFY modeTextChanged)
    Q_PROPERTY(QString motionText READ motionText NOTIFY motionTextChanged)
    Q_PROPERTY(QString stabText READ stabText NOTIFY stabTextChanged)
    Q_PROPERTY(QString cameraText READ cameraText NOTIFY cameraTextChanged)
    Q_PROPERTY(QString speedText READ speedText NOTIFY speedTextChanged)

    // ========================================================================
    // GIMBAL POSITION
    // ========================================================================
    Q_PROPERTY(float azimuth READ azimuth NOTIFY azimuthChanged)
    Q_PROPERTY(float elevation READ elevation NOTIFY elevationChanged)

    Q_PROPERTY(bool imuConnected READ imuConnected NOTIFY imuConnectedChanged)
    Q_PROPERTY(double vehicleHeading READ vehicleHeading NOTIFY vehicleHeadingChanged)
    Q_PROPERTY(double vehicleRoll READ vehicleRoll NOTIFY vehicleRollChanged)
    Q_PROPERTY(double vehiclePitch READ vehiclePitch NOTIFY vehiclePitchChanged)
    Q_PROPERTY(double imuTemperature READ imuTemperature NOTIFY imuTemperatureChanged)
    // ========================================================================
    // SYSTEM STATUS
    // ========================================================================
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(QString rateText READ rateText NOTIFY rateTextChanged)
    Q_PROPERTY(QString lrfText READ lrfText NOTIFY lrfTextChanged)
    Q_PROPERTY(QString fovText READ fovText NOTIFY fovTextChanged)

    // ⭐ Homing Operation Status
    Q_PROPERTY(QString homingText READ homingText NOTIFY homingTextChanged)
    Q_PROPERTY(bool homingVisible READ homingVisible NOTIFY homingVisibleChanged)

    // ========================================================================
    // TRACKING
    // ========================================================================
    Q_PROPERTY(QRectF trackingBox READ trackingBox NOTIFY trackingBoxChanged)
    Q_PROPERTY(bool trackingBoxVisible READ trackingBoxVisible NOTIFY trackingBoxVisibleChanged)
    Q_PROPERTY(QColor trackingBoxColor READ trackingBoxColor NOTIFY trackingBoxColorChanged)
    Q_PROPERTY(bool trackingBoxDashed READ trackingBoxDashed NOTIFY trackingBoxDashedChanged)
    Q_PROPERTY(bool isTrackingActive READ isTrackingActive NOTIFY isTrackingActiveChanged)
    Q_PROPERTY(float trackingConfidence READ trackingConfidence NOTIFY trackingConfidenceChanged)
    Q_PROPERTY(bool trackingActive READ trackingActive NOTIFY trackingActiveChanged)

    // Acquisition box (for Tracking_Acquisition phase)
    Q_PROPERTY(QRectF acquisitionBox READ acquisitionBox NOTIFY acquisitionBoxChanged)
    Q_PROPERTY(bool acquisitionBoxVisible READ acquisitionBoxVisible NOTIFY acquisitionBoxVisibleChanged)

    // ========================================================================
    // RETICLE (Main aiming reticle - with zeroing only)
    // ========================================================================
    Q_PROPERTY(int reticleType READ reticleType NOTIFY reticleTypeChanged)
    Q_PROPERTY(float reticleOffsetX READ reticleOffsetX NOTIFY reticleOffsetChanged)
    Q_PROPERTY(float reticleOffsetY READ reticleOffsetY NOTIFY reticleOffsetChanged)
    Q_PROPERTY(float currentFov READ currentFov NOTIFY currentFovChanged)

    // ========================================================================
    // CCIP PIPPER (Impact prediction with lead angle)
    // ========================================================================
    Q_PROPERTY(float ccipX READ ccipX NOTIFY ccipPositionChanged)
    Q_PROPERTY(float ccipY READ ccipY NOTIFY ccipPositionChanged)
    Q_PROPERTY(bool ccipVisible READ ccipVisible NOTIFY ccipVisibleChanged)
    Q_PROPERTY(QString ccipStatus READ ccipStatus NOTIFY ccipStatusChanged)

    // ========================================================================
    // PROCEDURES (Zeroing, Environment)
    // ========================================================================
    Q_PROPERTY(QString zeroingText READ zeroingText NOTIFY zeroingTextChanged)
    Q_PROPERTY(bool zeroingVisible READ zeroingVisible NOTIFY zeroingVisibleChanged)

    Q_PROPERTY(QString environmentText READ environmentText NOTIFY environmentTextChanged)
    Q_PROPERTY(bool environmentVisible READ environmentVisible NOTIFY environmentVisibleChanged)

    Q_PROPERTY(QString windageText READ windageText NOTIFY windageTextChanged)
    Q_PROPERTY(bool windageVisible READ windageVisible NOTIFY windageVisibleChanged)

    Q_PROPERTY(QString detectionText READ detectionText NOTIFY detectionTextChanged)
    Q_PROPERTY(bool detectionVisible READ detectionVisible NOTIFY detectionVisibleChanged)
    Q_PROPERTY(QVariantList detectionBoxes READ detectionBoxes NOTIFY detectionBoxesChanged)

    // ========================================================================
    // ZONE WARNINGS
    // ========================================================================
    Q_PROPERTY(QString zoneWarningText READ zoneWarningText NOTIFY zoneWarningTextChanged)
    Q_PROPERTY(bool zoneWarningVisible READ zoneWarningVisible NOTIFY zoneWarningVisibleChanged)

    // ========================================================================
    // LEAD ANGLE & SCAN
    // ========================================================================
    Q_PROPERTY(QString leadAngleText READ leadAngleText NOTIFY leadAngleTextChanged)
    Q_PROPERTY(bool leadAngleVisible READ leadAngleVisible NOTIFY leadAngleVisibleChanged)

    Q_PROPERTY(QString scanNameText READ scanNameText NOTIFY scanNameTextChanged)
    Q_PROPERTY(bool scanNameVisible READ scanNameVisible NOTIFY scanNameVisibleChanged)

    Q_PROPERTY(bool lacActive READ lacActive NOTIFY lacActiveChanged)
    Q_PROPERTY(float rangeMeters READ rangeMeters NOTIFY rangeMetersChanged)
    Q_PROPERTY(float confidenceLevel READ confidenceLevel NOTIFY confidenceLevelChanged)

    // ========================================================================
    // STARTUP SEQUENCE & ERROR MESSAGES
    // ========================================================================
    Q_PROPERTY(QString startupMessageText READ startupMessageText NOTIFY startupMessageTextChanged)
    Q_PROPERTY(bool startupMessageVisible READ startupMessageVisible NOTIFY startupMessageVisibleChanged)

    Q_PROPERTY(QString errorMessageText READ errorMessageText NOTIFY errorMessageTextChanged)
    Q_PROPERTY(bool errorMessageVisible READ errorMessageVisible NOTIFY errorMessageVisibleChanged)

    // ========================================================================
    // DEVICE HEALTH STATUS (for warning displays)
    // ========================================================================
    Q_PROPERTY(bool dayCameraConnected READ dayCameraConnected NOTIFY dayCameraConnectedChanged)
    Q_PROPERTY(bool dayCameraError READ dayCameraError NOTIFY dayCameraErrorChanged)
    Q_PROPERTY(bool nightCameraConnected READ nightCameraConnected NOTIFY nightCameraConnectedChanged)
    Q_PROPERTY(bool nightCameraError READ nightCameraError NOTIFY nightCameraErrorChanged)

    Q_PROPERTY(bool azServoConnected READ azServoConnected NOTIFY azServoConnectedChanged)
    Q_PROPERTY(bool azFault READ azFault NOTIFY azFaultChanged)
    Q_PROPERTY(bool elServoConnected READ elServoConnected NOTIFY elServoConnectedChanged)
    Q_PROPERTY(bool elFault READ elFault NOTIFY elFaultChanged)

    Q_PROPERTY(bool lrfConnected READ lrfConnected NOTIFY lrfConnectedChanged)
    Q_PROPERTY(bool lrfFault READ lrfFault NOTIFY lrfFaultChanged)
    Q_PROPERTY(bool lrfOverTemp READ lrfOverTemp NOTIFY lrfOverTempChanged)

    Q_PROPERTY(bool actuatorConnected READ actuatorConnected NOTIFY actuatorConnectedChanged)
    Q_PROPERTY(bool actuatorFault READ actuatorFault NOTIFY actuatorFaultChanged)

    Q_PROPERTY(bool plc21Connected READ plc21Connected NOTIFY plc21ConnectedChanged)
    Q_PROPERTY(bool plc42Connected READ plc42Connected NOTIFY plc42ConnectedChanged)

    Q_PROPERTY(bool joystickConnected READ joystickConnected NOTIFY joystickConnectedChanged)

    Q_PROPERTY(bool ammunitionLevel READ ammunitionLevel NOTIFY ammunitionLevelChanged)

    // ========================================================================
    // AMMUNITION FEED STATUS (for OSD display with animation)
    // States: 0=Idle, 1=Extending, 2=Retracting, 3=Fault
    // ========================================================================
    Q_PROPERTY(int ammoFeedState READ ammoFeedState NOTIFY ammoFeedStateChanged)
    Q_PROPERTY(bool ammoFeedCycleInProgress READ ammoFeedCycleInProgress NOTIFY ammoFeedCycleInProgressChanged)
    Q_PROPERTY(bool ammoLoaded READ ammoLoaded NOTIFY ammoLoadedChanged)

    // ========================================================================
    // SERVO DRIVER DEBUG DATA (for diagnostic OSD overlay)
    // ========================================================================
    Q_PROPERTY(bool servoDebugVisible READ servoDebugVisible NOTIFY servoDebugVisibleChanged)
    // Azimuth Servo
    Q_PROPERTY(float azMotorTemp READ azMotorTemp NOTIFY servoDebugChanged)
    Q_PROPERTY(float azDriverTemp READ azDriverTemp NOTIFY servoDebugChanged)
    Q_PROPERTY(float azRpm READ azRpm NOTIFY servoDebugChanged)
    Q_PROPERTY(float azTorque READ azTorque NOTIFY servoDebugChanged)
    // Elevation Servo
    Q_PROPERTY(float elMotorTemp READ elMotorTemp NOTIFY servoDebugChanged)
    Q_PROPERTY(float elDriverTemp READ elDriverTemp NOTIFY servoDebugChanged)
    Q_PROPERTY(float elRpm READ elRpm NOTIFY servoDebugChanged)
    Q_PROPERTY(float elTorque READ elTorque NOTIFY servoDebugChanged)

    // ========================================================================
    // GYROSTABILIZATION DEBUG DATA (for diagnostic OSD overlay)
    // ========================================================================
    Q_PROPERTY(bool stabDebugVisible READ stabDebugVisible NOTIFY stabDebugVisibleChanged)
    Q_PROPERTY(bool stabDebugActive READ stabDebugActive NOTIFY stabDebugChanged)
    Q_PROPERTY(bool stabDebugWorldHeld READ stabDebugWorldHeld NOTIFY stabDebugChanged)
    // Gyro rates (mapped to stabilizer frame)
    Q_PROPERTY(double stabDebugP READ stabDebugP NOTIFY stabDebugChanged)
    Q_PROPERTY(double stabDebugQ READ stabDebugQ NOTIFY stabDebugChanged)
    Q_PROPERTY(double stabDebugR READ stabDebugR NOTIFY stabDebugChanged)
    // Position error
    Q_PROPERTY(double stabDebugAzError READ stabDebugAzError NOTIFY stabDebugChanged)
    Q_PROPERTY(double stabDebugElError READ stabDebugElError NOTIFY stabDebugChanged)
    // Position correction velocity
    Q_PROPERTY(double stabDebugAzPosCorr READ stabDebugAzPosCorr NOTIFY stabDebugChanged)
    Q_PROPERTY(double stabDebugElPosCorr READ stabDebugElPosCorr NOTIFY stabDebugChanged)
    // Rate feed-forward velocity
    Q_PROPERTY(double stabDebugAzRateFF READ stabDebugAzRateFF NOTIFY stabDebugChanged)
    Q_PROPERTY(double stabDebugElRateFF READ stabDebugElRateFF NOTIFY stabDebugChanged)
    // Final command velocity
    Q_PROPERTY(double stabDebugAzFinal READ stabDebugAzFinal NOTIFY stabDebugChanged)
    Q_PROPERTY(double stabDebugElFinal READ stabDebugElFinal NOTIFY stabDebugChanged)
    // User input velocity
    Q_PROPERTY(double stabDebugAzUser READ stabDebugAzUser NOTIFY stabDebugChanged)
    Q_PROPERTY(double stabDebugElUser READ stabDebugElUser NOTIFY stabDebugChanged)


public:
    explicit OsdViewModel(QObject *parent = nullptr);

    // Getters
    QColor accentColor() const { return m_accentColor; }
    QString modeText() const { return m_modeText; }
    QString motionText() const { return m_motionText; }
    QString stabText() const { return m_stabText; }
    QString cameraText() const { return m_cameraText; }
    QString speedText() const { return m_speedText; }

    float azimuth() const { return m_azimuth; }
    float elevation() const { return m_elevation; }

    bool imuConnected() const { return m_imuConnected; }
    double vehicleHeading() const { return m_vehicleHeading; }
    double vehicleRoll() const { return m_vehicleRoll; }
    double vehiclePitch() const { return m_vehiclePitch; }
    double imuTemperature() const { return m_imuTemperature; }

    QString statusText() const { return m_statusText; }
    QString rateText() const { return m_rateText; }
    QString lrfText() const { return m_lrfText; }
    QString fovText() const { return m_fovText; }

    // ⭐ Homing getters
    QString homingText() const { return m_homingText; }
    bool homingVisible() const { return m_homingVisible; }

    QRectF trackingBox() const { return m_trackingBox; }
    bool trackingBoxVisible() const { return m_trackingBoxVisible; }
    QColor trackingBoxColor() const { return m_trackingBoxColor; }
    bool trackingBoxDashed() const { return m_trackingBoxDashed; }
    bool isTrackingActive() const { return m_isTrackingActive; }
    float trackingConfidence() const { return m_trackingConfidence; }  // VPI tracking confidence
    bool trackingActive() const { return m_isTrackingActive; }         // Alias for backward compatibility

    QRectF acquisitionBox() const { return m_acquisitionBox; }
    bool acquisitionBoxVisible() const { return m_acquisitionBoxVisible; }

    int reticleType() const { return static_cast<int>(m_reticleType); }
    float reticleOffsetX() const { return m_reticleOffsetX; }
    float reticleOffsetY() const { return m_reticleOffsetY; }
    float currentFov() const { return m_currentFov; }

    float ccipX() const { return m_ccipX; }
    float ccipY() const { return m_ccipY; }
    bool ccipVisible() const { return m_ccipVisible; }
    QString ccipStatus() const { return m_ccipStatus; }

    QString zeroingText() const { return m_zeroingText; }
    bool zeroingVisible() const { return m_zeroingVisible; }

    QString environmentText() const { return m_environmentText; }
    bool environmentVisible() const { return m_environmentVisible; }

    QString windageText() const { return m_windageText; }
    bool windageVisible() const { return m_windageVisible; }

    QString detectionText() const { return m_detectionText; }
    bool detectionVisible() const { return m_detectionVisible; }
    QVariantList detectionBoxes() const { return m_detectionBoxes; }

    QString zoneWarningText() const { return m_zoneWarningText; }
    bool zoneWarningVisible() const { return m_zoneWarningVisible; }

    QString leadAngleText() const { return m_leadAngleText; }
    bool leadAngleVisible() const { return m_leadAngleVisible; }

    QString scanNameText() const { return m_scanNameText; }
    bool scanNameVisible() const { return m_scanNameVisible; }

    bool lacActive() const { return m_lacActive; }
    float rangeMeters() const { return m_rangeMeters; }
    float confidenceLevel() const { return m_confidenceLevel; }

    QString startupMessageText() const { return m_startupMessageText; }
    bool startupMessageVisible() const { return m_startupMessageVisible; }

    QString errorMessageText() const { return m_errorMessageText; }
    bool errorMessageVisible() const { return m_errorMessageVisible; }

    bool dayCameraConnected() const { return m_dayCameraConnected; }
    bool dayCameraError() const { return m_dayCameraError; }
    bool nightCameraConnected() const { return m_nightCameraConnected; }
    bool nightCameraError() const { return m_nightCameraError; }

    bool azServoConnected() const { return m_azServoConnected; }
    bool azFault() const { return m_azFault; }
    bool elServoConnected() const { return m_elServoConnected; }
    bool elFault() const { return m_elFault; }

    bool lrfConnected() const { return m_lrfConnected; }
    bool lrfFault() const { return m_lrfFault; }
    bool lrfOverTemp() const { return m_lrfOverTemp; }

    bool actuatorConnected() const { return m_actuatorConnected; }
    bool actuatorFault() const { return m_actuatorFault; }

    bool plc21Connected() const { return m_plc21Connected; }
    bool plc42Connected() const { return m_plc42Connected; }

    bool joystickConnected() const { return m_joystickConnected; }

    bool ammunitionLevel() const { return m_ammunitionLevel; }

    // Ammunition Feed Status getters
    int ammoFeedState() const { return m_ammoFeedState; }
    bool ammoFeedCycleInProgress() const { return m_ammoFeedCycleInProgress; }
    bool ammoLoaded() const { return m_ammoLoaded; }

    // Servo Driver debug getters
    bool servoDebugVisible() const { return m_servoDebugVisible; }
    float azMotorTemp() const { return m_stateData.azMotorTemp; }
    float azDriverTemp() const { return m_stateData.azDriverTemp; }
    float azRpm() const { return m_stateData.azRpm; }
    float azTorque() const { return m_stateData.azTorque; }
    float elMotorTemp() const { return m_stateData.elMotorTemp; }
    float elDriverTemp() const { return m_stateData.elDriverTemp; }
    float elRpm() const { return m_stateData.elRpm; }
    float elTorque() const { return m_stateData.elTorque; }

    // Toggle servoDebug visibility (for debugging)
    Q_INVOKABLE void toggleServoDebugVisible() {
        m_servoDebugVisible = !m_servoDebugVisible;
        emit servoDebugVisibleChanged();
    }

    // Gyrostabilization debug getters
    bool stabDebugVisible() const { return m_stabDebugVisible; }
    bool stabDebugActive() const { return m_stateData.stabDebug.stabActive; }
    bool stabDebugWorldHeld() const { return m_stateData.stabDebug.worldTargetHeld; }
    double stabDebugP() const { return m_stateData.stabDebug.p_dps; }
    double stabDebugQ() const { return m_stateData.stabDebug.q_dps; }
    double stabDebugR() const { return m_stateData.stabDebug.r_dps; }
    double stabDebugAzError() const { return m_stateData.stabDebug.azError_deg; }
    double stabDebugElError() const { return m_stateData.stabDebug.elError_deg; }
    double stabDebugAzPosCorr() const { return m_stateData.stabDebug.azPosCorr_dps; }
    double stabDebugElPosCorr() const { return m_stateData.stabDebug.elPosCorr_dps; }
    double stabDebugAzRateFF() const { return m_stateData.stabDebug.azRateFF_dps; }
    double stabDebugElRateFF() const { return m_stateData.stabDebug.elRateFF_dps; }
    double stabDebugAzFinal() const { return m_stateData.stabDebug.finalAz_dps; }
    double stabDebugElFinal() const { return m_stateData.stabDebug.finalEl_dps; }
    double stabDebugAzUser() const { return m_stateData.stabDebug.userAz_dps; }
    double stabDebugElUser() const { return m_stateData.stabDebug.userEl_dps; }

    // Toggle stabDebug visibility (for debugging)
    Q_INVOKABLE void toggleStabDebugVisible() {
        m_stabDebugVisible = !m_stabDebugVisible;
        emit stabDebugVisibleChanged();
    }

public slots:
    // Setters
    void setAccentColor(const QColor& color);

    // Update methods (called by OsdController)
    void updateMode(OperationalMode mode);
    void updateMotionMode(MotionMode mode);
    void updateHomingState(HomingState state);  // ⭐ Homing state update
    void updateStabilization(bool enabled);
    void updateCameraType(const QString& type);
    void updateSpeed(double speed);

    void updateAzimuth(float azimuth);
    void updateElevation(float elevation);
    void updateImuData(bool connected, double yaw, double pitch, double roll, double temp);

    void updateSystemStatus(bool charged, bool armed, bool ready);
    void updateFiringMode(FireMode mode);
    void updateLrfDistance(float distance);
    void updateFov(float fov);

    void updateTrackingBox(float x, float y, float width, float height);
    void updateTrackingState(VPITrackingState state);
    void updateTrackingPhase(TrackingPhase phase, bool hasValidTarget, const QRectF& acquisitionBox);
    void updateTrackingActive(bool active);

    void updateReticleType(ReticleType type);
    void updateReticleOffset(float x_px, float y_px);

    void updateCcipPipper(float x_px, float y_px, bool visible, const QString& status);

    void updateZeroingDisplay(bool modeActive, bool applied, float azOffset, float elOffset);
    void updateEnvironmentDisplay(float tempCelsius, float altitudeMeters);
    void updateWindageDisplay(bool windageApplied, float windSpeedKnots, float windDirectionDeg, float calculatedCrosswindMS);
    void updateDetectionDisplay(bool enabled);
    void updateDetectionBoxes(const std::vector<YoloDetection>& detections);

    void updateZoneWarning(bool inNoFireZone, bool inNoTraverseLimit);
    void updateLeadAngleDisplay(const QString& statusText);
    void updateCurrentScanName(const QString& scanName);

    void updateLacActive(bool active);
    void updateRangeMeters(float range);
    void updateConfidenceLevel(float confidence);      // LAC confidence
    void updateTrackingConfidence(float confidence);   // VPI tracking confidence

    void updateStartupMessage(const QString& message, bool visible);
    void updateErrorMessage(const QString& message, bool visible);

    void updateDeviceHealth(bool dayCamConnected, bool dayCamError,
                           bool nightCamConnected, bool nightCamError,
                           bool azServoConnected, bool azFault,
                           bool elServoConnected, bool elFault,
                           bool lrfConnected, bool lrfFault, bool lrfOverTemp,
                           bool actuatorConnected, bool actuatorFault,
                           bool imuConnected,
                           bool plc21Connected, bool plc42Connected,
                           bool joystickConnected);
    void updateAmmunitionLevel(bool level);

    // Ammunition Feed Status update
    void updateAmmoFeedStatus(int state, bool cycleInProgress, bool loaded);

    // Gyrostabilization debug update (called by OsdController from SystemStateData)
    void updateStabDebug(const SystemStateData& data);


signals:
    void accentColorChanged();
    void modeTextChanged();
    void motionTextChanged();
    void stabTextChanged();
    void cameraTextChanged();
    void speedTextChanged();

    void azimuthChanged();
    void elevationChanged();

    // === IMU SIGNALS ===
    void imuConnectedChanged();
    void vehicleHeadingChanged();
    void vehicleRollChanged();
    void vehiclePitchChanged();
    void imuTemperatureChanged();

    void statusTextChanged();
    void rateTextChanged();
    void lrfTextChanged();
    void fovTextChanged();

    // ⭐ Homing signals
    void homingTextChanged();
    void homingVisibleChanged();

    void trackingBoxChanged();
    void trackingBoxVisibleChanged();
    void trackingBoxColorChanged();
    void trackingBoxDashedChanged();
    void isTrackingActiveChanged();
    void trackingConfidenceChanged();
    void trackingActiveChanged();

    void acquisitionBoxChanged();
    void acquisitionBoxVisibleChanged();

    void reticleTypeChanged();
    void reticleOffsetChanged();
    void currentFovChanged();

    void ccipPositionChanged();
    void ccipVisibleChanged();
    void ccipStatusChanged();

    void zeroingTextChanged();
    void zeroingVisibleChanged();

    void environmentTextChanged();
    void environmentVisibleChanged();

    void windageTextChanged();
    void windageVisibleChanged();

    void detectionTextChanged();
    void detectionVisibleChanged();
    void detectionBoxesChanged();

    void zoneWarningTextChanged();
    void zoneWarningVisibleChanged();

    void leadAngleTextChanged();
    void leadAngleVisibleChanged();

    void scanNameTextChanged();
    void scanNameVisibleChanged();

    void lacActiveChanged();
    void rangeMetersChanged();
    void confidenceLevelChanged();

    void startupMessageTextChanged();
    void startupMessageVisibleChanged();

    void errorMessageTextChanged();
    void errorMessageVisibleChanged();

    void dayCameraConnectedChanged();
    void dayCameraErrorChanged();
    void nightCameraConnectedChanged();
    void nightCameraErrorChanged();

    void azServoConnectedChanged();
    void azFaultChanged();
    void elServoConnectedChanged();
    void elFaultChanged();

    void lrfConnectedChanged();
    void lrfFaultChanged();
    void lrfOverTempChanged();

    void actuatorConnectedChanged();
    void actuatorFaultChanged();

    void plc21ConnectedChanged();
    void plc42ConnectedChanged();

    void joystickConnectedChanged();

    void ammunitionLevelChanged();

    // Ammunition Feed Status signals
    void ammoFeedStateChanged();
    void ammoFeedCycleInProgressChanged();
    void ammoLoadedChanged();

    // Servo Driver debug signals
    void servoDebugVisibleChanged();
    void servoDebugChanged();

    // Gyrostabilization debug signals
    void stabDebugVisibleChanged();
    void stabDebugChanged();


private:
    // Member variables
    QColor m_accentColor;
    QString m_modeText;
    QString m_motionText;
    QString m_stabText;
    QString m_cameraText;
    QString m_speedText;

    float m_azimuth;
    float m_elevation;

     bool m_imuConnected = false;
    double m_vehicleHeading = 0.0;    // Yaw/Heading (0-360°)
    double m_vehicleRoll = 0.0;       // Roll angle
    double m_vehiclePitch = 0.0;      // Pitch angle
    double m_imuTemperature = 0.0;    // IMU temp
    
    
    QString m_statusText;
    QString m_rateText;
    QString m_lrfText;
    QString m_fovText;

    // ⭐ Homing state
    QString m_homingText;
    bool m_homingVisible;

    QRectF m_trackingBox;
    bool m_trackingBoxVisible;
    QColor m_trackingBoxColor;
    bool m_trackingBoxDashed;
    bool m_isTrackingActive;

    QRectF m_acquisitionBox;
    bool m_acquisitionBoxVisible;

    ReticleType m_reticleType;
    float m_reticleOffsetX;
    float m_reticleOffsetY;
    float m_currentFov;

    float m_ccipX;
    float m_ccipY;
    bool m_ccipVisible;
    QString m_ccipStatus;

    QString m_zeroingText;
    bool m_zeroingVisible;

    QString m_environmentText;
    bool m_environmentVisible;

    QString m_windageText;
    bool m_windageVisible;

    QString m_detectionText;
    bool m_detectionVisible;
    QVariantList m_detectionBoxes;

    QString m_zoneWarningText;
    bool m_zoneWarningVisible;

    QString m_leadAngleText;
    bool m_leadAngleVisible;

    QString m_scanNameText;
    bool m_scanNameVisible;

    // Internal state for calculations
    bool m_sysCharged;
    bool m_sysArmed;
    bool m_sysReady;
    FireMode m_fireMode;

    // Screen dimensions (for reticle offset calculations)
    int m_screenWidth;
    int m_screenHeight;

    bool m_lacActive;
    float m_rangeMeters;
    float m_confidenceLevel;           // LAC confidence (lead angle compensation)
    float m_trackingConfidence;        // VPI tracking confidence (separate from LAC)

    QString m_startupMessageText;
    bool m_startupMessageVisible;

    QString m_errorMessageText;
    bool m_errorMessageVisible;

    bool m_dayCameraConnected;
    bool m_dayCameraError;
    bool m_nightCameraConnected;
    bool m_nightCameraError;

    bool m_azServoConnected;
    bool m_azFault;
    bool m_elServoConnected;
    bool m_elFault;

    bool m_lrfConnected;
    bool m_lrfFault;
    bool m_lrfOverTemp;

    bool m_actuatorConnected;
    bool m_actuatorFault;

    bool m_plc21Connected;
    bool m_plc42Connected;

    bool m_joystickConnected;

    bool m_ammunitionLevel;

    // Ammunition Feed Status
    int m_ammoFeedState = 0;  // 0=Idle, 1=Extending, 2=Retracting, 3=Fault
    bool m_ammoFeedCycleInProgress = false;
    bool m_ammoLoaded = false;

    // Servo Driver debug data
    bool m_servoDebugVisible = true;  // Default to visible for debugging

    // Gyrostabilization debug data
    bool m_stabDebugVisible = true;  // Default to visible for debugging
    SystemStateData m_stateData;      // Cached state data for stabDebug and servoDebug access
};

#endif // OSDVIEWMODEL_H
