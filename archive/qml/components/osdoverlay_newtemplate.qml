import QtQuick
import QtQuick.Window

Window {
    id: mainWindow_2
    visible: true
    width: 1024
    height: 768
    title: "RCWS OSD - Enhanced Combat Display"
    color: "black"

    // ========================================================================
    // HARDCODED VALUES FOR MOCKUP - CHANGE THESE TO TEST SCENARIOS
    // ========================================================================
    readonly property color accentColor: "#46E2A5"  // Green
    readonly property color warningColor: "#C81428"  // Red
    readonly property color cautionColor: "#FFA500" // Orange
    
    // === OPERATIONAL STATE ===
    property string currentMode: "OBS"  // IDLE, OBS, TRACKING, ENGAGE, EMERGENCY
    readonly property bool isGunArmed: true
    readonly property bool ammoLoaded: true
    
    // === WARNINGS & ZONES ===
    readonly property bool inNoFireZone: true
    readonly property bool inNoTraverse: true
    readonly property bool startupMessage: true
    
    // === PROCEDURES ===
    readonly property bool zeroingActive: true
    readonly property bool windageActive: true
    readonly property bool lacActive: true
    readonly property bool scanActive: true // OR TRP, MANUAL, RADAR
    readonly property bool detectionActive: true

    
    // === SENSORS ===
    readonly property bool imuConnected: false
    readonly property bool dayCameraConnected: true
    readonly property bool nightCameraConnected: false
    readonly property bool azServoFault: true
    readonly property bool elServoFault: false
    readonly property bool lrfFault: true
    readonly property bool lrfOverTemp: true
    
    // === TRACKING ===
    readonly property bool trackingActive: true
    readonly property real trackingConfidence: 0.85  // 0.0 to 1.0
    
    // === POSITIONING ===
    readonly property real gimbalAzimuth: 45.3      // Relative to vehicle
    readonly property real vehicleHeading: 240.0    // Vehicle compass heading
    readonly property real gimbalElevation: 15.2
    
    // === SENSORS VALUES ===
    readonly property real fov: 25.0                // 2.8° to 68°
    readonly property real lrfRange: 2450.0         // meters
    readonly property real rangeRate: -1.2          // m/s (negative = closing)
    readonly property real windSpeed: 12.0          // knots
    
    // === CALCULATED VALUES ===
    readonly property real trueBearing: {
        if (!imuConnected) return gimbalAzimuth;
        var bearing = gimbalAzimuth + vehicleHeading;
        while (bearing >= 360.0) bearing -= 360.0;
        while (bearing < 0.0) bearing += 360.0;
        return bearing;
    }

    // ========================================================================
    // BACKGROUND VIDEO (Simulated)
    // ========================================================================
    Image {
        id: backgroundImage
        anchors.fill: parent
        source: "file:////home/rapit/Documents/sea_scene.jpeg"  // ← Change this path
        fillMode: Image.PreserveAspectCrop  // Fills entire window, crops if needed

         z: 0  // Keep at bottom layer

        // Optional: Add a dark overlay for better OSD visibility
        Rectangle {
            anchors.fill: parent
            color: "black"
            opacity: 0.3  // 30% dark overlay - adjust as needed (0.0 to 1.0)
        }
    }

    // ========================================================================
    // TOP LEFT - STATUS BLOCK (Below mode box) - FIXED ALIGNMENT
    // ========================================================================
    Rectangle {
        id: statusBlock
        x: 10
        y: 10
        width: 380
        height: 95
        color: "#CC000000"
        border.color: accentColor
        border.width: 1

        Row {
            x: 8
            y: 8
            spacing: 20  // Space between left and right columns

            // === LEFT COLUMN ===
            Column {
                spacing: 4

                // MODE
                Text {
                    text: "MODE: " + getModeText(currentMode)
                    font.pixelSize: 15
                    font.bold: true
                    font.family: "Archivo Narrow"
                    color: accentColor
                }

                // RATE
                Text {
                    text: "RATE: SINGLE SHOT"
                    font.pixelSize: 13
                    font.family: "Archivo Narrow"
                    color: accentColor
                }

                // Procedures row (only visible when active)
                Row {
                    spacing: 15
                    visible: zeroingActive || windageActive || lacActive

                    Text {
                        visible: zeroingActive
                        text: "[Z] ZEROING"
                        font.pixelSize: 12
                        font.family: "Archivo Narrow"
                        color: accentColor
                    }

                    Text {
                        visible: windageActive
                        text: "[W: " + windSpeed.toFixed(0) + " kt]"
                        font.pixelSize: 12
                        font.family: "Archivo Narrow"
                        color: accentColor
                    }

                    Text {
                        visible: lacActive
                        text: "LAC: ON"
                        font.pixelSize: 12
                        font.family: "Archivo Narrow"
                        color: "yellow"
                    }
                }

                // DETECTION
                Text {
                    visible: detectionActive
                    text: "DETECTION: ON"
                    font.pixelSize: 12
                    font.family: "Archivo Narrow"
                    color: accentColor
                }
            }

            // === RIGHT COLUMN ===
            Column {
                spacing: 4

                // MOTION
                Text {
                    text: "MOTION: MAN"  // MAN, SCAN, TRP, RADAR, etc.
                    font.pixelSize: 13
                    font.family: "Archivo Narrow"
                    font.bold: true
                    color: accentColor
                }

                // SCAN INFO (directly under MOTION)
                Text {
                    visible: scanActive
                    text: "[SCAN: SECTOR 1]"
                    font.pixelSize: 12
                    font.family: "Archivo Narrow"
                    color: "cyan"
                }
            }
        }
    }

    // ========================================================================
    // TOP RIGHT - AZIMUTH DISPLAY
    // ========================================================================
    Rectangle {
        id: azimuthBox
        x: parent.width - 150
        y: 10
        width: 140
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
                font.family: "Archivo Narrow"
                color: accentColor
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Text {
                visible: imuConnected
                text: "(" + gimbalAzimuth.toFixed(1) + " REL)"
                font.pixelSize: 11
                font.family: "Archivo Narrow"
                color: "yellow"
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Text {
                text: getCardinalDirection(trueBearing)
                font.pixelSize: 16
                font.bold: true
                font.family: "Archivo Narrow"
                color: accentColor
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Text {
                visible: !imuConnected
                text: "NO IMU"
                font.pixelSize: 10
                font.bold: true
                font.family: "Archivo Narrow"
                color: warningColor
                anchors.horizontalCenter: parent.horizontalCenter
            }
        }
    }

    // ========================================================================
    // TOP RIGHT - GUN ARM STATUS BOX
    // ========================================================================
    Rectangle {
        id: gunStatusBox
        x: parent.width - 150
        y: 105
        width: 140
        height: 45
        color: isGunArmed ? warningColor : "#555555"
        border.color: "white"
        border.width: 3

        Text {
            anchors.centerIn: parent
            text: isGunArmed ? "ARMED" : "SAFE"
            font.pixelSize: 18
            font.bold: true
            font.family: "Archivo Narrow"
            color: "white"
        }

        SequentialAnimation on opacity {
            running: isGunArmed
            loops: Animation.Infinite
            NumberAnimation { from: 1.0; to: 0.6; duration: 700 }
            NumberAnimation { from: 0.6; to: 1.0; duration: 700 }
        }
    }


    // ========================================================================
    // RIGHT SIDE - SYSTEM HEALTH WARNINGS (Only when faults)
    // ========================================================================
    Column {
        x: parent.width - 150
        y: parent.height - 200
        spacing: 5
        visible: !dayCameraConnected || !nightCameraConnected || 
                 azServoFault || elServoFault || lrfFault || lrfOverTemp

        // Camera warning
        Rectangle {
            visible: !dayCameraConnected || !nightCameraConnected
            width: 140
            height: 28
            color: cautionColor
            border.color: "black"
            border.width: 2
            radius: 2
            
            Text {
                anchors.centerIn: parent
                text: "⚠ CAM FAULT"
                font.pixelSize: 12
                font.bold: true
                color: "black"
            }
        }

        // Servo warning
        Rectangle {
            visible: azServoFault || elServoFault
            width: 140
            height: 28
            color: cautionColor
            border.color: "black"
            border.width: 2
            radius: 2
            
            Text {
                anchors.centerIn: parent
                text: "⚠ SERVO FAULT"
                font.pixelSize: 12
                font.bold: true
                color: "black"
            }
        }

        // LRF warning
        Rectangle {
            visible: lrfFault || lrfOverTemp
            width: 140
            height: 28
            color: cautionColor
            border.color: "black"
            border.width: 2
            radius: 2
            
            Text {
                anchors.centerIn: parent
                text: "⚠ LRF FAULT"
                font.pixelSize: 12
                font.bold: true
                color: "black"
            }
        }
    }

    // ========================================================================
    // RIGHT SIDE - ELEVATION DISPLAY
    // ========================================================================

    Rectangle {
        id: elevationBox
        x: parent.width - 90
        y: parent.height/2 - 45
        width: 80
        height: 90
        color: "#99000000"
        border.color: accentColor
        border.width: 1

        Column {
            anchors.centerIn: parent
            spacing: 8

            Text {
                text: gimbalElevation.toFixed(1) + "°"
                font.pixelSize: 20
                font.bold: true
                font.family: "Archivo Narrow"
                color: accentColor
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Text {
                text: "ELEV"
                font.pixelSize: 11
                font.family: "Archivo Narrow"
                color: accentColor
                anchors.horizontalCenter: parent.horizontalCenter
            }
        }
    }

    // ========================================================================
    // CENTER - RETICLE (BoxCrosshair style)
    // ========================================================================
    // for better visibility include :
    // lineThickness: 2
    // outlineThickness: 4
    Item {
        id: reticle
        x: parent.width/2
        y: parent.height/2
        width: 60
        height: 60


        // Horizontal line
        Rectangle {
            width: 60
            height: 2
            color: accentColor
            anchors.centerIn: parent

        }

        // Vertical line
        Rectangle {
            width: 2
            height: 60
            color: accentColor
            anchors.centerIn: parent
        }

        // Corner markers
        Repeater {
            model: [
                {x: -25, y: -25, w: 12, h: 2},
                {x: -25, y: -25, w: 2, h: 12},
                {x: 13, y: -25, w: 12, h: 2},
                {x: 23, y: -25, w: 2, h: 12},
                {x: -25, y: 23, w: 12, h: 2},
                {x: -25, y: 13, w: 2, h: 12},
                {x: 13, y: 23, w: 12, h: 2},
                {x: 23, y: 13, w: 2, h: 12}
            ]
            
            Rectangle {
                x: modelData.x
                y: modelData.y
                width: modelData.w
                height: modelData.h
                color: accentColor
            }
        }

        // Center dot
        Rectangle {
            width: 4
            height: 4
            radius: 2
            color: accentColor
            anchors.centerIn: parent
        }
    }

    // ========================================================================
    // CENTER - NO FIRE ZONE WARNING
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
        visible: inNoFireZone
        z: 200

        Text {
            anchors.centerIn: parent
            text: "⚠ NO FIRE ZONE ⚠\nWEAPON INHIBITED"
            font.pixelSize: 20
            font.bold: true
            font.family: "Archivo Narrow"
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
    // CENTER - NO TRAVERSE WARNING
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
        visible: inNoTraverse
        z: 199

        Text {
            anchors.centerIn: parent
            text: "⚠ NO TRAVERSE ZONE"
            font.pixelSize: 18
            font.bold: true
            font.family: "Archivo Narrow"
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
        visible: startupMessage
        z: 100

        Text {
            anchors.centerIn: parent
            text: "SYSTEM INITIALIZATION..."
            font.pixelSize: 16
            font.family: "Archivo Narrow"
            color: accentColor
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
                text: "STAB: OFF"
                font.pixelSize: 13
                font.family: "Archivo Narrow"
                color: accentColor
                anchors.verticalCenter: parent.verticalCenter
            }

            Rectangle { width: 2; height: 20; color: accentColor }

            // CAM
            Text {
                text: "CAM: DAY"
                font.pixelSize: 13
                font.family: "Archivo Narrow"
                color: accentColor
                anchors.verticalCenter: parent.verticalCenter
            }

            Rectangle { width: 2; height: 20; color: accentColor }

            // ========================================================================
            // HORIZONTAL FOV SLIDER - Standalone Component (Place in bottom bar)
            // ========================================================================
            Row {
                spacing: 5
                anchors.verticalCenter: parent.verticalCenter

                Text {
                    text: "FOV: " + fov.toFixed(1) + "°"
                    font.pixelSize: 13
                    font.family: "Archivo Narrow"
                    color: accentColor
                    anchors.verticalCenter: parent.verticalCenter
                }

                Text {
                    text: "NFOV"
                    font.pixelSize: 10
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
                        //border.color: "white"
                        border.width: 2
                        x: {
                            var minFov = 2.8;
                            var maxFov = 68.0;
                            var normalized = (fov - minFov) / (maxFov - minFov);
                            return normalized * (parent.width - width);
                        }
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                Text {
                    text: "WFOV"
                    font.pixelSize: 10
                    color: accentColor
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            Rectangle { width: 2; height: 20; color: accentColor }

            // LRF + RANGE RATE (Enhanced)
            Column {
                spacing: 2
                anchors.verticalCenter: parent.verticalCenter

                Row {
                    spacing: 8
                    
                    Text {
                        text: lrfRange > 0 ? lrfRange.toFixed(0) + " m" : "--- m"
                        font.pixelSize: lrfRange > 0 ? 18 : 13
                        font.bold: lrfRange > 0
                        font.family: "Archivo Narrow"
                        color: lrfRange > 0 ? "yellow" : accentColor
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    // Range rate (closing/opening)
                    Text {
                        visible: trackingActive && Math.abs(rangeRate) > 0.1
                        text: rangeRate < 0 ? "↓" + Math.abs(rangeRate).toFixed(1) + " m/s" 
                                            : "↑" + Math.abs(rangeRate).toFixed(1) + " m/s"
                        font.pixelSize: 11
                        font.bold: true
                        font.family: "Archivo Narrow"
                        color: rangeRate < 0 ? "#44FF44" : "#FF4444"  // Green closing, Red opening
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }

            Rectangle { width: 2; height: 20; color: accentColor }

            // AMMO STATUS (NEW!)
            Row {
                spacing: 5
                anchors.verticalCenter: parent.verticalCenter
                
                Rectangle {
                    width: 18
                    height: 18
                    radius: 9
                    color: ammoLoaded ? "#00FF00" : "#FF0000"
                    border.color: "white"
                    border.width: 2
                    anchors.verticalCenter: parent.verticalCenter
                }
                
                Text {
                    text: ammoLoaded ? "AMMO" : "EMPTY"
                    font.pixelSize: 12
                    font.bold: true
                    font.family: "Archivo Narrow"
                    color: ammoLoaded ? "#00FF00" : "#FF0000"
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            Rectangle { width: 2; height: 20; color: accentColor }

            // TRK
            Text {
                text: trackingActive ? "TRK: ACTIVE" : "TRK: OFF"
                font.pixelSize: 13
                font.family: "Archivo Narrow"
                color: trackingActive ? "yellow" : accentColor
                anchors.verticalCenter: parent.verticalCenter
            }

            Text {
                text: "CONF: " + (trackingConfidence * 100).toFixed(0) + "%"
                font.pixelSize: 13
                font.family: "Archivo Narrow"
                color: trackingActive ? "yellow" : accentColor
                anchors.verticalCenter: parent.verticalCenter
            }

        }
    }

    // ========================================================================
    // DEBUG/TEST PANEL (Bottom left - remove in production)
    // ========================================================================
    /*Rectangle {
        x: 10
        y: parent.height - 180
        width: 300
        height: 130
        color: "#DD000000"
        border.color: "cyan"
        border.width: 1
        radius: 3
        visible: true  // Set to false when satisfied

        Column {
            x: 8
            y: 8
            spacing: 3

            Text {
                text: "🔧 TEST SCENARIOS (Change at top):"
                font.pixelSize: 11
                font.bold: true
                color: "cyan"
            }
            Text {
                text: "• Mode: " + currentMode
                font.pixelSize: 9
                color: getModeColor(currentMode)
            }
            Text {
                text: "• Armed: " + isGunArmed + " | Ammo: " + ammoLoaded
                font.pixelSize: 9
                color: "white"
            }
            Text {
                text: "• NoFire: " + inNoFireZone + " | NoTraverse: " + inNoTraverse
                font.pixelSize: 9
                color: "white"
            }
            Text {
                text: "• FOV: " + fov.toFixed(1) + "° (" + getFovZone(fov) + ")"
                font.pixelSize: 9
                color: getFovColor(fov)
            }
            Text {
                text: "• Tracking: " + trackingActive + " | Conf: " + (trackingConfidence*100).toFixed(0) + "%"
                font.pixelSize: 9
                color: "white"
            }
            Text {
                text: "• Range: " + lrfRange.toFixed(0) + "m | Rate: " + rangeRate.toFixed(1) + " m/s"
                font.pixelSize: 9
                color: "white"
            }
            Text {
                text: "• IMU: " + imuConnected + " | Faults: " + hasFaults()
                font.pixelSize: 9
                color: "white"
            }
        }
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

    function getModeIcon(mode) {
        switch(mode) {
            case "IDLE": return "●";
            case "OBS": return "◉";
            case "TRACKING": return "⊕";
            case "ENGAGE": return "⊗";
            case "EMERGENCY": return "⚠";
            default: return "?";
        }
    }

    function getModeText(mode) {
        switch(mode) {
            case "IDLE": return "IDLE";
            case "OBS": return "OBSERVING";
            case "TRACKING": return "TRACKING";
            case "ENGAGE": return "ENGAGED";
            case "EMERGENCY": return "E-STOP";
            default: return "UNKNOWN";
        }
    }

    function getModeColor(mode) {
        switch(mode) {
            case "IDLE": return "#555555";        // Gray
            case "OBS": return "#0088CC";         // Blue
            case "TRACKING": return "#FFAA00";    // Orange
            case "ENGAGE": return "#CC1428";      // Red
            case "EMERGENCY": return "#FF0000";   // Bright Red
            default: return "#444444";
        }
    }

    function getFovZone(currentFov) {
        if (currentFov >= 40) return "SEARCH";
        if (currentFov >= 15) return "IDENTIFY";
        return "ENGAGE";
    }

    function getFovColor(currentFov) {
        if (currentFov >= 40) return "#00FF00";  // Green - Search
        if (currentFov >= 15) return "#FFFF00";  // Yellow - Identify
        return "#FF0000";  // Red - Engage
    }

    function getConfidenceColor(conf) {
        if (conf >= 0.8) return "#00FF00";  // Green - good lock
        if (conf >= 0.5) return "#FFFF00";  // Yellow - marginal
        return "#FF0000";  // Red - poor lock
    }

    function hasFaults() {
        return !dayCameraConnected || !nightCameraConnected || 
               azServoFault || elServoFault || lrfFault || lrfOverTemp;
    }
}
