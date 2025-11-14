#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow> 
#include "controllers/systemcontroller.h"
#include "controllers/deviceconfiguration.h"
#include <gst/gst.h>


int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    gst_init(&argc, &argv);
    
    // Check for command-line arguments
    bool fullscreen = true;  // Default to fullscreen for deployment
    QStringList args = app.arguments();
    if (args.contains("--windowed") || args.contains("-w")) {
        fullscreen = false;
        qInfo() << "Running in windowed mode (for development)";
    }
    
    // Load configuration
    if (!DeviceConfiguration::load("./config/devices.json")) {
        qCritical() << "Failed to load device configuration!";
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
