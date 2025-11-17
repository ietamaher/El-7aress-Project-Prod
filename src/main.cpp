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
    // Enable threaded render loop to utilize multiple cores (improves by ~20-30%)
    qputenv("QSG_RENDER_LOOP", "threaded");

    // Use RHI (Rendering Hardware Interface) with OpenGL ES for Jetson
    qputenv("QT_QUICK_BACKEND", "rhi");
    qputenv("QSG_RHI_BACKEND", "gles2");

    // Enable QML compiler optimizations
    qputenv("QML_DISABLE_DISK_CACHE", "0");

    // Optimize for Jetson's GPU architecture
    qputenv("QSG_INFO", "1");  // Enable Scene Graph debug info (disable in production)

    QGuiApplication app(argc, argv);
    gst_init(&argc, &argv);
    
    // Check for command-line arguments
    bool fullscreen = false;  // Default to fullscreen for deployment
    QStringList args = app.arguments();
    if (args.contains("--windowed") || args.contains("-w")) {
        fullscreen = false;
        qInfo() << "Running in windowed mode (for development)";
    }
    
    // Determine config directory - try multiple locations
    QString configDir;
    QString appDir = QCoreApplication::applicationDirPath();

    // List of paths to try (in order of preference)
    QStringList searchPaths = {
        "./config",                    // Current working directory
        appDir + "/config",            // Next to executable
        appDir + "/../config",         // Parent of executable (build/Debug/.. scenario)
        appDir + "/../../config",      // Two levels up (build/Desktop-Debug/../../ scenario)
        appDir + "/../../../config"    // Three levels up (deep build dirs)
    };

    // Find first path that contains motion_tuning.json
    bool found = false;
    for (const QString& path : searchPaths) {
        QString testPath = QDir(path).absolutePath() + "/motion_tuning.json";
        if (QFileInfo::exists(testPath)) {
            configDir = path;
            found = true;
            qInfo() << "Found config at:" << QDir(path).absolutePath();
            break;
        }
    }

    if (!found) {
        qWarning() << "Config directory not found in any search path!";
        qWarning() << "Searched paths:";
        for (const QString& path : searchPaths) {
            qWarning() << "  -" << QDir(path).absolutePath();
        }
        configDir = "./config"; // Fallback to current dir
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