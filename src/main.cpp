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
    // CRITICAL: Configure Qt BEFORE QGuiApplication is created
    // ========================================================================
    
    // Check if running in production (systemd service)
    bool isProduction = qEnvironmentVariableIsSet("RCWS_PRODUCTION");
    
    if (isProduction || !qEnvironmentVariableIsSet("DISPLAY")) {
        // PRODUCTION MODE: EGLFS
        qputenv("QT_QPA_PLATFORM", "eglfs");
        qputenv("QT_QPA_EGLFS_INTEGRATION", "eglfs_kms");
        qputenv("QT_QPA_EGLFS_ALWAYS_SET_MODE", "1");
        qputenv("QT_QPA_EGLFS_FORCE_888", "1");
        qInfo() << "EGLFS mode configured";
    }
    
    // Always set these optimizations
    qputenv("QT_QUICK_BACKEND", "rhi");
    qputenv("QSG_RHI_BACKEND", "opengl");
    qputenv("QSG_RENDER_LOOP", "basic");
    
    // NOW create QGuiApplication (reads environment variables above)
    QGuiApplication app(argc, argv);
    
    qInfo() << "QPA Platform:" << app.platformName();
    
    gst_init(&argc, &argv);
    
    // ========================================================================
    // CONFIGURATION LOADING
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
    } else {
        qInfo() << "Loaded motion_tuning.json from:" << motionTuningPath;
    }

    // Validate all configurations
    if (!ConfigurationValidator::validateAll()) {
        qCritical() << "Configuration validation FAILED!";
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
        qInfo() << "Showing window (fullscreen in EGLFS)";
        //window->show();  // In EGLFS, show() is always fullscreen
        window->showFullScreen();
    }
    
    // Start hardware
    sysCtrl.startSystem();
    
    return app.exec();
}