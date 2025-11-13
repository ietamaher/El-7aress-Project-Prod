import QtQuick
import QtQuick.Shapes

Item {
    id: osdRoot
    anchors.fill: parent

    property var viewModel: osdViewModel // Context property from C++
    property color accentColor: viewModel ? viewModel.accentColor : "#46E2A5"

    // Constants
    readonly property real azIndicatorRadius: 50
    readonly property real elScaleHeight: 120

    // ========================================================================
    // TEXT OVERLAYS (Top-Left Corner)
    // ========================================================================
    Column {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 10
        spacing: 5

        // Mode
        Text {
            text: viewModel ? viewModel.modeText : "MODE: IDLE"
            font.pixelSize: 16
            font.bold: true
            font.family: "Segoe UI"
            color: osdRoot.accentColor
            style: Text.Outline
            styleColor: "black"
        }

        // Motion Mode
        Text {
            text: viewModel ? viewModel.motionText : "MOTION: MAN"
            font.pixelSize: 16
            font.family: "Segoe UI"
            color: osdRoot.accentColor
            style: Text.Outline
            styleColor: "black"
        }

        // System Status
        Text {
            text: viewModel ? viewModel.statusText : "SYS: --- SAF NRD"
            font.pixelSize: 16
            font.family: "Segoe UI"
            color: osdRoot.accentColor
            style: Text.Outline
            styleColor: "black"
        }

        // Rate
        Text {
            text: viewModel ? viewModel.rateText : "RATE: SINGLE SHOT"
            font.pixelSize: 16
            font.family: "Segoe UI"
            color: osdRoot.accentColor
            style: Text.Outline
            styleColor: "black"
        }

        // LRF Distance
        Text {
            text: viewModel ? viewModel.lrfText : "LRF: --- m"
            font.pixelSize: 16
            font.family: "Segoe UI"
            color: osdRoot.accentColor
            style: Text.Outline
            styleColor: "black"
        }

        // SPEED
        Text {
            text: viewModel ? viewModel.speedText : "SPD: 0.0%"
            font.pixelSize: 16
            font.family: "Segoe UI"
            color: osdRoot.accentColor
            style: Text.Outline
            styleColor: "black"
        }
        // Zeroing Status
        Text {
            visible: viewModel ? viewModel.zeroingVisible : false
            text: viewModel ? viewModel.zeroingText : ""
            font.pixelSize: 16
            font.family: "Segoe UI"
            color: osdRoot.accentColor
            style: Text.Outline
            styleColor: "black"
        }

        // Windage Status
        Text {
            visible: viewModel ? viewModel.windageVisible : false
            text: viewModel ? viewModel.windageText : ""
            font.pixelSize: 16
            font.family: "Segoe UI"
            color: osdRoot.accentColor
            style: Text.Outline
            styleColor: "black"
        }

        // Detection Status
        Text {
            visible: viewModel ? viewModel.detectionVisible : false
            text: viewModel ? viewModel.detectionText : ""
            font.pixelSize: 16
            font.bold: true
            font.family: "Segoe UI"
            color: "#00FF00"  // Green color for active detection
            style: Text.Outline
            styleColor: "black"
        }

        // Lead Angle Status
        Text {
            visible: viewModel ? viewModel.leadAngleVisible : false
            text: viewModel ? viewModel.leadAngleText : ""
            font.pixelSize: 16
            font.family: "Segoe UI"
            color: viewModel && viewModel.leadAngleText.includes("LAG") ? "yellow" :
                   (viewModel && viewModel.leadAngleText.includes("ZOOM") ? "#C81428" : osdRoot.accentColor)
            style: Text.Outline
            styleColor: "black"
        }

        // Scan Name
        Text {
            visible: viewModel ? viewModel.scanNameVisible : false
            text: viewModel ? viewModel.scanNameText : ""
            font.pixelSize: 16
            font.family: "Segoe UI"
            color: osdRoot.accentColor
            style: Text.Outline
            styleColor: "black"
        }
    }

    // ========================================================================
    // TOP-RIGHT CORNER (Speed)
    // ========================================================================


    // ========================================================================
    // BOTTOM-LEFT CORNER (Stab, Camera, FOV)
    // ========================================================================
    Row {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: 10
        spacing: 50

        Text {
            text: viewModel ? viewModel.stabText : "STAB: OFF"
            font.pixelSize: 16
            font.family: "Segoe UI"
            color: osdRoot.accentColor
            style: Text.Outline
            styleColor: "black"
        }

        Text {
            text: viewModel ? viewModel.cameraText : "CAM: DAY"
            font.pixelSize: 16
            font.family: "Segoe UI"
            color: osdRoot.accentColor
            style: Text.Outline
            styleColor: "black"
        }

        Text {
            text: viewModel ? viewModel.fovText : "FOV: 45.0°"
            font.pixelSize: 16
            font.family: "Segoe UI"
            color: osdRoot.accentColor
            style: Text.Outline
            styleColor: "black"
        }
    }

    // ========================================================================
    // CENTER WARNING (Zone Warnings)
    // ========================================================================
    Text {
        anchors.centerIn: parent
        anchors.verticalCenterOffset: 50
        visible: viewModel ? viewModel.zoneWarningVisible : false
        text: viewModel ? viewModel.zoneWarningText : ""
        font.pixelSize: 24
        font.bold: true
        font.family: "Segoe UI"
        color: "#C81428" // Red for warnings
        style: Text.Outline
        styleColor: "black"

        // Background for better visibility
        Rectangle {
            anchors.fill: parent
            anchors.margins: -5
            color: "black"
            opacity: 0.7
            radius: 3
            z: -1
        }
    }

    // ========================================================================
    // STARTUP SEQUENCE MESSAGE (Center, Above Mid-Height)
    // ========================================================================
    Item {
        anchors.centerIn: parent
        anchors.verticalCenterOffset: -80  // Above center
        visible: viewModel ? viewModel.startupMessageVisible : false
        width: startupMessageText.width + 40
        height: startupMessageText.height + 20

        // Military-grade dark background panel
        Rectangle {
            anchors.fill: parent
            color: "#000000"
            opacity: 0.85
            radius: 2
            border.color: osdRoot.accentColor
            border.width: 2
        }

        // Message text
        Text {
            id: startupMessageText
            anchors.centerIn: parent
            text: viewModel ? viewModel.startupMessageText : ""
            font.pixelSize: 20
            font.bold: true
            font.family: "Segoe UI"
            font.letterSpacing: 1.5
            color: osdRoot.accentColor
            style: Text.Normal

            // Blinking animation for professional appearance
            SequentialAnimation on opacity {
                running: parent.parent.visible
                loops: Animation.Infinite

                NumberAnimation {
                    from: 1.0
                    to: 0.7
                    duration: 800
                    easing.type: Easing.InOutQuad
                }
                NumberAnimation {
                    from: 0.7
                    to: 1.0
                    duration: 800
                    easing.type: Easing.InOutQuad
                }
            }
        }

        // Corner decorations for military style
        Rectangle {
            x: 0
            y: 0
            width: 10
            height: 2
            color: osdRoot.accentColor
        }
        Rectangle {
            x: 0
            y: 0
            width: 2
            height: 10
            color: osdRoot.accentColor
        }
        Rectangle {
            x: parent.width - 10
            y: 0
            width: 10
            height: 2
            color: osdRoot.accentColor
        }
        Rectangle {
            x: parent.width - 2
            y: 0
            width: 2
            height: 10
            color: osdRoot.accentColor
        }
        Rectangle {
            x: 0
            y: parent.height - 2
            width: 10
            height: 2
            color: osdRoot.accentColor
        }
        Rectangle {
            x: 0
            y: parent.height - 10
            width: 2
            height: 10
            color: osdRoot.accentColor
        }
        Rectangle {
            x: parent.width - 10
            y: parent.height - 2
            width: 10
            height: 2
            color: osdRoot.accentColor
        }
        Rectangle {
            x: parent.width - 2
            y: parent.height - 10
            width: 2
            height: 10
            color: osdRoot.accentColor
        }
    }

    // ========================================================================
    // AZIMUTH INDICATOR (Top-Right)
    // ========================================================================
    AzimuthIndicator {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.rightMargin: 30
        anchors.topMargin: 50
        width: azIndicatorRadius * 2
        height: azIndicatorRadius * 2

        // === PROPERTIES ===
        azimuth: viewModel ? viewModel.azimuth : 0              // Gimbal azimuth (relative)
        vehicleHeading: viewModel ? viewModel.vehicleHeading : 0 // ⭐ IMU yaw (vehicle heading)
        imuConnected: viewModel ? viewModel.imuConnected : false // ⭐ IMU connection status

        color: osdRoot.accentColor
        relativeColor: "yellow"  // Color for vehicle reference line
    }

    // ========================================================================
    // ELEVATION SCALE (Right Side)
    // ========================================================================
    ElevationScale {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.rightMargin: 20
        anchors.topMargin: 250
        height: elScaleHeight

        elevation: viewModel ? viewModel.elevation : 0
        color: osdRoot.accentColor
    }

    // ========================================================================
    // TRACKING BOX
    // ========================================================================
    TrackingBox {
        visible: viewModel ? viewModel.trackingBoxVisible : false
        x: viewModel ? viewModel.trackingBox.x : 0
        y: viewModel ? viewModel.trackingBox.y : 0
        width: viewModel ? viewModel.trackingBox.width : 0
        height: viewModel ? viewModel.trackingBox.height : 0

        boxColor: viewModel ? viewModel.trackingBoxColor : "yellow"
        dashed: viewModel ? viewModel.trackingBoxDashed : false
    }

    // ========================================================================
    // ACQUISITION BOX
    // ========================================================================
    Rectangle {
        visible: viewModel ? viewModel.acquisitionBoxVisible : false
        x: viewModel ? viewModel.acquisitionBox.x : 0
        y: viewModel ? viewModel.acquisitionBox.y : 0
        width: viewModel ? viewModel.acquisitionBox.width : 0
        height: viewModel ? viewModel.acquisitionBox.height : 0

        color: "transparent"
        border.color: "yellow"
        border.width: 2
    }

    // ========================================================================
    // DETECTION BOXES (YOLO Object Detection)
    // ========================================================================
    Repeater {
        model: viewModel ? viewModel.detectionBoxes : []

        delegate: Item {
            x: modelData.x
            y: modelData.y
            width: modelData.width
            height: modelData.height

            // Bounding box rectangle
            Rectangle {
                anchors.fill: parent
                color: "transparent"
                border.color: Qt.rgba(modelData.colorR / 255.0,
                                     modelData.colorG / 255.0,
                                     modelData.colorB / 255.0,
                                     1.0)
                border.width: 3
            }

            // Class label background
            Rectangle {
                x: 0
                y: -24
                width: labelText.width + 12
                height: 22
                color: Qt.rgba(modelData.colorR / 255.0,
                              modelData.colorG / 255.0,
                              modelData.colorB / 255.0,
                              0.8)
                radius: 3

                // Class name and confidence
                Text {
                    id: labelText
                    anchors.centerIn: parent
                    text: modelData.className + " " + (modelData.confidence * 100).toFixed(0) + "%"
                    font.pixelSize: 14
                    font.bold: true
                    font.family: "Segoe UI"
                    color: "white"
                    style: Text.Outline
                    styleColor: "black"
                }
            }
        }
    }

    // ========================================================================
    // RETICLE (Center with offset)
    // ========================================================================
    ReticleRenderer {
        /*id: reticle
        anchors.centerIn: parent
        x: parent.width / 2 + (viewModel ? viewModel.reticleOffsetX : 0)
        y: parent.height / 2 + (viewModel ? viewModel.reticleOffsetY : 0)

        reticleType: viewModel ? viewModel.reticleType : 1
        color: osdRoot.accentColor
        currentFov: viewModel ? viewModel.currentFov : 45.0

        // CCIP-specific properties
        lacActive: viewModel ? viewModel.lacActive : false
        rangeMeters: viewModel ? viewModel.rangeMeters : 0
        confidenceLevel: viewModel ? viewModel.confidenceLevel : 1.0*/

        id: reticle
        anchors.centerIn: parent
        anchors.horizontalCenterOffset: viewModel ? viewModel.reticleOffsetX : 0
        anchors.verticalCenterOffset: viewModel ? viewModel.reticleOffsetY : 0

        reticleType: viewModel ? viewModel.reticleType : 1
        color: osdRoot.accentColor
        currentFov: viewModel ? viewModel.currentFov : 45.0


        lacActive: viewModel ? viewModel.lacActive : false
        rangeMeters: viewModel ? viewModel.rangeMeters : 0
        confidenceLevel: viewModel ? viewModel.confidenceLevel : 1.0
    }

    // ========================================================================
    // CCIP PIPPER (Impact Prediction with Lead Angle)
    // ========================================================================
    CcipPipper {
        id: ccipPipper

        // Position using absolute screen coordinates from viewModel
        x: (viewModel ? viewModel.ccipX : (parent.width / 2)) - (width / 2)
        y: (viewModel ? viewModel.ccipY : (parent.height / 2)) - (height / 2)

        pipperEnabled: viewModel ? viewModel.ccipVisible : false
        status: viewModel ? viewModel.ccipStatus : "Off"
        accentColor: osdRoot.accentColor

        // CCIP is only visible when LAC is active and provides a valid solution
        // It shows where bullets will impact accounting for:
        // • Gun zeroing offsets
        // • Target lead angle (moving target compensation)
        // • Ballistic trajectory
    }

    // ========================================================================
    // FIXED LOB MARKER (Screen Center - Always visible)
    // ========================================================================
    Item {
        anchors.centerIn: parent
        width: 10
        height: 10

        // Simple cross marker
        Rectangle {
            width: 10
            height: 2
            anchors.centerIn: parent
            color: osdRoot.accentColor
        }
        Rectangle {
            width: 2
            height: 10
            anchors.centerIn: parent
            color: osdRoot.accentColor
        }
    }

    // ========================================================================
    // ERROR MESSAGE DISPLAY (Bottom-Right Corner)
    // ========================================================================
    Rectangle {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: 10
        anchors.bottomMargin: 10
        visible: viewModel ? viewModel.errorMessageVisible : false

        width: errorMessageColumn.width + 24
        height: errorMessageColumn.height + 16

        color: "#000000"
        opacity: 0.9
        radius: 3
        border.color: "#C81428"  // Red border for errors
        border.width: 2

        Column {
            id: errorMessageColumn
            anchors.centerIn: parent
            spacing: 8

            // Error icon and title
            Row {
                spacing: 8

                // Warning icon
                Text {
                    text: "⚠"
                    font.pixelSize: 18
                    font.bold: true
                    color: "#C81428"  // Red
                    anchors.verticalCenter: parent.verticalCenter
                }

                Text {
                    text: "SYSTEM ERROR"
                    font.pixelSize: 14
                    font.bold: true
                    font.family: "Segoe UI"
                    font.letterSpacing: 1.2
                    color: "#C81428"  // Red for error header
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            // Error message text
            Text {
                text: viewModel ? viewModel.errorMessageText : ""
                font.pixelSize: 12
                font.family: "Segoe UI"
                color: "#FFFFFF"
                wrapMode: Text.WordWrap
                width: 280
                horizontalAlignment: Text.AlignLeft
            }

            // Action message
            Text {
                text: "► CONTACT MANUFACTURER"
                font.pixelSize: 11
                font.bold: true
                font.family: "Segoe UI"
                color: "#FFA500"  // Orange for action

                // Blinking animation
                SequentialAnimation on opacity {
                    running: parent.parent.parent.visible
                    loops: Animation.Infinite

                    NumberAnimation {
                        from: 1.0
                        to: 0.4
                        duration: 600
                        easing.type: Easing.InOutQuad
                    }
                    NumberAnimation {
                        from: 0.4
                        to: 1.0
                        duration: 600
                        easing.type: Easing.InOutQuad
                    }
                }
            }
        }

        // Diagonal stripes for warning appearance
        Repeater {
            model: 3
            Rectangle {
                x: index * 15 + 5
                y: 0
                width: 1
                height: parent.height
                color: "#C81428"
                opacity: 0.2
            }
        }
    }
}
