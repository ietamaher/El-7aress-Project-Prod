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
    QGuiApplication app(argc, argv);
    gst_init(&argc, &argv);
    
    // Check for command-line arguments
    bool fullscreen = false;  // Default to fullscreen for deployment
    QStringList args = app.arguments();
    if (args.contains("--windowed") || args.contains("-w")) {
        fullscreen = false;
        qInfo() << "Running in windowed mode (for development)";
    }
    
    // Determine config directory (try current dir first, then app dir)
    QString configDir = "./config";
    if (!QFileInfo::exists(configDir + "/devices.json")) {
        // Try relative to executable location
        QString appDir = QCoreApplication::applicationDirPath();
        configDir = appDir + "/config";
        if (!QFileInfo::exists(configDir + "/devices.json")) {
            // Try parent directory (for build dir scenario)
            configDir = appDir + "/../config";
        }
    }

    qInfo() << "Using config directory:" << QDir(configDir).absolutePath();

    // Load configuration
    QString devicesPath = configDir + "/devices.json";
    if (!DeviceConfiguration::load(devicesPath)) {
        qCritical() << "Failed to load device configuration from:" << devicesPath;
        return -1;
    }

    // Load motion tuning configuration
    QString motionTuningPath = configDir + "/motion_tuning.json";
    if (!MotionTuningConfig::load(motionTuningPath)) {
        qWarning() << "Failed to load motion tuning config from:" << motionTuningPath;
        qWarning() << "Using default values";
        // Continue anyway - defaults are loaded
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
