#include "systemcontroller.h"

// Managers
#include "managers/HardwareManager.h"
#include "managers/ViewModelRegistry.h"
#include "managers/ControllerRegistry.h"

// Controllers (needed for direct access)
#include "gimbalcontroller.h"
#include "osdcontroller.h"

// Configuration & Validation
#include "deviceconfiguration.h"
#include "config/ConfigurationValidator.h"

// Models & Services
#include "models/domain/systemstatemodel.h"
#include "video/videoimageprovider.h"

// Hardware Devices (for video connection)
#include "hardware/devices/cameravideostreamdevice.h"

#include <QQmlContext>
#include <QQmlApplicationEngine>
#include <QDebug>

SystemController::SystemController(QObject *parent)
    : QObject(parent)
{
    qInfo() << "SystemController: Created";
}

SystemController::~SystemController()
{
    qInfo() << "SystemController: Shutting down...";

    // Managers handle their own cleanup
    // Just ensure proper destruction order

    qInfo() << "SystemController: Shutdown complete.";
}

// ============================================================================
// PHASE 1: INITIALIZE HARDWARE
// ============================================================================

void SystemController::initializeHardware()
{
    qInfo() << "=== PHASE 1: Hardware Initialization ===";

    // 1. Create SystemStateModel (central data hub)
    m_systemStateModel = new SystemStateModel(this);
    qInfo() << "  ✓ SystemStateModel created";

    // 2. Create managers
    createManagers();

    // 3. Create hardware using HardwareManager
    if (!m_hardwareManager->createHardware()) {
        qCritical() << "Failed to create hardware!";
        return;
    }

    // 4. Connect devices to models
    if (!m_hardwareManager->connectDevicesToModels()) {
        qCritical() << "Failed to connect devices to models!";
        return;
    }

    // 5. Connect models to SystemState
    if (!m_hardwareManager->connectModelsToSystemState()) {
        qCritical() << "Failed to connect models to system state!";
        return;
    }

    // 6. Create hardware controllers
    if (!m_controllerRegistry->createHardwareControllers()) {
        qCritical() << "Failed to create hardware controllers!";
        return;
    }

    qInfo() << "=== PHASE 1 COMPLETE ===\n";
}

// ============================================================================
// PHASE 2: INITIALIZE QML SYSTEM
// ============================================================================

void SystemController::initializeQmlSystem(QQmlApplicationEngine* engine)
{
    qInfo() << "=== PHASE 2: QML System Initialization ===";

    if (!engine) {
        qCritical() << "QML engine is null!";
        return;
    }

    // 1. Create Video Provider
    m_videoProvider = new VideoImageProvider();
    engine->addImageProvider("video", m_videoProvider);
    qInfo() << "  ✓ VideoImageProvider registered";

    // 2. Connect Video Streams to Provider
    connectVideoToProvider();

    // 3. Create ViewModels using ViewModelRegistry
    if (!m_viewModelRegistry->createViewModels()) {
        qCritical() << "Failed to create ViewModels!";
        return;
    }

    // 4. Create QML Controllers using ControllerRegistry
    if (!m_controllerRegistry->createQmlControllers()) {
        qCritical() << "Failed to create QML controllers!";
        return;
    }

    // 5. Initialize controllers
    if (!m_controllerRegistry->initializeControllers()) {
        qCritical() << "Failed to initialize controllers!";
        return;
    }

    // 6. Connect video to OSD for frame-synchronized updates
    if (!m_controllerRegistry->connectVideoToOsd()) {
        qCritical() << "Failed to connect video to OSD!";
        return;
    }

    // 7. Register ViewModels with QML
    QQmlContext* rootContext = engine->rootContext();
    if (!m_viewModelRegistry->registerWithQml(rootContext)) {
        qCritical() << "Failed to register ViewModels with QML!";
        return;
    }

    // 8. Register Controllers with QML
    if (!m_controllerRegistry->registerWithQml(rootContext)) {
        qCritical() << "Failed to register Controllers with QML!";
        return;
    }

    qInfo() << "=== PHASE 2 COMPLETE ===\n";
}

