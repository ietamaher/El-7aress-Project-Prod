#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QDir>
#include <QFileInfo>
#include "controllers/systemcontroller.h"
#include "controllers/deviceconfiguration.h"
#include "config/MotionTuningConfig.h"
#include "config/ConfigurationValidator.h"
#include <gst/gst.h>


int main(int argc, char *argv[])
{
    // ========================================================================
    // JETSON ORIN AGX PERFORMANCE OPTIMIZATIONS
    // ========================================================================
    // Use RHI (Rendering Hardware Interface) with OpenGL ES for Jetson
    qputenv("QT_QUICK_BACKEND", "rhi");
    qputenv("QSG_RHI_BACKEND", "gles2");

    // Enable QML compiler optimizations
    qputenv("QML_DISABLE_DISK_CACHE", "0");

    // Use basic render loop (safer than threaded for this embedded application)
    qputenv("QSG_RENDER_LOOP", "basic");

    QGuiApplication app(argc, argv);
    gst_init(&argc, &argv);
    
    // Check for command-line arguments
    bool fullscreen = false;  // Default to fullscreen for deployment
    QStringList args = app.arguments();
    if (args.contains("--windowed") || args.contains("-w")) {
        fullscreen = false;
        qInfo() << "Running in windowed mode (for development)";
    }
    
    // ========================================================================
    // CONFIGURATION LOADING - HYBRID APPROACH
    // Try filesystem first (for field updates), fallback to embedded resources
    // ========================================================================

    QString configDir = QCoreApplication::applicationDirPath() + "/config";
    qInfo() << "Configuration directory:" << QDir(configDir).absolutePath();

    // --- LOAD DEVICES CONFIGURATION ---
    QString devicesPath = configDir + "/devices.json";
    if (!QFileInfo::exists(devicesPath)) {
        qWarning() << "devices.json not found in filesystem, using embedded resource";
        devicesPath = ":/config/devices.json";
    }

    if (!DeviceConfiguration::load(devicesPath)) {
        qCritical() << "Failed to load device configuration from:" << devicesPath;
        qCritical() << "Cannot start without valid hardware configuration!";
        return -1;
    }
    qInfo() << "Loaded devices.json from:" << devicesPath;

    // --- LOAD MOTION TUNING CONFIGURATION ---
    QString motionTuningPath = configDir + "/motion_tuning.json";
    if (!QFileInfo::exists(motionTuningPath)) {
        qWarning() << "motion_tuning.json not found in filesystem, using embedded resource";
        motionTuningPath = ":/config/motion_tuning.json";
    }

    if (!MotionTuningConfig::load(motionTuningPath)) {
        qWarning() << "Failed to load motion tuning config from:" << motionTuningPath;
        qWarning() << "Using default values";
        // Continue anyway - defaults are loaded
    } else {
        qInfo() << "Loaded motion_tuning.json from:" << motionTuningPath;
    }

    // Validate all configurations
    if (!ConfigurationValidator::validateAll()) {
        qCritical() << "Configuration validation FAILED!";
        qCritical() << "Please fix configuration errors before continuing.";
        return -1;
    }

    // Initialize system
    SystemController sysCtrl;
    sysCtrl.initializeHardware();
    
    QQmlApplicationEngine engine;
    sysCtrl.initializeQmlSystem(&engine);
    
    // Load QML
    engine.load(QUrl(QStringLiteral("qrc:/qml/views/main.qml")));
    if (engine.rootObjects().isEmpty()) {
        qCritical() << "Failed to load QML!";
        return -1;
    }
    
    // Show window
    QObject *rootObject = engine.rootObjects().first();
    QQuickWindow *window = qobject_cast<QQuickWindow*>(rootObject);
    
    if (window) {
        if (fullscreen) {
            qInfo() << "Starting in FULLSCREEN mode";
            window->showFullScreen();
        } else {
            qInfo() << "Starting in WINDOWED mode";
            window->show();
        }
    } else {
        qWarning() << "Could not access window - using QML property fallback";
        if (fullscreen) {
            rootObject->setProperty("visibility", "FullScreen");
        }
    }
    
    // Start hardware
    sysCtrl.startSystem();
    
    return app.exec();
}