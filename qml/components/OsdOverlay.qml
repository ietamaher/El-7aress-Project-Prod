import QtQuick
import QtQuick.Shapes

Item {
    id: osdRoot
    anchors.fill: parent

    property var viewModel: osdViewModel // Context property from C++

    // ========================================================================
    // COLOR THEME
    // ========================================================================
    readonly property color accentColor: viewModel ? viewModel.accentColor : "#46E2A5"
    readonly property color warningColor: "#C81428"  // Red for critical warnings
    readonly property color cautionColor: "#FFA500"  // Orange for cautions

    // ========================================================================
    // FONT SETTINGS
    // ========================================================================
    readonly property string primaryFont: "Archivo Narrow"
    readonly property string fallbackFont: "Segoe UI"

    // ========================================================================
    // CALCULATED PROPERTIES
    // ========================================================================
    readonly property real trueBearing: {
        if (!viewModel || !viewModel.imuConnected) return viewModel ? viewModel.azimuth : 0.0;
        var bearing = viewModel.azimuth + viewModel.vehicleHeading;
        while (bearing >= 360.0) bearing -= 360.0;
        while (bearing < 0.0) bearing += 360.0;
        return bearing;
    }

    // ========================================================================
    // TOP LEFT - STATUS BLOCK (Organized Info in Columns)
    // ========================================================================
    Rectangle {
        id: statusBlock
        x: 10
        y: 10
        width: 340
        // Dynamic height based on content
        height: statusRow.height + 16  // Content height + padding (8px top + 8px bottom)
        color: "#CC000000"
        border.color: accentColor
        border.width: 1

        Row {
            id: statusRow
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: 8
            spacing: 10  // Space between left and right columns

            // === LEFT COLUMN ===
            Column {
                spacing: 2
                width: 168

                // MODE
                Text {
                    text: viewModel ? viewModel.modeText : "MODE: IDLE"
                    font.pixelSize: 15
                    font.bold: true
                    font.family: primaryFont
                    color: accentColor
                }

                // RATE
                Text {
                    text: viewModel ? viewModel.rateText : "RATE: SINGLE SHOT"
                    font.pixelSize: 13
                    font.family: primaryFont
                    color: accentColor
                }

                // Procedures row (only visible when active)
                Row {
                    spacing: 15
                    visible: (viewModel && viewModel.zeroingVisible) ||
                             (viewModel && viewModel.environmentVisible)

                    Text {
                        visible: viewModel ? viewModel.zeroingVisible : false
                        text: viewModel ? ("[" + viewModel.zeroingText + "]") : "[Z]"
                        font.pixelSize: 12
                        font.family: primaryFont
                        color: accentColor
                    }

                    Text {
                        visible: viewModel ? viewModel.environmentVisible : false
                        text: viewModel ? viewModel.environmentText : "ENV"
                        font.pixelSize: 12
                        font.family: primaryFont
                        color: accentColor
                    }


                }

                // DETECTION
                Text {
                    visible: viewModel ? viewModel.detectionVisible : false
                    text: viewModel ? viewModel.detectionText : "DETECTION: ON"
                    font.pixelSize: 12
                    font.family: primaryFont
                    color: accentColor
                }
            }

            // === RIGHT COLUMN ===
            Column {
                spacing: 2

                // MOTION
                Text {
                    text: viewModel ? viewModel.motionText : "MOTION: MAN"
                    font.pixelSize: 13
                    font.family: primaryFont
                    font.bold: true
                    color: accentColor
                }

                // SCAN INFO (directly under MOTION)
                Text {
                    visible: viewModel ? viewModel.scanNameVisible : false
                    text: viewModel ? ("[SCAN: " + viewModel.scanNameText + "]") : "[SCAN]"
                    font.pixelSize: 12
                    font.family: primaryFont
                    color: "cyan"
                }

                // LEAD ANGLE STATUS
                Text {
                    visible: viewModel ? viewModel.leadAngleVisible : false
                    text: viewModel ? viewModel.leadAngleText : ""
                    font.pixelSize: 12
                    font.family: primaryFont
                    color: viewModel && viewModel.leadAngleText.includes("LAG") ? "yellow" :
                           (viewModel && viewModel.leadAngleText.includes("ZOOM") ? warningColor : accentColor)
                }

            }
        }
    }

    // ========================================================================
    // TOP LEFT - WINDAGE/CROSSWIND DISPLAY (Below Status Block)
    // ========================================================================
    Rectangle {
        id: windageBox
        x: 10
        y: statusBlock.y + statusBlock.height + 8
        width: 340
        height: 32
        visible: viewModel ? viewModel.windageVisible : false
        color: "#99000000"
        border.color: accentColor
        border.width: 1

        Text {
            anchors.centerIn: parent
            text: viewModel ? viewModel.windageText : ""
            font.pixelSize: 13
            font.bold: true
            font.family: primaryFont
            color: accentColor
        }
    }

    // ========================================================================
    // TOP RIGHT - AZIMUTH DISPLAY WITH CARDINAL DIRECTIONS
    // ========================================================================
    Rectangle {
        id: azimuthBox
        x: parent.width - 130
        y: 10
        width: 120
        height: 85
        color: "#99000000"
        border.color: accentColor
        border.width: 1

        Column {
            anchors.centerIn: parent
            spacing: 3

            Text {
                text: trueBearing.toFixed(1) + "°"
                font.pixelSize: 24
                font.bold: true
                font.family: primaryFont
                color: accentColor
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Text {
                visible: viewModel ? viewModel.imuConnected : false
                text: viewModel ? ("(" + viewModel.azimuth.toFixed(1) + " REL)") : "(0.0 REL)"
                font.pixelSize: 11
                font.family: primaryFont
                color: "yellow"
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Text {
                text: getCardinalDirection(trueBearing)
                font.pixelSize: 16
                font.bold: true
                font.family: primaryFont
                color: accentColor
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Text {
                visible: viewModel ? !viewModel.imuConnected : true
                text: "NO IMU"
                font.pixelSize: 10
                font.bold: true
                font.family: primaryFont
                color: warningColor
                anchors.horizontalCenter: parent.horizontalCenter
            }
        }
    }

    // ========================================================================
    // TOP RIGHT - GUN ARM STATUS BOX (Below Azimuth)
    // ========================================================================
    Rectangle {
        id: gunStatusBox
        x: parent.width - 130
        y: 105
        width: 120
        height: 45
        color: getGunArmedStatus() ? warningColor : "#555555"
        border.color: "white"
        border.width: 3

        Text {
            anchors.centerIn: parent
            text: getGunArmedStatus() ? "ARMED" : "SAFE"
            font.pixelSize: 18
            font.bold: true
            font.family: primaryFont
            color: "white"
        }

        SequentialAnimation on opacity {
            running: getGunArmedStatus()
            loops: Animation.Infinite
            NumberAnimation { from: 1.0; to: 0.6; duration: 700 }
            NumberAnimation { from: 0.6; to: 1.0; duration: 700 }
        }
    }

    // ========================================================================
    // RIGHT SIDE - SYSTEM HEALTH WARNINGS (Only when faults detected)
    // ========================================================================
    function hasDeviceFaults() {
        if (!viewModel) return false;
        return !viewModel.dayCameraConnected || viewModel.dayCameraError ||
               !viewModel.nightCameraConnected || viewModel.nightCameraError ||
               !viewModel.azServoConnected || viewModel.azFault ||
               !viewModel.elServoConnected || viewModel.elFault ||
               !viewModel.lrfConnected || viewModel.lrfFault || viewModel.lrfOverTemp ||
               !viewModel.actuatorConnected || viewModel.actuatorFault ||
               !viewModel.imuConnected ||
               !viewModel.plc21Connected ||
               !viewModel.plc42Connected;
    }

    Column {
        x: parent.width - 145
        y: parent.height - 300
        spacing: 3
        visible: hasDeviceFaults()

        // Day Camera warnings
        Rectangle {
            visible: viewModel && !viewModel.dayCameraConnected
            width: 140
            height: 28
            color: warningColor  // Red for disconnection
            border.color: "black"
            border.width: 2
            radius: 2

            Text {
                anchors.centerIn: parent
                text: "⚠ DAY CAM DISC"
                font.pixelSize: 11
                font.bold: true
                font.family: primaryFont
                color: "white"
            }
        }
        Rectangle {
            visible: viewModel && viewModel.dayCameraConnected && viewModel.dayCameraError
            width: 140
            height: 28
            color: cautionColor  // Orange for fault
            border.color: "black"
            border.width: 2
            radius: 2

            Text {
                anchors.centerIn: parent
                text: "⚠ DAY CAM FAULT"
                font.pixelSize: 11
                font.bold: true
                font.family: primaryFont
                color: "black"
            }
        }

        // Night Camera warnings
        Rectangle {
            visible: viewModel && !viewModel.nightCameraConnected
            width: 140
            height: 28
            color: warningColor
            border.color: "black"
            border.width: 2
            radius: 2

            Text {
                anchors.centerIn: parent
                text: "⚠ IR CAM DISC"
                font.pixelSize: 11
                font.bold: true
                font.family: primaryFont
                color: "white"
            }
        }
        Rectangle {
            visible: viewModel && viewModel.nightCameraConnected && viewModel.nightCameraError
            width: 140
            height: 28
            color: cautionColor
            border.color: "black"
            border.width: 2
            radius: 2

            Text {
                anchors.centerIn: parent
                text: "⚠ IR CAM FAULT"
                font.pixelSize: 11
                font.bold: true
                font.family: primaryFont
                color: "black"
            }
        }

        // Azimuth Servo warnings
        Rectangle {
            visible: viewModel && !viewModel.azServoConnected
            width: 140
            height: 28
            color: warningColor
            border.color: "black"
            border.width: 2
            radius: 2

            Text {
                anchors.centerIn: parent
                text: "⚠ AZ SERVO DISC"
                font.pixelSize: 11
                font.bold: true
                font.family: primaryFont
                color: "white"
            }
        }
        Rectangle {
            visible: viewModel && viewModel.azServoConnected && viewModel.azFault
            width: 140
            height: 28
            color: cautionColor
            border.color: "black"
            border.width: 2
            radius: 2

            Text {
                anchors.centerIn: parent
                text: "⚠ AZ SERVO FAULT"
                font.pixelSize: 11
                font.bold: true
                font.family: primaryFont
                color: "black"
            }
        }

        // Elevation Servo warnings
        Rectangle {
            visible: viewModel && !viewModel.elServoConnected
            width: 140
            height: 28
            color: warningColor
            border.color: "black"
            border.width: 2
            radius: 2

            Text {
                anchors.centerIn: parent
                text: "⚠ EL SERVO DISC"
                font.pixelSize: 11
                font.bold: true
                font.family: primaryFont
                color: "white"
            }
        }
        Rectangle {
            visible: viewModel && viewModel.elServoConnected && viewModel.elFault
            width: 140
            height: 28
            color: cautionColor
            border.color: "black"
            border.width: 2
            radius: 2

            Text {
                anchors.centerIn: parent
                text: "⚠ EL SERVO FAULT"
                font.pixelSize: 11
                font.bold: true
                font.family: primaryFont
                color: "black"
            }
        }

        // LRF warnings
        Rectangle {
            visible: viewModel && !viewModel.lrfConnected
            width: 140
            height: 28
            color: warningColor
            border.color: "black"
            border.width: 2
            radius: 2

            Text {
                anchors.centerIn: parent
                text: "⚠ LRF DISC"
                font.pixelSize: 11
                font.bold: true
                font.family: primaryFont
                color: "white"
            }
        }
        Rectangle {
            visible: viewModel && viewModel.lrfConnected && viewModel.lrfFault
            width: 140
            height: 28
            color: cautionColor
            border.color: "black"
            border.width: 2
            radius: 2

            Text {
                anchors.centerIn: parent
                text: "⚠ LRF FAULT"
                font.pixelSize: 11
                font.bold: true
                font.family: primaryFont
                color: "black"
            }
        }
        Rectangle {
            visible: viewModel && viewModel.lrfOverTemp
            width: 140
            height: 28
            color: cautionColor
            border.color: "black"
            border.width: 2
            radius: 2

            Text {
                anchors.centerIn: parent
                text: "⚠ LRF OVERTEMP"
                font.pixelSize: 11
                font.bold: true
                font.family: primaryFont
                color: "black"
            }
        }

        // Actuator warnings
        Rectangle {
            visible: viewModel && !viewModel.actuatorConnected
            width: 140
            height: 28
            color: warningColor
            border.color: "black"
            border.width: 2
            radius: 2

            Text {
                anchors.centerIn: parent
                text: "⚠ ACTUATOR DISC"
                font.pixelSize: 11
                font.bold: true
                font.family: primaryFont
                color: "white"
            }
        }
        Rectangle {
            visible: viewModel && viewModel.actuatorConnected && viewModel.actuatorFault
            width: 140
            height: 28
            color: cautionColor
            border.color: "black"
            border.width: 2
            radius: 2

            Text {
                anchors.centerIn: parent
                text: "⚠ ACTUATOR FAULT"
                font.pixelSize: 11
                font.bold: true
                font.family: primaryFont
                color: "black"
            }
        }

        // IMU warning
        Rectangle {
            visible: viewModel && !viewModel.imuConnected
            width: 140
            height: 28
            color: warningColor
            border.color: "black"
            border.width: 2
            radius: 2

            Text {
                anchors.centerIn: parent
                text: "⚠ IMU DISC"
                font.pixelSize: 11
                font.bold: true
                font.family: primaryFont
                color: "white"
            }
        }

        // PLC21 warning
        Rectangle {
            visible: viewModel && !viewModel.plc21Connected
            width: 140
            height: 28
            color: warningColor
            border.color: "black"
            border.width: 2
            radius: 2

            Text {
                anchors.centerIn: parent
                text: "⚠ PLC21 DISC"
                font.pixelSize: 11
                font.bold: true
                font.family: primaryFont
                color: "white"
            }
        }

        // PLC42 warning
        Rectangle {
            visible: viewModel && !viewModel.plc42Connected
            width: 140
            height: 28
            color: warningColor
            border.color: "black"
            border.width: 2
            radius: 2

            Text {
                anchors.centerIn: parent
                text: "⚠ PLC42 DISC"
                font.pixelSize: 11
                font.bold: true
                font.family: primaryFont
                color: "white"
            }
        }
    }

    // ========================================================================
    // RIGHT SIDE - SPEED DISPLAY
    // ========================================================================
    Rectangle {
        id: speedBox
        x: parent.width - 85
        y: parent.height/2 - 130
        width: 75
        height: 80
        color: "#99000000"
        border.color: accentColor
        border.width: 1

        Column {
            anchors.centerIn: parent
            spacing: 8

            Text {
                text:  viewModel ? viewModel.speedText : "0.0%"
                font.pixelSize: 20
                font.bold: true
                font.family: primaryFont
                color: accentColor
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Text {
                text: "SPEED"
                font.pixelSize: 11
                font.family: primaryFont
                color: accentColor
                anchors.horizontalCenter: parent.horizontalCenter
            }
        }
    }

    // ========================================================================
    // RIGHT SIDE - ELEVATION DISPLAY
    // ========================================================================
    Rectangle {
        id: elevationBox
        x: parent.width - 85
        y: parent.height/2 - 45
        width: 75
        height: 80
        color: "#99000000"
        border.color: accentColor
        border.width: 1

        Column {
            anchors.centerIn: parent
            spacing: 8

            Text {
                text: viewModel ? (viewModel.elevation.toFixed(1) + "°") : "0.0°"
                font.pixelSize: 20
                font.bold: true
                font.family: primaryFont
                color: accentColor
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Text {
                text: "ELEV"
                font.pixelSize: 11
                font.family: primaryFont
                color: accentColor
                anchors.horizontalCenter: parent.horizontalCenter
            }
        }
    }

    // ========================================================================
    // CENTER - RETICLE (Existing ReticleRenderer Component)
    // ========================================================================
    ReticleRenderer {
        id: reticle
        anchors.centerIn: parent
        anchors.horizontalCenterOffset: viewModel ? viewModel.reticleOffsetX : 0
        anchors.verticalCenterOffset: viewModel ? viewModel.reticleOffsetY : 0

        reticleType: viewModel ? viewModel.reticleType : 1
        color: accentColor
        currentFov: viewModel ? viewModel.currentFov : 45.0

        lacActive: viewModel ? viewModel.lacActive : false
        rangeMeters: viewModel ? viewModel.rangeMeters : 0
        confidenceLevel: viewModel ? viewModel.confidenceLevel : 1.0
    }

    // ========================================================================
    // CENTER - FIXED LOB MARKER (Screen Center - Always visible)
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
            color: accentColor
        }
        Rectangle {
            width: 2
            height: 10
            anchors.centerIn: parent
            color: accentColor
        }
    }

    // ========================================================================
    // DEBUG: Absolute position marker at 512x384
    // ========================================================================
    Rectangle {
        id: debugCenterMarker
        x: 512 - 15
        y: 384 - 15
        width: 30
        height: 30
        color: "transparent"
        border.color: "magenta"
        border.width: 3
        visible: true
        z: 500

        Text {
            anchors.top: parent.bottom
            anchors.topMargin: 2
            anchors.horizontalCenter: parent.horizontalCenter
            text: "DEBUG\n512x384"
            font.pixelSize: 10
            color: "magenta"
            horizontalAlignment: Text.AlignHCenter
            style: Text.Outline
            styleColor: "#000000"
        }

        Component.onCompleted: {
            console.log("🔍 DEBUG MARKER: Positioned at absolute x=" + x + ", y=" + y
                       + ", parent.width=" + parent.width + ", parent.height=" + parent.height
                       + ", center should be at (" + (parent.width/2) + "," + (parent.height/2) + ")");
        }
    }

    // ========================================================================
    // CENTER - CCIP PIPPER (Impact Prediction with Lead Angle)
    // ========================================================================
    CcipPipper {
        id: ccipPipper

        // Position using absolute screen coordinates from viewModel
        x: (viewModel ? viewModel.ccipX : (parent.width / 2)) - (width / 2)
        y: (viewModel ? viewModel.ccipY : (parent.height / 2)) - (height / 2)

        pipperEnabled: viewModel ? viewModel.ccipVisible : false
        status: viewModel ? viewModel.ccipStatus : "Off"
        accentColor: osdRoot.accentColor

        // Debug logging
        onXChanged: {
            if (visible) {
                console.log("🎯 CCIP QML: viewModel.ccipX=" + (viewModel ? viewModel.ccipX : "null")
                           + ", width=" + width + ", calculated x=" + x
                           + ", actual center X=" + (x + width/2));
            }
        }
        onYChanged: {
            if (visible) {
                console.log("🎯 CCIP QML: viewModel.ccipY=" + (viewModel ? viewModel.ccipY : "null")
                           + ", height=" + height + ", calculated y=" + y
                           + ", actual center Y=" + (y + height/2)
                           + ", parent.height=" + parent.height);
            }
        }
    }

    // ========================================================================
    // TRACKING BOX (Existing Component)
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
    // ACQUISITION BOX (For Tracking Phase)
    // ========================================================================
    Rectangle {
        id: acquisitionBox
        visible: viewModel ? viewModel.acquisitionBoxVisible : false
        x: viewModel ? viewModel.acquisitionBox.x : 0
        y: viewModel ? viewModel.acquisitionBox.y : 0
        width: viewModel ? viewModel.acquisitionBox.width : 0
        height: viewModel ? viewModel.acquisitionBox.height : 0

        color: "transparent"
        border.color: "yellow"
        border.width: 2

        // Debug logging
        onXChanged: {
            if (visible) {
                console.log("📦 ACQ BOX QML: viewModel.acquisitionBox.x=" + (viewModel ? viewModel.acquisitionBox.x : "null")
                           + ", calculated x=" + x
                           + ", width=" + width
                           + ", center X=" + (x + width/2));
            }
        }
        onYChanged: {
            if (visible) {
                console.log("📦 ACQ BOX QML: viewModel.acquisitionBox.y=" + (viewModel ? viewModel.acquisitionBox.y : "null")
                           + ", calculated y=" + y
                           + ", height=" + height
                           + ", center Y=" + (y + height/2)
                           + ", parent.height=" + parent.height);
            }
        }
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
                    font.family: primaryFont
                    color: "white"
                    style: Text.Outline
                    styleColor: "black"
                }
            }
        }
    }

    // ========================================================================
    // BOTTOM LEFT - NO FIRE ZONE WARNING
    // ========================================================================
    Rectangle {
        id: noFireWarning
        x: 10
        y: parent.height - 115
        width: 480
        height: 65
        color: warningColor
        opacity: 0.95
        border.color: "white"
        border.width: 4
        radius: 4
        visible: viewModel ? viewModel.zoneWarningVisible && viewModel.zoneWarningText.includes("NO FIRE") : false
        z: 200

        Text {
            anchors.centerIn: parent
            text: "⚠ NO FIRE ZONE ⚠\nWEAPON INHIBITED"
            font.pixelSize: 20
            font.bold: true
            font.family: primaryFont
            color: "white"
            horizontalAlignment: Text.AlignHCenter
        }

        SequentialAnimation on opacity {
            running: noFireWarning.visible
            loops: Animation.Infinite
            NumberAnimation { from: 0.95; to: 0.5; duration: 500 }
            NumberAnimation { from: 0.5; to: 0.95; duration: 500 }
        }
    }

    // ========================================================================
    // BOTTOM RIGHT - NO TRAVERSE WARNING
    // ========================================================================
    Rectangle {
        id: noTraverseWarning
        x: parent.width - 300
        y: parent.height - 100
        width: 290
        height: 50
        color: cautionColor
        opacity: 0.9
        border.color: "black"
        border.width: 3
        radius: 4
        visible: viewModel ? viewModel.zoneWarningVisible && viewModel.zoneWarningText.includes("NO TRAVERSE") : false
        z: 199

        Text {
            anchors.centerIn: parent
            text: "⚠ NO TRAVERSE ZONE"
            font.pixelSize: 18
            font.bold: true
            font.family: primaryFont
            color: "black"
        }
    }

    // ========================================================================
    // CENTER - STARTUP MESSAGE
    // ========================================================================
    Rectangle {
        id: startupBox
        x: parent.width/2 - width/2
        y: parent.height/2 - 100
        width: 420
        height: 50
        color: "#CC000000"
        border.color: accentColor
        border.width: 2
        radius: 3
        visible: viewModel ? viewModel.startupMessageVisible : false
        z: 100

        Text {
            anchors.centerIn: parent
            text: viewModel ? viewModel.startupMessageText : "SYSTEM INITIALIZATION..."
            font.pixelSize: 16
            font.family: primaryFont
            color: accentColor

            // Blinking animation
            SequentialAnimation on opacity {
                running: parent.parent.visible
                loops: Animation.Infinite
                NumberAnimation { from: 1.0; to: 0.7; duration: 800 }
                NumberAnimation { from: 0.7; to: 1.0; duration: 800 }
            }
        }
    }

    // ========================================================================
    // BOTTOM RIGHT - ERROR MESSAGE DISPLAY
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
        border.color: warningColor
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
                    color: warningColor
                    anchors.verticalCenter: parent.verticalCenter
                }

                Text {
                    text: "SYSTEM ERROR"
                    font.pixelSize: 14
                    font.bold: true
                    font.family: primaryFont
                    font.letterSpacing: 1.2
                    color: warningColor
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            // Error message text
            Text {
                text: viewModel ? viewModel.errorMessageText : ""
                font.pixelSize: 12
                font.family: primaryFont
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
                font.family: primaryFont
                color: cautionColor

                // Blinking animation
                SequentialAnimation on opacity {
                    running: parent.parent.parent.visible
                    loops: Animation.Infinite
                    NumberAnimation { from: 1.0; to: 0.4; duration: 600 }
                    NumberAnimation { from: 0.4; to: 1.0; duration: 600 }
                }
            }
        }
    }

    // ========================================================================
    // BOTTOM - ENHANCED STATUS BAR
    // ========================================================================
    Rectangle {
        id: bottomBar
        x: 10
        y: parent.height - 40
        width: parent.width - 20
        height: 30
        color: "#B3000000"
        border.color: accentColor
        border.width: 1

        Row {
            anchors.centerIn: parent
            spacing: 20

            // STAB
            Text {
                text: viewModel ? viewModel.stabText : "STAB: OFF"
                font.pixelSize: 14
                font.family: primaryFont
                color: accentColor
                anchors.verticalCenter: parent.verticalCenter
            }

            Rectangle { width: 2; height: 20; color: accentColor }

            // CAM
            Text {
                text: viewModel ? viewModel.cameraText : "CAM: DAY"
                font.pixelSize: 14
                font.family: primaryFont
                color: accentColor
                anchors.verticalCenter: parent.verticalCenter
            }

            Rectangle { width: 2; height: 20; color: accentColor }

            // ========================================================================
            // FOV SLIDER WIDGET
            // ========================================================================
            Row {
                spacing: 5
                anchors.verticalCenter: parent.verticalCenter

                Text {
                    text: viewModel ? viewModel.fovText : "FOV: 45.0°"
                    font.pixelSize: 14
                    font.family: primaryFont
                    color: accentColor
                    anchors.verticalCenter: parent.verticalCenter
                }

                Text {
                    text: "NFOV"
                    font.pixelSize: 11
                    font.family: primaryFont
                    color: accentColor
                    anchors.verticalCenter: parent.verticalCenter
                }

                Item {
                    width: 60
                    height: 20
                    anchors.verticalCenter: parent.verticalCenter

                    Rectangle {
                        width: parent.width
                        height: 2
                        color: accentColor
                        anchors.verticalCenter: parent.verticalCenter
                        opacity: 0.5
                    }

                    Rectangle {
                        width: 2
                        height: 6
                        color: accentColor
                        x: 0
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Rectangle {
                        width: 2
                        height: 6
                        color: accentColor
                        x: parent.width - width
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Rectangle {
                        width: 8
                        height: 8
                        radius: 4
                        color: accentColor
                        border.width: 2
                        x: {
                            var currentFov = viewModel ? viewModel.currentFov : 45.0;
                            // Dynamic FOV limits based on camera type
                            var isDayCamera = viewModel && viewModel.cameraText.includes("DAY");
                            var minFov = isDayCamera ? 2.3 : 5.2;   // Day: 2.3°, Night: 5.2°
                            var maxFov = isDayCamera ? 63.7 : 10.4;  // Day: 63.7°, Night: 10.4°
                            var normalized = (currentFov - minFov) / (maxFov - minFov);
                            return normalized * (parent.width - width);
                        }
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                Text {
                    text: "WFOV"
                    font.pixelSize: 11
                    font.family: primaryFont
                    color: accentColor
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            Rectangle { width: 2; height: 20; color: accentColor }

            // LRF RANGE
            Text {
                text: viewModel ? viewModel.lrfText : "LRF: --- m"
                font.pixelSize: 18
                font.bold: true
                font.family: primaryFont
                color: accentColor
                anchors.verticalCenter: parent.verticalCenter
            }

            Rectangle { width: 2; height: 20; color: accentColor }

            // SYSTEM STATUS
            Text {
                text: viewModel ? viewModel.statusText : "SYS: --- SAF NRD"
                font.pixelSize: 14
                font.family: primaryFont
                color: accentColor
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }

    // ========================================================================
    // AZIMUTH INDICATOR (Top-Right, Circular Compass)
    // ========================================================================
    /*AzimuthIndicator {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.rightMargin: 30
        anchors.topMargin: 200
        width: 100
        height: 100

        azimuth: viewModel ? viewModel.azimuth : 0
        vehicleHeading: viewModel ? viewModel.vehicleHeading : 0
        imuConnected: viewModel ? viewModel.imuConnected : false

        color: accentColor
        relativeColor: "yellow"
    }*/

    // ========================================================================
    // ELEVATION SCALE (Right Side, Vertical Scale)
    // ========================================================================
    /*ElevationScale {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.rightMargin: 20
        anchors.topMargin: 320
        height: 120

        elevation: viewModel ? viewModel.elevation : 0
        color: accentColor
    }*/

    // ========================================================================
    // HELPER FUNCTIONS
    // ========================================================================
    function getCardinalDirection(bearing) {
        var directions = ["N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
                         "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"];
        var index = Math.round(bearing / 22.5) % 16;
        return directions[index];
    }

    function getGunArmedStatus() {
        // Extract "ARM" or "SAF" from statusText
        if (!viewModel) return false;
        return viewModel.statusText.includes("ARM");
    }

    /* UNCOMMENT WHEN DEVICE HEALTH PROPERTIES ARE ADDED TO VIEWMODEL
    function hasDeviceFaults() {
        if (!viewModel) return false;
        return !viewModel.dayCameraConnected ||
               !viewModel.nightCameraConnected ||
               viewModel.azServoFault ||
               viewModel.elServoFault ||
               viewModel.lrfFault ||
               viewModel.lrfOverTemp ||
               viewModel.actuatorFault ||
               !viewModel.plc21Connected ||
               !viewModel.plc42Connected;
    }
    */
}
