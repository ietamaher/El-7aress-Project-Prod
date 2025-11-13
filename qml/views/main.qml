import QtQuick
import QtQuick.Controls
import "qrc:/qml/components"
import "qrc:/qml/views"
import "../components"

Window {
    id: mainWindow
    visible: true
    width: 1024
    height: 768
    title: "RCWS System"

    // Background color (visible if video fails)
    color: "black"

    // ========================================================================
    // VIDEO FEED BACKGROUND
    // ========================================================================
    Image {
        id: videoDisplay
        anchors.fill: parent
        fillMode: Image.PreserveAspectFit
        source: "image://video/camera"
        cache: false // Don't cache, we want fresh frames

        // Timer to refresh video feed
        Timer {
            interval: 33 // ~30 FPS (adjust based on camera framerate)
            running: true
            repeat: true
            onTriggered: {
                // Force QML to request new image from provider
                videoDisplay.source = "image://video/camera?" + Date.now()
            }
        }

        // Fallback if video not available
        Text {
            anchors.centerIn: parent
            text: "Waiting for video signal..."
            color: "gray"
            font.pixelSize: 24
            visible: videoDisplay.status === Image.Null || videoDisplay.status === Image.Error
        }
    }

    // ========================================================================
    // OSD OVERLAY (On top of video)
    // ========================================================================
    OsdOverlay {
        id: osdOverlay
        //anchors.fill: parent
        z: 10 // Above video
    }

    // === MAIN MENU ===
    MainMenu {
        id: mainMenu
        viewModel: mainMenuViewModel
        osdViewModel: osdViewModelInstance
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.topMargin: 150
        anchors.leftMargin: 10
        z: 100
    }

    // === RETICLE MENU ===
    MainMenu {
        id: reticleMenu
        viewModel: reticleMenuViewModel
        height: 300
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.topMargin: 150
        anchors.leftMargin: 10
        z: 100
    }

    // === COLOR MENU ===
    MainMenu {
        id: colorMenu
        viewModel: colorMenuViewModel
        osdViewModel: osdViewModelInstance
        height: 300
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.topMargin: 150
        anchors.leftMargin: 10
        z: 100
    }

    // === ZEROING OVERLAY ===
    ZeroingOverlay {
        id: zeroingOverlay
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.topMargin: 150
        anchors.leftMargin: 10
        z: 100
    }

    // === WINDAGE OVERLAY ===
    WindageOverlay {
        id: windageOverlay
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.topMargin: 150
        anchors.leftMargin: 10
        z: 100
    }

    // === ENVIRONMENTAL OVERLAY ===
    EnvironmentalOverlay {
        id: environmentalOverlay
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.topMargin: 150
        anchors.leftMargin: 10
        z: 100
    }

    ZoneDefinitionOverlay {
        id: zoneDefinitionOverlay
        anchors.fill: parent
        z: 100   // Above OSD

        // ViewModels are accessible via context properties
        // Already bound in ZoneDefinitionOverlay.qml
    }

    SystemStatusOverlay{
            id: systemStatusOverlay
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.topMargin: 150
            anchors.leftMargin: 10
            z: 100   // Above OSD

    }

    AboutDialog{
        id: aboutDialog
        anchors.fill: parent
        z: 100   // Above OSD
    }

    // ========================================================================
    // DEBUG INFO (Remove in production)
    // ========================================================================
    /*Rectangle {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 10
        width: 200
        height: 60
        color: "red"
        opacity: 0.7
        visible: true // Set to true for debugging
        z: 1000
        Column {
            anchors.centerIn: parent
            spacing: 5

            Text {
                text: "Video: " + (videoDisplay.status === Image.Ready ? "OK" : "NO SIGNAL")
                color: videoDisplay.status === Image.Ready ? "green" : "red"
                font.pixelSize: 12
            }

            Text {
                text: "Size: " + videoDisplay.sourceSize.width + "x" + videoDisplay.sourceSize.height
                color: "white"
                font.pixelSize: 12
            }

            Text {
                text: "OSD: " + (osdViewModel ? "ACTIVE" : "INACTIVE")
                color: osdViewModel ? "green" : "red"
                font.pixelSize: 12
            }
        }
    }*/

    // Physical Button Handlers - ONLY 3 BUTTONS
    Row {
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: 20
        spacing: 15
        z: 200

        Button {
            text: "UP"
            width: 80
            height: 40
            onClicked: {
                console.log("[UI] UP button clicked")
                appController.onUpButtonPressed()
            }
        }

        Button {
            text: "MENU/VAL"
            width: 100
            height: 40
            highlighted: true  // Make it stand out as the primary button
            onClicked: {
                console.log("[UI] MENU/VAL button clicked")
                appController.onMenuValButtonPressed()
            }
        }

        Button {
            text: "DOWN"
            width: 80
            height: 40
            onClicked: {
                console.log("[UI] DOWN button clicked")
                appController.onDownButtonPressed()
            }
        }
    }

}
