#ifndef CAMERAVIDEOSTREAMDEVICE_H
#define CAMERAVIDEOSTREAMDEVICE_H

// ============================================================================
// INCLUDES
// ============================================================================

// Standard Library
#include <atomic>
#include <string>
#include <vector>

// Qt Framework
#include <QThread>
#include <QImage>
#include <QMutex>
#include <QRect>
#include <QColor>
#include <QObject>
#include <QString>
#include <QFuture>  // ✅ For tracking async detection tasks

// GStreamer
#include <gst/gst.h>
#include <gst/app/gstappsink.h>

// NVIDIA VPI (Vision Programming Interface)
#include <vpi/Types.h>
#include <vpi/Array.h>
#include <vpi/Image.h>
#include <vpi/Stream.h>
#include <vpi/algo/ConvertImageFormat.h>
#include <vpi/algo/CropScaler.h>
#include <vpi/algo/DCFTracker.h>
#include <vpi/OpenCVInterop.hpp>

// OpenCV
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

// Project
#include "utils/inference.h"
#include "models/domain/systemstatemodel.h"

// ============================================================================
// DATA STRUCTURES
// ============================================================================

/**
 * @brief Frame data structure containing processed video and all system metadata
 *
 * This structure combines the video frame with all relevant system state data
 * including tracking, ballistics, sensors, and weapon system status. It is
 * emitted from the video processing thread to the OSD renderer.
 */
struct FrameData {
    // Camera & Image Data
    int cameraIndex = -1;
    QImage baseImage;
    float cameraFOV = 0.0f;

    // VPI Tracking Data
    bool trackingEnabled = false;
    bool trackerInitialized = false;
    VPITrackingState trackingState = VPI_TRACKING_STATE_LOST;
    QRect trackingBbox = QRect(0, 0, 0, 0);
    float trackingConfidence = 0.0f;
    TrackingPhase currentTrackingPhase = TrackingPhase::Off;
    bool trackerHasValidTarget = false;
    float acquisitionBoxX_px = 0.0f;
    float acquisitionBoxY_px = 0.0f;
    float acquisitionBoxW_px = 0.0f;
    float acquisitionBoxH_px = 0.0f;

    // YOLO Object Detection
    bool detectionEnabled = false;
    std::vector<YoloDetection> detections;

    // Gimbal State
    OperationalMode currentOpMode = OperationalMode::Idle;
    MotionMode motionMode = MotionMode::Manual;
    bool stabEnabled = false;
    float azimuth = 0.0f;
    float elevation = 0.0f;
    float speed = 0.0f;

    // IMU Sensor Data
    bool imuConnected = false;
    double imuRollDeg;
    double imuPitchDeg;
    double imuYawDeg;
    double imuTemp;
    double gyroX;
    double gyroY;
    double gyroZ;
    double accelX;
    double accelY;
    double accelZ;

    // Laser Range Finder
    float lrfDistance = 0.0f;

    // Weapon System Status
    bool sysCharged = false;
    bool gunArmed = false;
    bool sysReady = false;
    FireMode fireMode = FireMode::SingleShot;
    bool stationAmmunitionLevel = false;

    // Ballistics - Zeroing
    bool zeroingModeActive = false;
    float zeroingAzimuthOffset = 0.0f;
    float zeroingElevationOffset = 0.0f;
    bool zeroingAppliedToBallistics = false;

    // Ballistics - Windage
    bool windageModeActive = false;
    float windageSpeedKnots = 0.0f;
    float windageDirectionDegrees = 0.0f;
    bool windageAppliedToBallistics = false;
    float calculatedCrosswindMS = 0.0f;

    // Ballistics - Lead Angle Compensation
    bool leadAngleActive = false;
    LeadAngleStatus leadAngleStatus;
    float leadAngleOffsetAz_deg;
    float leadAngleOffsetEl_deg;
    QString leadStatusText;

