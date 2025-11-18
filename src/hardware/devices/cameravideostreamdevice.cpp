#include "cameravideostreamdevice.h"
#include "vpi_helpers.h" // For CHECK_VPI_STATUS

#include <QDebug>
#include <QElapsedTimer>
#include <QtConcurrent/QtConcurrent>
#include <stdexcept>
#include <algorithm> // For std::min, std::max

#include <opencv2/imgcodecs.hpp>
#include <cuda_runtime.h>


CameraVideoStreamDevice::CameraVideoStreamDevice(int cameraIndex,
                               const QString &deviceName,
                               int sourceWidth,
                               int sourceHeight,
                               SystemStateModel* stateModel,
                               QObject *parent)
    : QThread(parent), // Base class first
    // Configuration & Identification (in declaration order)
    m_cameraIndex(cameraIndex),
    m_deviceName(deviceName),
    m_sourceWidth(sourceWidth),
    m_sourceHeight(sourceHeight),
    m_outputWidth(1024),
    m_outputHeight(768),
    m_stateModel(stateModel),
    m_maxTrackedTargets(1),     // const int - declared early
    m_abortRequest(false),      // atomic<bool> - declared after maxTrackedTargets
    
    // State variables in declaration order
    m_stabEnabled(false),
    m_currentAzimuth(0.0f),
    m_currentElevation(0.0f),
    m_cameraFOV(10.4f),
    m_lrfDistance(0.0f),
    
    m_sysCharged(true),
    m_sysArmed(false),
    m_sysReady(true),           // bool - declared before other atomics
    m_speed(0.0f),
    m_trackingEnabled(false),   // atomic<bool> - declared after m_sysReady
    m_trackerInitialized(false),
    m_detectionEnabled(false),  // atomic<bool> - declared last among detection flags
    
    // GStreamer Components
    m_pipeline(nullptr),
    m_appSink(nullptr),
    m_gstLoop(nullptr),
    
    // VPI Components & State (in declaration order)
    m_vpiBackend(VPI_BACKEND_CUDA),
    m_vpiStream(nullptr),
    m_dcfPayload(nullptr),
    m_cropScalePayload(nullptr),
    m_vpiFrameNV12(nullptr),
    m_vpiTgtPatches(nullptr),
    m_vpiInTargets(nullptr),
    m_vpiOutTargets(nullptr),
    m_vpiConfidenceScores(nullptr),
    m_vpiCorrelationMap(nullptr),
    m_vpiTgtPatchSize(0),
    m_vpiFeaturePatchSize(0),
    m_currentTarget(),          // VPIDCFTrackedBoundingBox
    m_velocityTimer(),          // QElapsedTimer
    m_lastTargetCenterX_px(0.0f),
    m_lastTargetCenterY_px(0.0f),
    m_currentConfidence(0.0f),  // Tracking confidence score
    m_smoothedConfidence(0.0f), // Smoothed confidence for display

    // OpenCV Buffers
    m_yuy2_host_buffer(),       // cv::Mat
    
    // State Variables (in declaration order from header)
    m_currentMode(OperationalMode::Surveillance),
    m_motionMode(MotionMode::Manual),
    m_currentZeroingModeActive(false),
    m_currentZeroingApplied(false),
    m_currentZeroingAzOffset(0.0f),
    m_currentZeroingElOffset(0.0f),
    
    m_currentWindageModeActive(false),
    m_currentWindageApplied(false),
    m_currentWindageSpeed(0.0f),
    m_currentWindageDirection(0.0f),
    m_currentCalculatedCrosswind(0.0f),
    m_currentIsReticleInNoFireZone(false),
    m_currentGimbalStoppedAtNTZLimit(false),
    m_currentReticleAimpointImageX_px(0),
    m_currentReticleAimpointImageY_px(0),
    m_currentLeadStatusText(""),
    m_currentScanName(""),
    m_currentTrackingPhase(TrackingPhase::Off), // Assuming default value
    m_trackerHasValidTarget(false),
    m_currentAcquisitionBoxX_px(0),
    m_currentAcquisitionBoxY_px(0),
    m_currentAcquisitionBoxW_px(0),
    m_currentAcquisitionBoxH_px(0),
    m_currentActiveCameraIsDay(true),
    m_fireMode(FireMode::SingleShot),
    m_reticleType(ReticleType::BoxCrosshair),
    m_colorStyle(70, 226, 165),
    m_isLacActiveForReticle(false),
    
    // YoloInference Engine (last member)
    // CUDA is always enabled for GPU acceleration
    // Inference runs only on day camera (controlled by runtime check at line 614)
    m_inference("/home/rapit/yolov8s.onnx",
                cv::Size(640, 640),
                "", // classes.txt path
                true), // use CUDA for GPU acceleration

    // Async Detection Members
    m_detectionInProgress(false),

    // Latency Measurement
    m_frameArrivalTime(0),
    m_totalLatency_ms(0),
    m_frameProcessedCount(0),

    // Frame counter
    m_frameCount(0)
    // m_stateMutex is default constructed (no initialization needed)
{
    // Start latency timer
    m_latencyTimer.start();
    m_lastDetectionTime.start();

    memset(&m_currentTarget, 0, sizeof(m_currentTarget));
    m_currentTarget.state = VPI_TRACKING_STATE_LOST;

        // Sanity check calculated width (should be even for YUY2)
        if (m_outputWidth % 2 != 0) {
            qWarning() << "Calculated output width" << m_outputWidth << "is odd, adjusting to" << m_outputWidth - 1;
            m_outputWidth--;
        }
        qInfo() << "Cam" << cameraIndex << ": Source Dim=" << m_sourceWidth << "x" << m_sourceHeight
                << ", Output Dim=" << m_outputWidth << "x" << m_outputHeight;


    // Initialize OSD state variables
    m_currentMode = OperationalMode::Idle; // Uses OpMode from osdrenderer.h via cameravideostreamdevice.h
    m_motionMode = MotionMode::Manual;
    m_stabEnabled = false;
    m_lrfDistance = 0.0f;
    m_sysCharged = false;
    m_sysArmed = false;
    m_sysReady = false;
    m_speed = 0.0;
    m_fireMode = FireMode::SingleShot; // Assuming this is defined in osdrenderer.h

    m_cameraFOV = 45.0f;
    m_currentAzimuth = 0.0f;
    m_currentElevation = 0.0f;

    if (m_cameraIndex == 0) {                // Sony (day)
        m_cropTop = 0;
        m_cropBottom = 0;
        m_cropLeft = 0;
        m_cropRight = 0; // No cropping for Sony day camera
    } else {                                 // FLIR (night)  videocrop top=28 left=54  bottom=24  right=46
        m_cropTop = 28;
        m_cropBottom = 60;
        m_cropLeft = 116;
        m_cropRight = 116; // Cropping for FLIR night camera

    }


}

// Destructor (No changes needed based on errors)
CameraVideoStreamDevice::~CameraVideoStreamDevice()
{
    qInfo() << "CameraVideoStreamDevice destructor called for Cam" << m_cameraIndex;
    if (isRunning()) {
        stop();
        wait(1500);
        if (isRunning()) {
             qWarning() << "Cam" << m_cameraIndex << ": Thread still running in destructor, terminating forcefully.";
             terminate();
             wait();
        }
    }
    cleanupVPI();
    cleanupGStreamer();
    qInfo() << "CameraVideoStreamDevice cleanup complete for Cam" << m_cameraIndex;
}

// stop() method (No changes needed based on errors)
void CameraVideoStreamDevice::stop()
{
    qInfo() << "Stop requested for CameraVideoStreamDevice Cam" << m_cameraIndex;
    m_abortRequest.store(true);

    if (m_gstLoop && g_main_loop_is_running(m_gstLoop)) {
         qInfo() << "Cam" << m_cameraIndex << ": Quitting GStreamer main loop.";
         g_main_loop_quit(m_gstLoop);
    } else {
         qDebug() << "Cam" << m_cameraIndex << ": GStreamer main loop not running or null when stop requested.";
    }
}



