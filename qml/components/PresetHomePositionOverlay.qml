import QtQuick
import QtQuick.Controls
import QtQuick.Effects

/**
 * PresetHomePositionOverlay.qml
 *
 * UI overlay for the Preset Home Position procedure.
 * Guides the operator through setting the current gimbal position
 * as the home reference point for the motor controller.
 *
 * @author Claude Code
 * @date December 2025
 */
Rectangle {
    id: presetHomeRoot
    width: 550
    height: 400
    radius: 0
    color: "#0A0A0A"

    border.width: 1
    border.color: "#1A1A1A"
    visible: presetHomePositionViewModel ? presetHomePositionViewModel.visible : false
    property color accentColor: presetHomePositionViewModel ? presetHomePositionViewModel.accentColor : "#46E2A5"

    layer.enabled: true
    layer.effect: MultiEffect {
        shadowEnabled: true
        shadowColor: "#80000000"
        shadowBlur: 0.5
        shadowVerticalOffset: 10
    }

    Column {
        anchors.fill: parent
        anchors.margins: 0
        spacing: 0

        // Header section
        Rectangle {
            width: parent.width
            height: 80
            color: "transparent"

            Column {
                anchors.centerIn: parent
                spacing: 8

                Text {
                    text: presetHomePositionViewModel ? presetHomePositionViewModel.title : "Preset Home Position"
                    font.pixelSize: 22
                    font.weight: Font.Normal
                    font.family: "Segoe UI"
                    color: "#FFFFFF"
                    horizontalAlignment: Text.AlignHCenter
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }
        }

        Rectangle {
            width: parent.width
            height: 1
            color: "#151515"
        }

        // Content section
        Item {
            width: parent.width
            height: parent.height - 130

            Column {
                anchors.centerIn: parent
                spacing: 15
                width: parent.width - 60

                // Instruction text
                Text {
                    text: presetHomePositionViewModel ? presetHomePositionViewModel.instruction : ""
                    font.pixelSize: 13
                    font.family: "Segoe UI"
                    color: "#CCCCCC"
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: presetHomeRoot.width - 60
                    lineHeight: 1.3
                }

                // Status text
                Text {
                    width: parent.width
                    text: presetHomePositionViewModel ? presetHomePositionViewModel.status : ""
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                    font.family: "Segoe UI"
                    color: accentColor
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                }

                // Position Data Panel
                Rectangle {
                    width: parent.width
                    height: 100
                    color: "#0F0F0F"
                    radius: 5
                    border.color: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.3)
                    border.width: 1

                    Column {
                        anchors.centerIn: parent
                        spacing: 12

                        // Current Azimuth
                        Row {
                            spacing: 15
                            anchors.horizontalCenter: parent.horizontalCenter

                            Text {
                                text: "Azimuth:"
                                font.pixelSize: 13
                                font.family: "Segoe UI"
                                color: "#AAAAAA"
                                width: 100
                                horizontalAlignment: Text.AlignRight
                            }

                            Rectangle {
                                width: 140
                                height: 28
                                color: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.1)
                                border.color: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.2)
                                border.width: 1
                                radius: 3

                                Text {
                                    anchors.centerIn: parent
                                    text: {
                                        if (presetHomePositionViewModel && presetHomePositionViewModel.currentAzimuthDeg !== undefined) {
                                            return presetHomePositionViewModel.currentAzimuthDeg.toFixed(2) + " deg"
                                        }
                                        return "0.00 deg"
                                    }
                                    font.pixelSize: 14
                                    font.weight: Font.Bold
                                    font.family: "Segoe UI Mono", "Consolas", "Segoe UI"
                                    color: "#FFFFFF"
                                }
                            }
                        }

                        Rectangle {
                            width: 280
                            height: 1
                            color: "#252525"
                            anchors.horizontalCenter: parent.horizontalCenter
                        }

                        // Current Elevation
                        Row {
                            spacing: 15
                            anchors.horizontalCenter: parent.horizontalCenter

                            Text {
                                text: "Elevation:"
                                font.pixelSize: 13
                                font.family: "Segoe UI"
                                color: "#AAAAAA"
                                width: 100
                                horizontalAlignment: Text.AlignRight
                            }

                            Rectangle {
                                width: 140
                                height: 28
                                color: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.1)
                                border.color: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.2)
                                border.width: 1
                                radius: 3

                                Text {
                                    anchors.centerIn: parent
                                    text: {
                                        if (presetHomePositionViewModel && presetHomePositionViewModel.currentElevationDeg !== undefined) {
                                            return presetHomePositionViewModel.currentElevationDeg.toFixed(2) + " deg"
                                        }
                                        return "0.00 deg"
                                    }
                                    font.pixelSize: 14
                                    font.weight: Font.Bold
                                    font.family: "Segoe UI Mono", "Consolas", "Segoe UI"
                                    color: "#FFFFFF"
                                }
                            }
                        }
                    }
                }
            }
        }

        // Footer section
        Rectangle {
            width: parent.width
            height: 1
            color: "#151515"
        }

        Rectangle {
            width: parent.width
            height: 50
            color: "#0D0D0D"

            Text {
                anchors.centerIn: parent
                text: "Press MENU/VAL to confirm  |  Use JOYSTICK to position"
                font.pixelSize: 12
                font.family: "Segoe UI"
                color: "#606060"
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }
}