    // Fire Control - Aiming Points
    int reticleAimpointImageX_px;      // Gun boresight with zeroing ONLY
    int reticleAimpointImageY_px;      // Gun boresight with zeroing ONLY
    float ccipImpactImageX_px = 0.0f;  // CCIP: bullet impact with zeroing + lead
    float ccipImpactImageY_px = 0.0f;  // CCIP: bullet impact with zeroing + lead

    // Safety Zones
    bool isReticleInNoFireZone = false;
    bool gimbalStoppedAtNTZLimit = false;

    // OSD Display
    ReticleType reticleType = ReticleType::BoxCrosshair;
    QColor colorStyle = QColor(70, 226, 165);
    QString currentScanName = "";
};

// ============================================================================
// CLASS DEFINITION
// ============================================================================

/**
 * @brief Video stream processor with VPI tracking and YOLO detection
 *
 * This class runs in a dedicated thread and processes video frames from a
 * GStreamer pipeline. It performs:
 * - VPI-accelerated object tracking (DCF tracker on CUDA)
 * - YOLO object detection (async on GPU)
 * - Frame format conversion (YUY2 → BGRA)
 * - System state synchronization
 * - OSD metadata preparation
 *
 * Architecture:
 * - GStreamer: Video capture and preprocessing
 * - VPI (CUDA): High-performance tracking
 * - YOLO (CUDA): Object detection
 * - Qt Signals: Thread-safe communication
 */
class CameraVideoStreamDevice : public QThread
{
    Q_OBJECT

public:
    // ========================================================================
    // PUBLIC INTERFACE
    // ========================================================================

    explicit CameraVideoStreamDevice(int cameraIndex,
                                     const QString &deviceName,
                                     int sourceWidth,
                                     int sourceHeight,
                                     SystemStateModel* stateModel,
                                     QObject *parent = nullptr);
    ~CameraVideoStreamDevice() override;

    void stop();

public slots:
    void setTrackingEnabled(bool enabled);
    void setDetectionEnabled(bool enabled);
    void onSystemStateChanged(const SystemStateData &newState);

signals:
    void frameDataReady(std::shared_ptr<const FrameData> data);
    void processingError(int cameraIndex, const QString &errorMessage);
    void statusUpdate(int cameraIndex, const QString &statusMessage);

protected:
    // ========================================================================
    // THREAD EXECUTION
    // ========================================================================

    void run() override;

private:
    // ========================================================================
    // PRIVATE METHODS
    // ========================================================================

    // GStreamer Pipeline
    bool initializeGStreamer();
    void cleanupGStreamer();
    static GstFlowReturn on_new_sample_from_sink(GstAppSink *sink, gpointer user_data);
    GstFlowReturn handleNewSample(GstAppSink *sink);

    // VPI Processing
    bool initializeVPI();
    void cleanupVPI();
    bool processFrame(GstBuffer *buffer);
    bool initializeFirstTarget(VPIImage vpiFrameInput, float boxX, float boxY, float boxW, float boxH);
    bool runTrackingCycle(VPIImage vpiFrameInput);

    // Utilities
    QImage cvMatToQImage(const cv::Mat &inMat);

    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================

    // --- Configuration & Identification ---
    const int m_cameraIndex;
    const QString m_deviceName;
    const int m_sourceWidth;
    const int m_sourceHeight;
    int m_outputWidth;
    int m_outputHeight;
    SystemStateModel* m_stateModel;
    const int m_maxTrackedTargets;
    std::atomic<bool> m_abortRequest;

    // --- GStreamer Components ---
    GstElement *m_pipeline;
    GstElement *m_appSink;
    GMainLoop *m_gstLoop;

    // --- VPI Components ---
    VPIBackend m_vpiBackend;
    VPIStream m_vpiStream;
    VPIPayload m_dcfPayload;
    VPIPayload m_cropScalePayload;
    VPIImage m_vpiFrameNV12;
    VPIImage m_vpiTgtPatches;
    VPIArray m_vpiInTargets;
    VPIArray m_vpiOutTargets;
    VPIArray m_vpiConfidenceScores;
    VPIImage m_vpiCorrelationMap;
    int m_vpiTgtPatchSize;
    int m_vpiFeaturePatchSize;