// setTrackingEnabled() method (No changes needed based on errors)
void CameraVideoStreamDevice::setTrackingEnabled(bool enabled)
{
    qInfo() << "Cam" << m_cameraIndex << ": Setting tracking enabled state to:" << enabled;
    m_trackingEnabled.store(enabled);

    if (!enabled) {
        m_trackerInitialized = false;
        m_smoothedConfidence = 0.0f;  // Reset smoothed confidence for fresh start
        qInfo() << "Cam" << m_cameraIndex << ": Tracking disabled, tracker marked for re-initialization.";
        m_currentTarget.state = VPI_TRACKING_STATE_LOST;
    }
}

void CameraVideoStreamDevice::setDetectionEnabled(bool enabled)
{
    qInfo() << "Cam" << m_cameraIndex << ": Setting detection enabled state to:" << enabled;
    m_detectionEnabled.store(enabled);
}

// run() method (No changes needed based on errors)
void CameraVideoStreamDevice::run()
{
    qInfo() << "CameraVideoStreamDevice thread started for Camera" << m_cameraIndex;
    emit statusUpdate(m_cameraIndex, "Initializing...");

    bool vpiInitialized = false;
    bool gstInitialized = false;

    try {
        emit statusUpdate(m_cameraIndex, "Initializing GStreamer...");
        gstInitialized = initializeGStreamer();
        if (!gstInitialized) throw std::runtime_error("GStreamer initialization failed.");
        qInfo() << "GStreamer initialized successfully for Camera" << m_cameraIndex;

        emit statusUpdate(m_cameraIndex, "Initializing VPI...");
        vpiInitialized = initializeVPI();
        if (!vpiInitialized) throw std::runtime_error("VPI initialization failed.");
        qInfo() << "VPI initialized successfully for Camera" << m_cameraIndex;

        m_yuy2_host_buffer.create(m_sourceHeight, m_sourceWidth, CV_8UC2);

        emit statusUpdate(m_cameraIndex, "Starting GStreamer pipeline...");
        if (gst_element_set_state(m_pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
            throw std::runtime_error("Failed to set GStreamer pipeline to PLAYING state.");
        }
        qInfo() << "GStreamer pipeline is PLAYING for Camera" << m_cameraIndex;

        emit statusUpdate(m_cameraIndex, "Processing video...");
        qInfo() << "Running GStreamer main loop for Camera" << m_cameraIndex;
        g_main_loop_run(m_gstLoop);
        qInfo() << "GStreamer main loop finished for Camera" << m_cameraIndex;

    } catch (const std::exception &e) {
        QString errorMsg = QString("Init/Runtime Error: %1").arg(e.what());
        emit processingError(m_cameraIndex, errorMsg);
        qCritical() << "Cam" << m_cameraIndex << ": Exception in run():" << e.what();
    } catch (...) {
        emit processingError(m_cameraIndex, "Unknown error during init/runtime.");
        qCritical() << "Cam" << m_cameraIndex << ": Unknown exception in run()";
    }

    // Cleanup sequence
    emit statusUpdate(m_cameraIndex, "Stopping pipeline and cleaning up...");
    qInfo() << "Cam" << m_cameraIndex << ": Starting cleanup sequence...";

    if (m_pipeline) {
        qInfo() << "Cam" << m_cameraIndex << ": Setting GStreamer pipeline to NULL state...";
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
        qInfo() << "Cam" << m_cameraIndex << ": Pipeline state set to NULL.";
    }

    if (vpiInitialized) {
        qInfo() << "Cam" << m_cameraIndex << ": Cleaning up VPI resources...";
        cleanupVPI();
         qInfo() << "Cam" << m_cameraIndex << ": VPI cleanup finished.";
    }

    if (gstInitialized) {
        qInfo() << "Cam" << m_cameraIndex << ": Cleaning up GStreamer resources...";
        cleanupGStreamer();
        qInfo() << "Cam" << m_cameraIndex << ": GStreamer cleanup finished.";
    }

    emit statusUpdate(m_cameraIndex, "Processing stopped.");
    qInfo() << "CameraVideoStreamDevice thread finished for Camera" << m_cameraIndex;
}

void CameraVideoStreamDevice::onSystemStateChanged(const SystemStateData &newState)
{
    // Lock the mutex to safely update member variables
    // These members will be read by processFrame in the same thread,
    // but locking ensures atomicity if multiple signals arrive quickly
    // or if you were to access these members from other slots/threads.
  QMutexLocker locker(&m_stateMutex);

    // Update members based on the new state data
    m_currentMode = newState.opMode;
    m_motionMode = newState.motionMode;
    m_stabEnabled = newState.enableStabilization;
    m_currentAzimuth = newState.gimbalAz;         // Assuming gimbalAz is the display value
    m_currentElevation = newState.gimbalEl;       // Assuming gimbalEl is the display value
    m_imuConnected = newState.imuConnected;
    m_imuRollDeg = newState.imuRollDeg;
    m_imuPitchDeg = newState.imuPitchDeg;
    m_imuYawDeg = newState.imuYawDeg;       // Vehicle heading
    m_imuTemp = newState.imuTemp;
    m_gyroX = newState.GyroX;
    m_gyroY = newState.GyroY;
    m_gyroZ = newState.GyroZ;
    m_accelX = newState.AccelX;
    m_accelY = newState.AccelY;
    m_accelZ = newState.AccelZ;
    m_lrfDistance = newState.currentTargetRange;
    m_sysCharged = newState.ammoLoaded; // Map SystemStateData fields to your OSD needs
    m_sysArmed = newState.gunArmed;
    m_sysReady = newState.isReady();      // Use the helper function? Or specific flags
    m_cameraFOV = newState.activeCameraIsDay ? newState.dayCurrentHFOV : newState.nightCurrentHFOV; // Example: Get FOV based on active cam
    m_speed = newState.gimbalSpeed; // Assuming gimbalSpeed is the speed value
    m_fireMode = newState.fireMode; // Assuming fireMode is the rate mode
    m_reticleType = newState.reticleType; // Assuming reticleType is the type
    m_colorStyle = newState.colorStyle; // Assuming colorStyle is the color style
     m_currentZeroingModeActive  = newState.zeroingModeActive  ;  
     m_currentZeroingApplied = newState.zeroingAppliedToBallistics;
     m_currentZeroingAzOffset = newState.zeroingAzimuthOffset;
     m_currentZeroingElOffset = newState.zeroingElevationOffset;   

     m_currentWindageModeActive  = newState.windageModeActive;
     m_currentWindageApplied = newState.windageAppliedToBallistics;
    m_currentWindageSpeed = newState.windageSpeedKnots; // Assuming this is the windage speed in knots
    m_currentWindageDirection = newState.windageDirectionDegrees;
    m_currentCalculatedCrosswind = newState.calculatedCrosswindMS;  
 
    m_currentIsReticleInNoFireZone  = newState.isReticleInNoFireZone; // Assuming this is the no-fire zone status
    // Update the gimbal stopped at NTZ limit status
    m_currentGimbalStoppedAtNTZLimit = newState.isReticleInNoTraverseZone; // Assuming this is the NTZ limit status
    m_isLacActiveForReticle = newState.leadAngleCompensationActive; // Assuming this is the LAC status
    m_currentReticleAimpointImageX_px= newState.reticleAimpointImageX_px; // Reticle: gun boresight with zeroing ONLY
    m_currentReticleAimpointImageY_px= newState.reticleAimpointImageY_px; // Reticle: gun boresight with zeroing ONLY
    m_currentCcipImpactImageX_px = newState.ccipImpactImageX_px; // ✅ CCIP: bullet impact with zeroing + lead
    m_currentCcipImpactImageY_px = newState.ccipImpactImageY_px; // ✅ CCIP: bullet impact with zeroing + lead
     m_currentLeadStatusText= newState.leadStatusText; // Assuming this is the lead status text
    m_currentScanName = newState.currentScanName; // Assuming this is the current scan name
    // Note: Don't update m_trackingEnabled here directly from newState.trackingActive.
    // m_trackingEnabled is the *command* given via setTrackingEnabled slot.
    // The actual tracking status displayed on OSD should come from newState.trackingActive
    // which will be put into FrameData inside processFrame.
    m_currentActiveCameraIsDay = newState.activeCameraIsDay;
    m_currentTrackingPhase = newState.currentTrackingPhase;
    m_currentAcquisitionBoxX_px = newState.acquisitionBoxX_px;
    m_currentAcquisitionBoxY_px = newState.acquisitionBoxY_px;
    m_currentAcquisitionBoxW_px = newState.acquisitionBoxW_px;
    m_currentAcquisitionBoxH_px = newState.acquisitionBoxH_px;

    m_currentLeadAngleStatus = newState.currentLeadAngleStatus;   
    m_currentLeadAngleOffsetAz = newState.leadAngleOffsetAz;      
    m_currentLeadAngleOffsetEl = newState.leadAngleOffsetEl;
    m_currentAmmunitionLevel = newState.stationAmmunitionLevel;
    
    m_detectionEnabled.store(newState.detectionEnabled);

}


// --- GStreamer Handling --- (No changes needed based on errors)
bool CameraVideoStreamDevice::initializeGStreamer()
{
    if (m_pipeline) {
        qWarning() << "Cam" << m_cameraIndex << ": GStreamer already initialized.";
        return true;
    }
    gst_init(nullptr, nullptr);

    QString pipelineStr = QString("v4l2src device=%1 do-timestamp=true ! "
        "video/x-raw,format=YUY2,width=%2,height=%3,framerate=30/1 ! "
        "videocrop top=%4 left= %6 bottom=%5  right=%7 ! "
        "videoscale ! "
        "video/x-raw,width=1024,height=768 ! "
        "queue max-size-buffers=1 leaky=downstream ! "
        "appsink name=mysink emit-signals=true max-buffers=1 drop=true sync=false")
        .arg(m_deviceName)
        .arg(m_sourceWidth)
        .arg(m_sourceHeight)
        .arg(m_cropTop)
        .arg(m_cropBottom)
        .arg(m_cropLeft)
        .arg(m_cropRight);

    /*QString pipelineStr = QString(
                              "v4l2src device=%1 do-timestamp=true ! "
                              "image/jpeg,width=%2,height=%3,framerate=30/1 ! jpegdec ! video/x-raw ! "
                              "aspectratiocrop aspect-ratio=4/3 ! "
                              "videoscale  ! "
                              "video/x-raw,width=1024,height=768 ! "
                              "videoconvert ! video/x-raw,format=YUY2 ! " // Explicit conversion
                              "queue max-size-buffers=2 leaky=downstream ! "
                              "appsink name=mysink emit-signals=true max-buffers=2 drop=true sync=false"
                              ).arg(m_deviceName).arg(m_sourceWidth).arg(m_sourceHeight);
*/
    qInfo() << "Cam" << m_cameraIndex << " GStreamer Pipeline:" << pipelineStr;
    GError *error = nullptr;
    m_pipeline = gst_parse_launch(pipelineStr.toUtf8().constData(), &error);
    if (!m_pipeline) {
        qCritical() << "Cam" << m_cameraIndex << ": Failed to parse GStreamer pipeline:" << (error ? error->message : "Unknown error");
        if (error) g_error_free(error);
        return false;
    }
    if (error) {
        qWarning() << "Cam" << m_cameraIndex << ": GStreamer parsing warning/error:" << error->message;
        g_error_free(error);
    }
    m_appSink = gst_bin_get_by_name(GST_BIN(m_pipeline), "mysink");
    if (!m_appSink) {
        qCritical() << "Cam" << m_cameraIndex << ": Failed to get appsink element.";
        gst_object_unref(m_pipeline); m_pipeline = nullptr; return false;
    }
    g_object_set(G_OBJECT(m_appSink), "emit-signals", TRUE, nullptr);
    /*GstAppSinkCallbacks callbacks = {
        nullptr,                                        // eos
        nullptr,                                        // new_preroll
        &CameraVideoStreamDevice::on_new_sample_from_sink, // new_sample
        nullptr,                                        // new_event (if exists)
        {nullptr, nullptr, nullptr}            // _gst_reserved array
    };*/
    GstAppSinkCallbacks callbacks = {};
    callbacks.new_sample = &CameraVideoStreamDevice::on_new_sample_from_sink;
    //GstAppSinkCallbacks callbacks = {nullptr, nullptr, &CameraVideoStreamDevice::on_new_sample_from_sink, nullptr};
    gst_app_sink_set_callbacks(GST_APP_SINK(m_appSink), &callbacks, this, nullptr);
    m_gstLoop = g_main_loop_new(nullptr, FALSE);
    if (!m_gstLoop) {
        qCritical() << "Cam" << m_cameraIndex << ": Failed to create GStreamer main loop.";
        gst_object_unref(m_pipeline); m_pipeline = nullptr; m_appSink = nullptr; return false;
    }
    return true;
}

void CameraVideoStreamDevice::cleanupGStreamer()
{
    qInfo() << "Cam" << m_cameraIndex << ": Cleaning up GStreamer...";
    if (m_gstLoop) {
        if (g_main_loop_is_running(m_gstLoop)) {
             qWarning() << "Cam" << m_cameraIndex << ": GStreamer main loop still running during cleanup!";
        }
        g_main_loop_unref(m_gstLoop); m_gstLoop = nullptr;
         qInfo() << "Cam" << m_cameraIndex << ": Unreferenced GStreamer main loop.";
    }
    if (m_pipeline) {
        gst_object_unref(m_pipeline); m_pipeline = nullptr; m_appSink = nullptr;
         qInfo() << "Cam" << m_cameraIndex << ": Unreferenced GStreamer pipeline.";
    } else {
         qDebug() << "Cam" << m_cameraIndex << ": GStreamer pipeline already null during cleanup.";
    }
}

GstFlowReturn CameraVideoStreamDevice::on_new_sample_from_sink(GstAppSink *sink, gpointer user_data)
{
    CameraVideoStreamDevice *processor = static_cast<CameraVideoStreamDevice *>(user_data);
    if (processor->m_abortRequest.load(std::memory_order_relaxed)) {
         qDebug() << "Cam" << processor->m_cameraIndex << ": Abort requested, skipping new sample.";
        return GST_FLOW_EOS;
    }
    return processor->handleNewSample(sink);
}

GstFlowReturn CameraVideoStreamDevice::handleNewSample(GstAppSink *sink)
{
    GstSample *sample = gst_app_sink_pull_sample(sink);
    if (!sample) {
        if (gst_app_sink_is_eos(sink)) {
            qInfo() << "Cam" << m_cameraIndex << ": EOS received.";
             if (m_gstLoop && g_main_loop_is_running(m_gstLoop)) g_main_loop_quit(m_gstLoop);
            return GST_FLOW_EOS;
        } else if (m_abortRequest.load()) {
             qDebug() << "Cam" << m_cameraIndex << ": Sample pull failed after abort request.";
             return GST_FLOW_EOS;
        } else {
            qWarning() << "Cam" << m_cameraIndex << ": Failed to pull sample (not EOS).";
            return GST_FLOW_ERROR;
        }
    }
    GstBuffer *buffer = gst_sample_get_buffer(sample);
    if (!buffer) {
        qWarning() << "Cam" << m_cameraIndex << ": Failed to get buffer from sample.";
        gst_sample_unref(sample); return GST_FLOW_ERROR;
    }

    bool success = false;
    try {
        success = processFrame(buffer);
    } catch (const std::exception &e) {
        qCritical() << "Cam" << m_cameraIndex << ": Exception during processFrame:" << e.what();
        emit processingError(m_cameraIndex, QString("Frame Error: %1").arg(e.what()));
        success = false;
    } catch (...) {
         qCritical() << "Cam" << m_cameraIndex << ": Unknown exception during processFrame.";
        emit processingError(m_cameraIndex, "Unknown error during frame processing.");
        success = false;
    }
    gst_sample_unref(sample);
    if (m_abortRequest.load(std::memory_order_relaxed)) {
        qDebug() << "Cam" << m_cameraIndex << ": Abort requested during frame processing.";
        return GST_FLOW_EOS;
    }
    return success ? GST_FLOW_OK : GST_FLOW_ERROR;
}


// --- VPI Handling --- (No changes needed based on errors)
bool CameraVideoStreamDevice::initializeVPI()
{
    // CUDA device check and reset before VPI initialization
    cudaError_t cudaStatus = cudaSetDevice(0);
    if (cudaStatus != cudaSuccess) {
        qWarning() << "Cam" << m_cameraIndex << ": CUDA device unavailable:"
                   << cudaGetErrorString(cudaStatus);

        // Try to reset the device
        cudaStatus = cudaDeviceReset();
        if (cudaStatus != cudaSuccess) {
            qCritical() << "Cam" << m_cameraIndex << ": Failed to reset CUDA device:"
                        << cudaGetErrorString(cudaStatus);
            return false;
        }

        // Retry setting device
        cudaStatus = cudaSetDevice(0);
        if (cudaStatus != cudaSuccess) {
            qCritical() << "Cam" << m_cameraIndex << ": CUDA device still unavailable after reset";
            return false;
        }
    }

    qInfo() << "Cam" << m_cameraIndex << ": CUDA device initialized successfully";

    try {
        CHECK_VPI_STATUS(vpiStreamCreate(0, &m_vpiStream));
        CHECK_VPI_STATUS(vpiImageCreate(m_outputWidth, m_outputHeight, VPI_IMAGE_FORMAT_NV12_ER, 0, &m_vpiFrameNV12));
        CHECK_VPI_STATUS(vpiCreateCropScaler(m_vpiBackend, 1, m_maxTrackedTargets, &m_cropScalePayload));
        VPIDCFTrackerCreationParams dcfParams;
        CHECK_VPI_STATUS(vpiInitDCFTrackerCreationParams(&dcfParams));
        m_vpiFeaturePatchSize = dcfParams.featurePatchSize;  // Store feature patch size (24)
        m_vpiTgtPatchSize = dcfParams.featurePatchSize * dcfParams.hogCellSize;  // Target patch size (96)
        CHECK_VPI_STATUS(vpiCreateDCFTracker(m_vpiBackend, 1, m_maxTrackedTargets, &dcfParams, &m_dcfPayload));
        VPIImageFormat patchFormat = (m_vpiBackend == VPI_BACKEND_PVA) ? VPI_IMAGE_FORMAT_RGB8p : VPI_IMAGE_FORMAT_RGBA8;
        CHECK_VPI_STATUS(vpiImageCreate(m_vpiTgtPatchSize, m_vpiTgtPatchSize * m_maxTrackedTargets, patchFormat, 0, &m_vpiTgtPatches));
        CHECK_VPI_STATUS(vpiArrayCreate(m_maxTrackedTargets, VPI_ARRAY_TYPE_DCF_TRACKED_BOUNDING_BOX, 0, &m_vpiInTargets));
        CHECK_VPI_STATUS(vpiArrayCreate(m_maxTrackedTargets, VPI_ARRAY_TYPE_DCF_TRACKED_BOUNDING_BOX, 0, &m_vpiOutTargets));
        // Create an array to hold the confidence scores (per-object max correlation response)
        CHECK_VPI_STATUS(vpiArrayCreate(m_maxTrackedTargets, VPI_ARRAY_TYPE_F32, 0, &m_vpiConfidenceScores));
        // Create an image to hold the full correlation response map (fallback method)
        // Correlation map size MUST be featurePatchSize (not featurePatchSize * hogCellSize)
        CHECK_VPI_STATUS(vpiImageCreate(m_vpiFeaturePatchSize, m_vpiFeaturePatchSize * m_maxTrackedTargets,
                                       VPI_IMAGE_FORMAT_F32, 0, &m_vpiCorrelationMap));
        qInfo() << "Cam" << m_cameraIndex << ": Created correlation map" << m_vpiFeaturePatchSize << "x" << (m_vpiFeaturePatchSize * m_maxTrackedTargets)
                << "(featurePatchSize=" << m_vpiFeaturePatchSize << ", tgtPatchSize=" << m_vpiTgtPatchSize << ")";
    } catch (const std::exception &e) {
        qCritical() << "Cam" << m_cameraIndex << ": VPI Initialization failed:" << e.what();
        cleanupVPI(); return false;
    }
    return true;
}

void CameraVideoStreamDevice::cleanupVPI()
{
    qInfo() << "Cam" << m_cameraIndex << ": Cleaning up VPI resources...";
     if (m_vpiStream) {
         qInfo() << "Cam" << m_cameraIndex << ": Syncing VPI stream before cleanup...";
         VPIStatus syncStatus = vpiStreamSync(m_vpiStream);
         if (syncStatus != VPI_SUCCESS) {
              qWarning() << "Cam" << m_cameraIndex << ": VPI Stream sync failed during cleanup: " << vpiStatusGetName(syncStatus);
         }
     } else {
          qDebug() << "Cam" << m_cameraIndex << ": VPI stream is null during cleanup.";
     }
    VPI_SAFE_DESTROY(vpiArrayDestroy, m_vpiInTargets);
    VPI_SAFE_DESTROY(vpiArrayDestroy, m_vpiOutTargets);
    VPI_SAFE_DESTROY(vpiImageDestroy, m_vpiTgtPatches);
    VPI_SAFE_DESTROY(vpiPayloadDestroy, m_dcfPayload);
    VPI_SAFE_DESTROY(vpiPayloadDestroy, m_cropScalePayload);
    VPI_SAFE_DESTROY(vpiImageDestroy, m_vpiFrameNV12);
    VPI_SAFE_DESTROY(vpiStreamDestroy, m_vpiStream);
    VPI_SAFE_DESTROY(vpiArrayDestroy, m_vpiConfidenceScores);
    VPI_SAFE_DESTROY(vpiImageDestroy, m_vpiCorrelationMap);

    // CUDA context cleanup
    cudaError_t cudaStatus = cudaDeviceSynchronize();
    if (cudaStatus != cudaSuccess) {
        qWarning() << "Cam" << m_cameraIndex << ": CUDA sync failed:"
                   << cudaGetErrorString(cudaStatus);
    }

    qInfo() << "Cam" << m_cameraIndex << ": Finished cleaning VPI objects.";
}


// --- Frame Processing Logic ---
// processFrame: Populate FrameData, including data.trackingBbox (should compile now)
bool CameraVideoStreamDevice::processFrame(GstBuffer *buffer)
{
    // ========================================================================
    // LATENCY MEASUREMENT: Record frame arrival timestamp
    // ========================================================================
    m_frameArrivalTime = m_latencyTimer.elapsed();

    GstMapInfo mapInfo = GST_MAP_INFO_INIT;
    VPIImage vpiImgInput_wrapped = nullptr;
    cv::Mat cvFrameBGRA;
    cv::Mat cvFrameBGR;

    try {
        // 1. Map GStreamer Buffer & Copy YUY2
        if (!gst_buffer_map(buffer, &mapInfo, GST_MAP_READ)) {
            qWarning() << "Cam" << m_cameraIndex << ": Failed to map GStreamer buffer"; return false;
        }
        size_t expected_size = static_cast<size_t>(m_outputWidth * m_outputHeight * 2);
        if (mapInfo.size < expected_size) { // Check if buffer is at least expected size
             qWarning() << "Cam" << m_cameraIndex << ": GStreamer buffer size (" << mapInfo.size
                        << ") smaller than expected YUY2 size (" << expected_size << ")!";
             gst_buffer_unmap(buffer, &mapInfo); return false;
        }
        // Ensure host buffer is allocated
        if (m_yuy2_host_buffer.empty() || m_yuy2_host_buffer.total() * m_yuy2_host_buffer.elemSize() != expected_size) {
            m_yuy2_host_buffer.create(m_outputHeight, m_outputWidth, CV_8UC2);
        }
        memcpy(m_yuy2_host_buffer.data, mapInfo.data, expected_size);
        gst_buffer_unmap(buffer, &mapInfo);

        // 2. Convert YUY2 host buffer to BGRA
        cv::cvtColor(m_yuy2_host_buffer, cvFrameBGRA, cv::COLOR_YUV2BGRA_YUY2);
        if (cvFrameBGRA.empty()) throw std::runtime_error("cv::cvtColor failed YUY2->BGRA.");

        // ====================================================================
        // ASYNC OBJECT DETECTION - Non-blocking inference
        // ====================================================================
        std::vector<YoloDetection> detections;
        bool detection_enabled = m_detectionEnabled.load(std::memory_order_relaxed);

        // Only run detection on day camera (camera 0), every 3rd frame (10Hz)
        if (detection_enabled && m_cameraIndex == 0 && (m_frameCount % 3 == 0)) {
            // Convert BGRA to BGR for YOLO (non-blocking, just queue)
            if (cvFrameBGRA.channels() == 4) {
                cv::cvtColor(cvFrameBGRA, cvFrameBGR, cv::COLOR_BGRA2BGR);
            } else if (cvFrameBGRA.channels() == 3) {
                cvFrameBGR = cvFrameBGRA;
            }

            // Launch async detection if previous one finished
            if (!cvFrameBGR.empty()) {
                QMutexLocker locker(&m_detectionMutex);
                if (!m_detectionInProgress) {
                    // Copy frame for async processing
                    m_detectionFrame = cvFrameBGR.clone();
                    m_detectionInProgress = true;
                    locker.unlock();

                    // Launch async detection (non-blocking)
                    QtConcurrent::run([this]() {
                        auto result = m_inference.runInference(m_detectionFrame);

                        QMutexLocker lock(&m_detectionMutex);
                        m_latestDetections = result;
                        m_detectionInProgress = false;
                        m_lastDetectionTime.restart();
                    });
                }
            }
        }

        // Use latest available detections (non-blocking read)
        {
            QMutexLocker locker(&m_detectionMutex);
            detections = m_latestDetections;
        }
        // --- Object Detection End (Async) ---

        // 3. Wrap BGRA Mat for VPI input
        CHECK_VPI_STATUS(vpiImageCreateWrapperOpenCVMat(cvFrameBGRA, 0, &vpiImgInput_wrapped));

        // 4. Tracking Logic (State-Driven)
        TrackingPhase currentPhase = m_currentTrackingPhase; // Use local cached copy
        bool amITheActiveCamera = (m_cameraIndex == 0) ? m_currentActiveCameraIsDay : !m_currentActiveCameraIsDay;

        // Action 1: Handle turning tracking OFF
        if (currentPhase == TrackingPhase::Off) {
            if (m_trackerInitialized) { // If phase was just switched to Off, reset our internal state
                qDebug() << "[CAM" << m_cameraIndex << "] TrackingPhase is Off, resetting local tracker state.";
                m_trackerInitialized = false;
                m_currentTarget = {}; // Zero-initialize the struct
                m_currentTarget.state = VPI_TRACKING_STATE_LOST;
            }
        }
        // Action 2: Handle tracking operations if tracking is commanded ON in any phase
        else {
            if (amITheActiveCamera) {
                switch (currentPhase) {
                    case TrackingPhase::Acquisition:
                        // In Acquisition phase, we don't initialize or run the VPI tracker.
                        // The OSD will draw the acquisition box based on m_currentAcquisitionBoxX_px etc.
                        // Ensure m_trackerInitialized is false and m_currentTarget is LOST.
                        if (m_trackerInitialized) {
                            qDebug() << "[CAM" << m_cameraIndex << "] In Acquisition, resetting local tracker state.";
                            m_trackerInitialized = false;
                            m_currentTarget = {};
                            m_currentTarget.state = VPI_TRACKING_STATE_LOST;
                        }
                        break;

                    case TrackingPhase::Tracking_LockPending:
                        // Is this the very first frame after receiving a Lock-On command?
                        if (!m_trackerInitialized) {
                            qDebug() << "[CAM" << m_cameraIndex << "] Initializing tracker with acquisition box...";
                            if (initializeFirstTarget(vpiImgInput_wrapped,
                                                    m_currentAcquisitionBoxX_px, m_currentAcquisitionBoxY_px,
                                                    m_currentAcquisitionBoxW_px, m_currentAcquisitionBoxH_px))
                            {
                                m_trackerInitialized = true;
                            } else {
                                qWarning() << "[CAM" << m_cameraIndex << "] Tracker init failed. Reporting failure to model.";
                                // Report failure to model so it can transition back to Off
                                m_stateModel->updateTrackingResult(m_cameraIndex, false, 0,0,0,0,0,0, VPI_TRACKING_STATE_LOST, 0.0f);
                            }
                        }
                        // Fall through to runTrackingCycle if initialized (or just initialized)
                        // This allows the tracker to immediately try to localize after initialization
                        // and report its state (NEW, then hopefully TRACKED/LOST)
                        if (m_trackerInitialized) {
                            if (!runTrackingCycle(vpiImgInput_wrapped)) {
                                qWarning() << "Cam" << m_cameraIndex << ": Tracking cycle failed or target lost during LockPending.";
                                // m_currentTarget.state is updated to VPI_TRACKING_STATE_LOST inside runTrackingCycle.
                                // This "lost" state will be reported to the model below.
                            }
                        }
                        break;

                    case TrackingPhase::Tracking_ActiveLock:
                    case TrackingPhase::Tracking_Coast:
                        // If we are initialized, we must run the tracking cycle to localize the target on the new frame.
                        if (m_trackerInitialized) {
                            if (!runTrackingCycle(vpiImgInput_wrapped)) {
                                qWarning() << "Cam" << m_cameraIndex << ": Tracking cycle failed or target lost during ActiveLock/Coast.";
                                // m_currentTarget.state is updated to VPI_TRACKING_STATE_LOST inside runTrackingCycle.
                                // This "lost" state will be reported to the model below.
                            }
                        } else {
                            // Anomaly: In ActiveLock/Coast but tracker not initialized. Force reset.
                            qWarning() << "[CAM" << m_cameraIndex << "] Anomaly: In ActiveLock/Coast but tracker not initialized. Resetting.";
                            m_currentTarget = {};
                            m_currentTarget.state = VPI_TRACKING_STATE_LOST;
                            // Inform model of lost state so it can transition to Off
                            m_stateModel->updateTrackingResult(m_cameraIndex, false, 0,0,0,0,0,0, VPI_TRACKING_STATE_LOST, 0.0f);
                        }
                        break;

                    case TrackingPhase::Tracking_Firing:
                        // In Firing phase, the system holds position. Tracking updates might still come in,
                        // but the tracker should continue to run to maintain its internal state.
                        // However, the model's phase transition logic for Firing is external.
                        if (m_trackerInitialized) {
                            if (!runTrackingCycle(vpiImgInput_wrapped)) {
                                qWarning() << "Cam" << m_cameraIndex << ": Tracking cycle failed or target lost during Firing.";
                            }
                        } else {
                            qWarning() << "[CAM" << m_cameraIndex << "] Anomaly: In Firing but tracker not initialized. Resetting.";
                            m_currentTarget = {};
                            m_currentTarget.state = VPI_TRACKING_STATE_LOST;
                            m_stateModel->updateTrackingResult(m_cameraIndex, false, 0,0,0,0,0,0, VPI_TRACKING_STATE_LOST, 0.0f);
                        }
                        break;

                    default:
                        // Handle any other unexpected phases, perhaps reset tracker
                        if (m_trackerInitialized) {
                            qWarning() << "[CAM" << m_cameraIndex << "] Unexpected TrackingPhase: " << static_cast<int>(currentPhase) << ". Resetting tracker.";
                            m_trackerInitialized = false;
                            m_currentTarget = {};
                            m_currentTarget.state = VPI_TRACKING_STATE_LOST;
                        }
                        break;
                }
            } else { // If I am NOT the active camera, but tracking is on in the system
                if (m_trackerInitialized) { // Ensure my own (inactive) tracker is reset
                     qDebug() << "[CAM" << m_cameraIndex << "] I am INACTIVE, resetting local tracker state.";
                     m_trackerInitialized = false;
                     m_currentTarget = {};
                     m_currentTarget.state = VPI_TRACKING_STATE_LOST;
                }
            }
        }

        // This part is perfect and should be kept exactly as it is.
        // It correctly handles all cases and passes all data to the model.
        if (m_stateModel) {
            bool trackerIsValidThisFrame = (m_trackerInitialized && m_currentTarget.state == VPI_TRACKING_STATE_TRACKED);
            float cX_px = 0.0f, cY_px = 0.0f, tW_px = 0.0f, tH_px = 0.0f;
            float velX_px_s = 0.0f, velY_px_s = 0.0f;

            if (trackerIsValidThisFrame) {
                cX_px = m_currentTarget.bbox.left + m_currentTarget.bbox.width / 2.0f;
                cY_px = m_currentTarget.bbox.top + m_currentTarget.bbox.height / 2.0f;
                tW_px = static_cast<float>(m_currentTarget.bbox.width);
                tH_px = static_cast<float>(m_currentTarget.bbox.height);

                qint64 ms_elapsed = m_velocityTimer.restart();
                double dt_s = ms_elapsed / 1000.0;
                if (dt_s > 1e-6 && m_lastTargetCenterX_px > 0) {
                    velX_px_s = (cX_px - m_lastTargetCenterX_px) / dt_s;
                    velY_px_s = (cY_px - m_lastTargetCenterY_px) / dt_s;
                }
                m_lastTargetCenterX_px = cX_px;
                m_lastTargetCenterY_px = cY_px;
            } else {
                m_lastTargetCenterX_px = 0.0f;
                m_lastTargetCenterY_px = 0.0f;
            }
            //qDebug() << "[CAM" << m_cameraIndex << " | processFrame] Reporting to model. trackerIsValidThisFrame:" << trackerIsValidThisFrame   << "m_currentTarget.state:" << static_cast<int>(m_currentTarget.state);
            // Call the model's update method (using the new name if you changed it)
            // Use smoothed confidence for stable OSD display (reduces jitter)
            m_stateModel->updateTrackingResult(m_cameraIndex, trackerIsValidThisFrame,
                                               cX_px, cY_px, tW_px, tH_px,
                                               velX_px_s, velY_px_s, m_currentTarget.state,
                                               m_smoothedConfidence);
        }
         // --- END OF SystemStateModel UPDATE ---

        // 5. Sync VPI
        CHECK_VPI_STATUS(vpiStreamSync(m_vpiStream));

        // 6. Prepare FrameData
        FrameData data;
        data.cameraIndex = m_cameraIndex;
        data.baseImage = cvMatToQImage(cvFrameBGRA);
        if (data.baseImage.isNull()) qWarning() << "Cam" << m_cameraIndex << ": Failed convert cv::Mat to QImage";

        //data.trackingEnabled = tracking_this_frame;
        data.trackerInitialized = m_trackerInitialized;
        data.trackingState = m_currentTarget.state; // VPITrackingState
        data.trackingConfidence = m_smoothedConfidence;  // Smoothed tracking confidence for stable display (0.0-1.0)

        // >>> *** Convert VPIRectI (m_currentTarget.bbox) to QRect (data.trackingBbox) ***
        data.trackingBbox = QRect(m_currentTarget.bbox.left,
                                                            m_currentTarget.bbox.top,
                                                            m_currentTarget.bbox.width,
                                                            m_currentTarget.bbox.height);
        data.cameraFOV = m_cameraFOV;
        data.currentOpMode = m_currentMode;
        data.motionMode = m_motionMode;
        data.stabEnabled = m_stabEnabled;
        data.azimuth = m_currentAzimuth;
        data.elevation = m_currentElevation;
        data.imuConnected = m_imuConnected;
        data.imuRollDeg = m_imuRollDeg;
        data.imuPitchDeg = m_imuPitchDeg;
        data.imuYawDeg = m_imuYawDeg;           // Vehicle heading for azimuth calculation
        data.imuTemp = m_imuTemp;
        data.gyroX = m_gyroX;
        data.gyroY = m_gyroY;
        data.gyroZ = m_gyroZ;
        data.accelX = m_accelX;
        data.accelY = m_accelY;
        data.accelZ = m_accelZ;

        data.speed = m_speed;
        data.lrfDistance = m_lrfDistance;
        data.sysCharged = m_sysCharged;
        data.gunArmed = m_sysArmed;
        data.sysReady = m_sysReady;
        data.fireMode = m_fireMode;
        data.reticleType = m_reticleType;
        data.colorStyle = m_colorStyle;
        data.detectionEnabled = detection_enabled;
        data.detections = detections;
        data.zeroingModeActive = m_currentZeroingModeActive;
        data.zeroingAppliedToBallistics = m_currentZeroingApplied;
        data.zeroingAzimuthOffset = m_currentZeroingAzOffset;
        data.zeroingElevationOffset = m_currentZeroingElOffset;

        data.windageModeActive = m_currentWindageModeActive;
        data.windageAppliedToBallistics = m_currentWindageApplied;
        data.windageSpeedKnots = m_currentWindageSpeed;
        data.windageDirectionDegrees = m_currentWindageDirection;
        data.calculatedCrosswindMS = m_currentCalculatedCrosswind;
        data.isReticleInNoFireZone = m_currentIsReticleInNoFireZone;
        data.gimbalStoppedAtNTZLimit = m_currentGimbalStoppedAtNTZLimit;
        data.leadAngleActive = m_isLacActiveForReticle; // Assuming this is the LAC status
        data.reticleAimpointImageX_px = m_currentReticleAimpointImageX_px; // Reticle: gun boresight with zeroing ONLY
        data.reticleAimpointImageY_px = m_currentReticleAimpointImageY_px; // Reticle: gun boresight with zeroing ONLY
        data.ccipImpactImageX_px = m_currentCcipImpactImageX_px; // ✅ CCIP: bullet impact with zeroing + lead
        data.ccipImpactImageY_px = m_currentCcipImpactImageY_px; // ✅ CCIP: bullet impact with zeroing + lead
        data.leadStatusText = m_currentLeadStatusText; // Assuming this is the lead status text
        data.currentScanName = m_currentScanName; // Assuming this is the current scan name
        data.currentTrackingPhase = m_currentTrackingPhase;
        data.acquisitionBoxX_px = m_currentAcquisitionBoxX_px;
        data.acquisitionBoxY_px = m_currentAcquisitionBoxY_px ;
        data.acquisitionBoxW_px = m_currentAcquisitionBoxW_px  ;
        data.acquisitionBoxH_px = m_currentAcquisitionBoxH_px  ;
        data.trackerHasValidTarget = true;
        data.leadAngleStatus = m_currentLeadAngleStatus;          
        data.leadAngleOffsetAz_deg = m_currentLeadAngleOffsetAz;
        data.leadAngleOffsetEl_deg = m_currentLeadAngleOffsetEl;
        data.stationAmmunitionLevel = m_currentAmmunitionLevel;

        // ====================================================================
        // LATENCY MEASUREMENT: Calculate glass-to-glass latency
        // ====================================================================
        qint64 frameCompleteTime = m_latencyTimer.elapsed();
        qint64 frameLatency_ms = frameCompleteTime - m_frameArrivalTime;
        m_totalLatency_ms += frameLatency_ms;
        m_frameProcessedCount++;

        // Report every 30 frames (once per second at 30fps)
        if (m_frameProcessedCount % 30 == 0) {
            qint64 avgLatency = m_totalLatency_ms / m_frameProcessedCount;
            qInfo() << "🎯 [CAM" << m_cameraIndex << "] Glass-to-Glass Latency:"
                    << "Current=" << frameLatency_ms << "ms"
                    << "| Average=" << avgLatency << "ms"
                    << "| Frames=" << m_frameProcessedCount;
        }

        // 7. Emit FrameData
        if (!data.baseImage.isNull()) emit frameDataReady(data);

    } catch (const std::exception &e) {
        qCritical() << "Cam" << m_cameraIndex << ": Exception in processFrame loop:" << e.what();
        emit processingError(m_cameraIndex, QString("Frame Loop Error: %1").arg(e.what()));
        VPI_SAFE_DESTROY(vpiImageDestroy, vpiImgInput_wrapped); return false;
    }

    // 8. Destroy Wrapper
    VPI_SAFE_DESTROY(vpiImageDestroy, vpiImgInput_wrapped);
    return true;
}

bool CameraVideoStreamDevice::initializeFirstTarget(VPIImage vpiFrameInput, float boxX, float boxY, float boxW, float boxH)
{
    qInfo() << "Cam" << m_cameraIndex << ": Initializing first tracker target with BBox at"
            << boxX << "," << boxY << "Size" << boxW << "x" << boxH;
    try {
        // We now use the passed-in box parameters directly
        VPIArrayData targetsData;
        CHECK_VPI_STATUS(vpiArrayLockData(m_vpiInTargets, VPI_LOCK_WRITE, VPI_ARRAY_BUFFER_HOST_AOS, &targetsData));
        if (targetsData.buffer.aos.capacity < 1) {
             qCritical() << "Cam" << m_cameraIndex << ": VPI target array capacity is zero!";
             vpiArrayUnlock(m_vpiInTargets); return false;
        }
        auto *pTarget = static_cast<VPIDCFTrackedBoundingBox *>(targetsData.buffer.aos.data);
        pTarget->bbox.left   = static_cast<int32_t>(boxX);
        pTarget->bbox.top    = static_cast<int32_t>(boxY);
        pTarget->bbox.width  = static_cast<int32_t>(boxW);
        pTarget->bbox.height = static_cast<int32_t>(boxH);
        pTarget->state       = VPI_TRACKING_STATE_NEW;
        pTarget->seqIndex    = 0;
        pTarget->filterLR    = 0.075f;
        pTarget->filterChannelWeightsLR = 0.1f;
        pTarget->userData    = nullptr;
        m_currentTarget = *pTarget;
        *targetsData.buffer.aos.sizePointer = 1;
        CHECK_VPI_STATUS(vpiArrayUnlock(m_vpiInTargets));

        CHECK_VPI_STATUS(vpiSubmitConvertImageFormat(m_vpiStream, VPI_BACKEND_CUDA, vpiFrameInput, m_vpiFrameNV12, nullptr));
        CHECK_VPI_STATUS(vpiSubmitCropScalerBatch(m_vpiStream, 0, m_cropScalePayload, &m_vpiFrameNV12,
                                                  1, m_vpiInTargets, m_vpiTgtPatchSize, m_vpiTgtPatchSize, m_vpiTgtPatches));
        CHECK_VPI_STATUS(vpiSubmitDCFTrackerUpdateBatch(m_vpiStream, m_vpiBackend, m_dcfPayload, nullptr, 0,
                                                        nullptr, nullptr, m_vpiTgtPatches, m_vpiInTargets, nullptr));
        CHECK_VPI_STATUS(vpiStreamSync(m_vpiStream));
    } catch (const std::exception &e) {
        qCritical() << "Cam" << m_cameraIndex << ": Failed init first target:" << e.what();
        m_currentTarget.state = VPI_TRACKING_STATE_LOST;
        m_trackerInitialized = false;
        return false;
    }
    return true;
}

// runTrackingCycle() method (No changes needed based on errors)
bool CameraVideoStreamDevice::runTrackingCycle(VPIImage vpiFrameInput)
{
    //const float CONFIDENCE_THRESHOLD_LOW = 0.25f; // Example: If score drops below this, enter shadow mode.
    //const float CONFIDENCE_THRESHOLD_HIGH = 0.40f; // Example: If score rises above this, re-acquire lock.

    try {
        CHECK_VPI_STATUS(vpiSubmitConvertImageFormat(m_vpiStream, VPI_BACKEND_CUDA, vpiFrameInput, m_vpiFrameNV12, nullptr));
        CHECK_VPI_STATUS(vpiSubmitCropScalerBatch(m_vpiStream, 0, m_cropScalePayload, &m_vpiFrameNV12,
                                                  1, m_vpiInTargets, m_vpiTgtPatchSize, m_vpiTgtPatchSize, m_vpiTgtPatches));
        // Request BOTH correlation map AND max correlation scores for fallback
        CHECK_VPI_STATUS(vpiSubmitDCFTrackerLocalizeBatch(m_vpiStream, m_vpiBackend, m_dcfPayload, NULL, 0,
                                                          NULL, m_vpiTgtPatches, m_vpiInTargets, m_vpiOutTargets,
                                                          m_vpiCorrelationMap, m_vpiConfidenceScores, NULL));
        CHECK_VPI_STATUS(vpiStreamSync(m_vpiStream));

        VPIArrayData outTargetsData;
        VPIArrayData confidenceData;
        CHECK_VPI_STATUS(vpiArrayLockData(m_vpiOutTargets, VPI_LOCK_READ, VPI_ARRAY_BUFFER_HOST_AOS, &outTargetsData));
        CHECK_VPI_STATUS(vpiArrayLockData(m_vpiConfidenceScores, VPI_LOCK_READ, VPI_ARRAY_BUFFER_HOST_AOS, &confidenceData));
        
        //qDebug() << "[CAM" << m_cameraIndex << "] runTrackingCycle: After localize, outTargetsData size:" << *outTargetsData.buffer.aos.sizePointer;

        bool target_found = false;
        if (*outTargetsData.buffer.aos.sizePointer > 0) {
            VPIDCFTrackedBoundingBox *tempTarget = static_cast<VPIDCFTrackedBoundingBox *>(outTargetsData.buffer.aos.data);

            // Read confidence if available from VPI
            float currentConfidence = 0.0f;
            if (*confidenceData.buffer.aos.sizePointer > 0) {
                // METHOD 1: VPI provided per-object max correlation scores
                currentConfidence = static_cast<float*>(confidenceData.buffer.aos.data)[0];
                qDebug() << "[CAM" << m_cameraIndex << "] VPI provided confidence (method 1):" << currentConfidence;
            } else {
                // METHOD 2: Try reading from full correlation response map
                VPIImageData correlationImgData;
                VPIStatus lockStatus = vpiImageLockData(m_vpiCorrelationMap, VPI_LOCK_READ, VPI_IMAGE_BUFFER_HOST_PITCH_LINEAR, &correlationImgData);

                if (lockStatus == VPI_SUCCESS && correlationImgData.buffer.pitch.planes[0].data != nullptr) {
                    // Successfully locked correlation map, find maximum value
                    int width = correlationImgData.buffer.pitch.planes[0].width;
                    int height = correlationImgData.buffer.pitch.planes[0].height;
                    int32_t pitchBytes = correlationImgData.buffer.pitch.planes[0].pitchBytes;
                    const uint8_t *basePtr = static_cast<const uint8_t*>(correlationImgData.buffer.pitch.planes[0].data);

                    float maxCorr = -1e9f;
                    for (int y = 0; y < height; ++y) {
                        const float *rowPtr = reinterpret_cast<const float*>(basePtr + y * pitchBytes);
                        for (int x = 0; x < width; ++x) {
                            if (rowPtr[x] > maxCorr) {
                                maxCorr = rowPtr[x];
                            }
                        }
                    }
                    // Normalize DCF correlation response to 0-1 range
                    // DCF correlation scores typically range from ~0 to ~8
                    // Map: 0→0.0, 4→0.5, 8→1.0, >8→1.0 (clamped)
                    currentConfidence = std::min(1.0f, std::max(0.0f, maxCorr / 8.0f));
                    qDebug() << "[CAM" << m_cameraIndex << "] VPI correlation map confidence (method 2): raw=" << maxCorr
                             << "normalized=" << currentConfidence << "(" << (int)(currentConfidence * 100) << "%) map size:" << width << "x" << height;
                    vpiImageUnlock(m_vpiCorrelationMap);
                } else {
                    // METHOD 3: VPI didn't populate confidence array or correlation map - estimate from tracking state
                    // This is a fallback for VPI backends that don't support confidence scores
                    //
                    // ACTUAL VPI Tracking State Enum (verified from runtime behavior):
                    // 0 = VPI_TRACKING_STATE_NEW (initialization)
                    // 1 = VPI_TRACKING_STATE_LOST (object lost) ← SWAPPED
                    // 2 = VPI_TRACKING_STATE_TRACKED (object successfully found) ← SWAPPED - MOST COMMON
                    // 3 = VPI_TRACKING_STATE_SHADOW_TRACKED (tracked using prediction, partially obscured)
                    // NOTE: States 1 and 2 are opposite of some VPI documentation!
                    switch (tempTarget->state) {
                        case 0:  // VPI_TRACKING_STATE_NEW
                            currentConfidence = 0.50f;  // Medium confidence - new target, needs validation
                            break;
                        case 1:  // VPI_TRACKING_STATE_LOST (verified: means LOST, not TRACKED)
                            currentConfidence = 0.0f;   // No confidence - tracking lost
                            break;
                        case 2:  // VPI_TRACKING_STATE_TRACKED (verified: means TRACKED, not LOST)
                            currentConfidence = 0.90f;  // High confidence - actively tracked with visual confirmation
                            break;
                        case 3:  // VPI_TRACKING_STATE_SHADOW_TRACKED
                            currentConfidence = 0.65f;  // Medium-high confidence - using prediction/coasting
                            break;
                        default:
                            currentConfidence = 0.30f;  // Low confidence - unknown state
                            qWarning() << "[CAM" << m_cameraIndex << "] Unknown VPI tracking state:" << tempTarget->state;
                            break;
                    }
                    static bool warningShown = false;
                    if (!warningShown) {
                        qWarning() << "[CAM" << m_cameraIndex << "] VPI confidence unavailable (methods 1 & 2 failed) - using state-based estimation (method 3)";
                        warningShown = true;  // Only warn once
                    }
                }
            }

            // Apply exponential moving average filter for smooth OSD display
            // alpha = 0.2 means ~5 frames to reach 90% of new value (reduces jitter)
            const float alpha = 0.2f;
            if (m_smoothedConfidence == 0.0f) {
                // First frame, initialize with current value
                m_smoothedConfidence = currentConfidence;
            } else {
                // Smooth updates: smoothed = alpha * new + (1-alpha) * old
                m_smoothedConfidence = alpha * currentConfidence + (1.0f - alpha) * m_smoothedConfidence;
            }

            qDebug() << "[CAM" << m_cameraIndex << "] VPI Localize Result: State=" << tempTarget->state
                     << "Confidence=" << currentConfidence << "Smoothed=" << m_smoothedConfidence
                     << "(VPI array size=" << *confidenceData.buffer.aos.sizePointer << ")";

            // Store confidence scores for later use
            m_currentConfidence = currentConfidence;

            // IMPORTANT: Copy the data to m_currentTarget
            m_currentTarget = *tempTarget;

            //qDebug() << "[CAM" << m_cameraIndex << "] runTrackingCycle: Localized BBox: L=" << m_currentTarget.bbox.left
             //        << ", T=" << m_currentTarget.bbox.top
             //        << ", W=" << m_currentTarget.bbox.width
             //        << ", H=" << m_currentTarget.bbox.height
             //        << ", State=" << static_cast<int>(m_currentTarget.state);

            if (m_currentTarget.state == VPI_TRACKING_STATE_LOST ||
                m_currentTarget.bbox.left < 0 || m_currentTarget.bbox.top < 0 ||
                m_currentTarget.bbox.width <= 0 || m_currentTarget.bbox.height <= 0 ||
                m_currentTarget.bbox.left + m_currentTarget.bbox.width > m_outputWidth ||
                m_currentTarget.bbox.top + m_currentTarget.bbox.height > m_outputHeight) {
                 qInfo() << "Cam" << m_cameraIndex << ": Target lost or invalid box after localize. State=" << m_currentTarget.state;
                 m_currentTarget.state = VPI_TRACKING_STATE_LOST; // Ensure state is LOST if box is invalid
                 target_found = false;
            } else {
                target_found = true;
            }
        } else {
             qWarning() << "Cam" << m_cameraIndex << ": Output target array empty after localize.";
             m_currentTarget.state = VPI_TRACKING_STATE_LOST; target_found = false;
        }
        CHECK_VPI_STATUS(vpiArrayUnlock(m_vpiOutTargets));
                CHECK_VPI_STATUS(vpiArrayUnlock(m_vpiConfidenceScores));

        if (target_found) {
            // If target is found, update the tracker's internal model for the next frame
            // We need to copy the *current* tracked target from m_currentTarget back to m_vpiInTargets
            // for the update batch, and then swap for the next localize cycle.
            VPIArrayData inTargetsData;
            CHECK_VPI_STATUS(vpiArrayLockData(m_vpiInTargets, VPI_LOCK_WRITE, VPI_ARRAY_BUFFER_HOST_AOS, &inTargetsData));
            if (inTargetsData.buffer.aos.capacity < 1) {
                qCritical() << "Cam" << m_cameraIndex << ": VPI inTargets array capacity is zero for update!";
                vpiArrayUnlock(m_vpiInTargets); return false;
            }
            *static_cast<VPIDCFTrackedBoundingBox *>(inTargetsData.buffer.aos.data) = m_currentTarget; // Copy current tracked box back
            *inTargetsData.buffer.aos.sizePointer = 1;
            CHECK_VPI_STATUS(vpiArrayUnlock(m_vpiInTargets));

            CHECK_VPI_STATUS(vpiSubmitDCFTrackerUpdateBatch(m_vpiStream, m_vpiBackend, m_dcfPayload, nullptr, 0,
                                                            nullptr, nullptr, m_vpiTgtPatches, m_vpiInTargets, nullptr));
            CHECK_VPI_STATUS(vpiStreamSync(m_vpiStream)); // Sync after update

            //qDebug() << "[CAM" << m_cameraIndex << "] runTrackingCycle: After update batch, m_currentTarget state:" << static_cast<int>(m_currentTarget.state);

        } else {
            // If target not found, ensure m_vpiInTargets is reset for the next cycle
            VPIArrayData inTargetsData;
            CHECK_VPI_STATUS(vpiArrayLockData(m_vpiInTargets, VPI_LOCK_WRITE, VPI_ARRAY_BUFFER_HOST_AOS, &inTargetsData));
            *inTargetsData.buffer.aos.sizePointer = 0; // Clear the array
            CHECK_VPI_STATUS(vpiArrayUnlock(m_vpiInTargets));
            //qDebug() << "[CAM" << m_cameraIndex << "] runTrackingCycle: Target not found, resetting inTargets.";
        }

        // The swap should still be fine, as m_vpiInTargets now holds the data for the *next* localize cycle.
        // std::swap(m_vpiInTargets, m_vpiOutTargets); // This swap is implicitly handled by the logic above if m_vpiInTargets is updated directly.
        // If m_vpiInTargets and m_vpiOutTargets are distinct buffers used in a ping-pong fashion,
        // the current setup where m_vpiInTargets is explicitly updated with m_currentTarget
        // and then used for the update batch, and then becomes the input for the next localize
        // is correct. The swap is not strictly necessary if you're always writing to m_vpiInTargets
        // for the next cycle's input.

    } catch (const std::exception &e) {
        qCritical() << "Cam" << m_cameraIndex << ": Exception during tracking cycle:" << e.what();
         m_currentTarget.state = VPI_TRACKING_STATE_LOST; return false;
    }
    return true;
}


// --- Helper Functions --- (No changes needed based on errors)
QImage CameraVideoStreamDevice::cvMatToQImage(const cv::Mat &mat)
{
    if (mat.empty()) return QImage();

    switch (mat.type()) {
    case CV_8UC4: { // 4-channel BGRA (OpenCV default ordering: B,G,R,A)
        // QImage::Format_ARGB32 is stored in memory as BGRA on little-endian systems,
        // so this mapping is correct and avoids expensive per-pixel conversions.
        QImage img(mat.data, mat.cols, mat.rows, static_cast<int>(mat.step),
                   QImage::Format_ARGB32);
        return img.copy(); // deep copy because mat.data will be freed/unmapped soon
    }

    case CV_8UC3: { // 3-channel BGR
        // QImage::Format_RGB888 expects R,G,B order. OpenCV is B,G,R -> use rgbSwapped().
        QImage img(mat.data, mat.cols, mat.rows, static_cast<int>(mat.step),
                   QImage::Format_RGB888);
        QImage swapped = img.rgbSwapped(); // Creates the correct RGB ordering
        return swapped.copy();
    }

    case CV_8UC1: { // Grayscale
        QImage img(mat.data, mat.cols, mat.rows, static_cast<int>(mat.step),
                   QImage::Format_Grayscale8);
        return img.copy();
    }

    default:
        qWarning() << "cvMatToQImage: Unsupported cv::Mat type:" << mat.type();
        return QImage();
    }
}