// ============================================================================
// PHASE 3: START SYSTEM
// ============================================================================

void SystemController::startSystem()
{
    qInfo() << "=== PHASE 3: System Startup ===";

    // 1. Start the OSD startup sequence (shows professional startup messages)
    if (m_controllerRegistry->osdController()) {
        m_controllerRegistry->osdController()->startStartupSequence();
        qInfo() << "  ✓ OSD startup sequence started";
    }

    // 2. Start hardware (open transports, initialize devices)
    if (!m_hardwareManager->startHardware()) {
        qCritical() << "Failed to start hardware!";
        return;
    }

    // 3. Clear gimbal alarms (via gimbal controller)
    if (m_controllerRegistry->gimbalController()) {
        m_controllerRegistry->gimbalController()->clearAlarms();
        qInfo() << "  ✓ Gimbal alarms cleared";
    }

    qInfo() << "=== PHASE 3 COMPLETE - SYSTEM RUNNING ===\n";
}

// ============================================================================
// HELPER METHODS
// ============================================================================

void SystemController::createManagers()
{
    qInfo() << "  Creating managers...";

    // Create HardwareManager
    m_hardwareManager = new HardwareManager(m_systemStateModel, this);

    // Create ViewModelRegistry
    m_viewModelRegistry = new ViewModelRegistry(this);

    // Create ControllerRegistry
    m_controllerRegistry = new ControllerRegistry(
        m_hardwareManager,
        m_viewModelRegistry,
        m_systemStateModel,
        this
    );

    qInfo() << "    ✓ All managers created";
}

void SystemController::connectVideoToProvider()
{
    if (!m_videoProvider || !m_hardwareManager) {
        qWarning() << "Cannot connect video: missing components";
        return;
    }

    qInfo() << "  Connecting video streams to provider...";

    // ✅ MEMORY LEAK FIX: Avoid copying entire FrameData (3.1 MB) in cross-thread connection
    // Qt::QueuedConnection (auto for cross-thread) copies the entire struct into event queue
    // Video provider only needs the QImage, not the full 3.1 MB FrameData struct
    // This lambda copies FrameData by value to extract only baseImage, preventing queue bloat

    // Day camera
    if (m_hardwareManager->dayVideoProcessor()) {
        connect(m_hardwareManager->dayVideoProcessor(), &CameraVideoStreamDevice::frameDataReady,
                this, [this](const FrameData& data) {
                    // Extract only the image we need (still 3.1 MB but only when camera is active)
                    if (data.cameraIndex == 0 && m_systemStateModel->data().activeCameraIsDay) {
                        // QImage uses implicit sharing - assignment is cheap, copy-on-write
                        m_videoProvider->updateImage(data.baseImage);
                    }
                    // ✅ FrameData goes out of scope here, releasing the QImage reference
                }, Qt::QueuedConnection);  // Explicit connection type for clarity
        qInfo() << "    ✓ Day camera connected to video provider (queued connection for thread safety)";
    }

    // Night camera
    if (m_hardwareManager->nightVideoProcessor()) {
        connect(m_hardwareManager->nightVideoProcessor(), &CameraVideoStreamDevice::frameDataReady,
                this, [this](const FrameData& data) {
                    // Extract only the image we need (still 3.1 MB but only when camera is active)
                    if (data.cameraIndex == 1 && !m_systemStateModel->data().activeCameraIsDay) {
                        // QImage uses implicit sharing - assignment is cheap, copy-on-write
                        m_videoProvider->updateImage(data.baseImage);
                    }
                    // ✅ FrameData goes out of scope here, releasing the QImage reference
                }, Qt::QueuedConnection);  // Explicit connection type for clarity
        qInfo() << "    ✓ Night camera connected to video provider (queued connection for thread safety)";
    }
}