    // --- VPI Tracking State ---
    VPIDCFTrackedBoundingBox m_currentTarget;
    std::atomic<bool> m_trackingEnabled;
    bool m_trackerInitialized;
    QElapsedTimer m_velocityTimer;
    float m_lastTargetCenterX_px;
    float m_lastTargetCenterY_px;
    float m_currentConfidence;
    float m_smoothedConfidence;

    // --- YOLO Detection (Async) ---
    YoloInference m_inference;
    std::atomic<bool> m_detectionEnabled;
    QMutex m_detectionMutex;
    std::vector<YoloDetection> m_latestDetections;
    QElapsedTimer m_lastDetectionTime;
    bool m_detectionInProgress;
    cv::Mat m_detectionFrame;
    QFuture<void> m_detectionFuture;  // ✅ MEMORY LEAK FIX: Track async task to prevent accumulation

    // --- OpenCV Buffers ---
    cv::Mat m_yuy2_host_buffer;

    // --- Cropping Configuration ---
    int m_cropTop;
    int m_cropBottom;
    int m_cropLeft;
    int m_cropRight;

    // --- System State (Thread-Safe with Mutex) ---
    QMutex m_stateMutex;

    // Gimbal State
    OperationalMode m_currentMode;
    MotionMode m_motionMode;
    bool m_stabEnabled;
    float m_currentAzimuth;
    float m_currentElevation;
    float m_speed;

    // IMU Data
    bool m_imuConnected;
    double m_imuRollDeg;
    double m_imuPitchDeg;
    double m_imuYawDeg;
    double m_imuTemp;
    double m_gyroX, m_gyroY, m_gyroZ;
    double m_accelX, m_accelY, m_accelZ;

    // Sensors
    float m_cameraFOV;
    float m_lrfDistance;

    // Weapon System
    bool m_sysCharged;
    bool m_sysArmed;
    bool m_sysReady;
    bool m_currentAmmunitionLevel;
    FireMode m_fireMode;

    // Ballistics - Zeroing
    bool m_currentZeroingModeActive;
    bool m_currentZeroingApplied;
    float m_currentZeroingAzOffset;
    float m_currentZeroingElOffset;

    // Ballistics - Windage
    bool m_currentWindageModeActive;
    bool m_currentWindageApplied;
    float m_currentWindageSpeed;
    float m_currentWindageDirection;
    float m_currentCalculatedCrosswind;

    // Ballistics - Lead Angle
    bool m_isLacActiveForReticle;
    LeadAngleStatus m_currentLeadAngleStatus;
    float m_currentLeadAngleOffsetAz;
    float m_currentLeadAngleOffsetEl;
    QString m_currentLeadStatusText;

    // Fire Control
    int m_currentReticleAimpointImageX_px;
    int m_currentReticleAimpointImageY_px;
    float m_currentCcipImpactImageX_px;
    float m_currentCcipImpactImageY_px;

    // Safety Zones
    bool m_currentIsReticleInNoFireZone;
    bool m_currentGimbalStoppedAtNTZLimit;

    // Tracking Phase Management
    TrackingPhase m_currentTrackingPhase;
    bool m_trackerHasValidTarget;
    int m_currentAcquisitionBoxX_px;
    int m_currentAcquisitionBoxY_px;
    int m_currentAcquisitionBoxW_px;
    int m_currentAcquisitionBoxH_px;
    bool m_currentActiveCameraIsDay;

    // OSD Display
    ReticleType m_reticleType;
    QColor m_colorStyle;
    QString m_currentScanName;

    // --- Performance Metrics ---
    QElapsedTimer m_latencyTimer;
    qint64 m_frameArrivalTime;
    qint64 m_totalLatency_ms;
    qint64 m_frameProcessedCount;
    int m_frameCount;
};

#endif // CAMERAVIDEOSTREAMDEVICE_H
