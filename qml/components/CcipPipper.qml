import QtQuick

/**
 * @brief CCIP (Continuously Computed Impact Point) Pipper
 *
 * Shows where bullets will impact with lead angle compensation.
 * This is SEPARATE from the tracking box which shows visual target position.
 *
 * ARCHITECTURAL PRINCIPLE:
 * • Tracking Box = Where target IS (visual tracking)
 * • CCIP Pipper = Where bullets WILL HIT (ballistic prediction)
 *
 * States:
 * • Cyan Diamond: Valid firing solution
 * • Yellow Diamond: Computing/Lag (lead at limit)
 * • Red Diamond: Zoom Out (FOV too narrow for lead angle)
 * • Hidden: LAC disabled
 */
Item {
    id: root

    // ========================================================================
    // PUBLIC PROPERTIES
    // ========================================================================

    property bool pipperEnabled: false  // External control: set from parent
    property string status: "Off"       // "Off", "On", "Lag", "ZoomOut"
    property color accentColor: "#46E2A5"

    // ========================================================================
    // VISUAL CONFIGURATION
    // ========================================================================

    readonly property int pipperSize: 24
    readonly property int pipperLineWidth: 3
    readonly property int centerDotSize: 6

    // State colors
    readonly property color validColor: "#00FFFF"      // Cyan - good solution
    readonly property color computingColor: "#FFFF00"  // Yellow - computing/lag
    readonly property color invalidColor: "#FF3333"    // Red - zoom out needed

    // Current color based on status
    property color currentColor: {
        switch(status) {
            case "On": return validColor;
            case "Lag": return computingColor;
            case "ZoomOut": return invalidColor;
            default: return accentColor;
        }
    }

    width: pipperSize * 2
    height: pipperSize * 2

    // Visibility: enabled from parent AND has valid status
    visible: root.pipperEnabled && (status === "On" || status === "Lag" || status === "ZoomOut")

    // ========================================================================
    // CCIP PIPPER VISUAL (Diamond Shape)
    // ========================================================================

    // Diamond outline (4 lines forming diamond)
    Canvas {
        id: diamondCanvas
        anchors.fill: parent

        onPaint: {
            var ctx = getContext("2d");
            ctx.clearRect(0, 0, width, height);

            if (!visible) return;

            ctx.strokeStyle = root.currentColor;
            ctx.lineWidth = root.pipperLineWidth;
            ctx.lineCap = "round";
            ctx.lineJoin = "round";

            // Calculate diamond points (centered)
            var centerX = width / 2;
            var centerY = height / 2;
            var size = root.pipperSize;

            // Draw diamond (4 lines)
            ctx.beginPath();
            ctx.moveTo(centerX, centerY - size);        // Top
            ctx.lineTo(centerX + size, centerY);        // Right
            ctx.lineTo(centerX, centerY + size);        // Bottom
            ctx.lineTo(centerX - size, centerY);        // Left
            ctx.closePath();
            ctx.stroke();
        }

        // Redraw when properties change
        Connections {
            target: root
            function onCurrentColorChanged() { diamondCanvas.requestPaint(); }
            function onVisibleChanged() { diamondCanvas.requestPaint(); }
        }
    }

    // Center dot
    Rectangle {
        width: root.centerDotSize
        height: root.centerDotSize
        radius: root.centerDotSize / 2
        color: root.currentColor
        anchors.centerIn: parent
        // No explicit visible binding needed - inherits from parent
    }

    // ========================================================================
    // STATUS INDICATOR (Below pipper)
    // ========================================================================

    Text {
        id: statusText
        anchors.top: parent.bottom
        anchors.topMargin: 4
        anchors.horizontalCenter: parent.horizontalCenter

        text: {
            switch(root.status) {
                case "On": return "CCIP";
                case "Lag": return "CCIP LAG";
                case "ZoomOut": return "ZOOM OUT";
                default: return "";
            }
        }

        color: root.currentColor
        font.family: "Monospace"
        font.pixelSize: 11
        font.bold: true
        // No explicit visible binding needed - inherits from parent

        // Black outline for readability
        style: Text.Outline
        styleColor: "#000000"
    }

    // ========================================================================
    // PULSING ANIMATION (When computing)
    // ========================================================================

    SequentialAnimation {
        running: root.pipperEnabled && (root.status === "Lag" || root.status === "ZoomOut")
        loops: Animation.Infinite

        NumberAnimation {
            target: root
            property: "opacity"
            from: 1.0
            to: 0.6
            duration: 500
            easing.type: Easing.InOutQuad
        }

        NumberAnimation {
            target: root
            property: "opacity"
            from: 0.6
            to: 1.0
            duration: 500
            easing.type: Easing.InOutQuad
        }
    }

    // ========================================================================
    // DEBUG INFO (Comment out for production)
    // ========================================================================

    /*
    Rectangle {
        anchors.top: statusText.bottom
        anchors.topMargin: 2
        anchors.horizontalCenter: parent.horizontalCenter
        width: debugText.width + 8
        height: debugText.height + 4
        color: "#80000000"
        radius: 3
        visible: root.visible

        Text {
            id: debugText
            anchors.centerIn: parent
            text: "Status: " + root.status
            color: "#00FF00"
            font.family: "Monospace"
            font.pixelSize: 9
        }
    }
    */
}
