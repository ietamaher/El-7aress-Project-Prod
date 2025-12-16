import QtQuick
import QtQuick.Controls
import QtQuick.Effects

/**
 * HomeCalibrationOverlay.qml
 *
 * UI overlay for the Azimuth Home Offset Calibration procedure.
 * Guides the operator through compensating for ABZO encoder drift
 * in the Oriental Motor AZD-KD servo.
 *
 * @author Claude Code
 * @date December 2025
 */
Rectangle {
    id: homeCalibRoot
    width: 550
    height: 450
    radius: 0
    color: "#0A0A0A"

    border.width: 1
    border.color: "#1A1A1A"
    visible: homeCalibrationViewModel ? homeCalibrationViewModel.visible : false
    property color accentColor: homeCalibrationViewModel ? homeCalibrationViewModel.accentColor : "#46E2A5"

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
                    text: homeCalibrationViewModel ? homeCalibrationViewModel.title : "Az Home Calibration"
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
                    text: homeCalibrationViewModel ? homeCalibrationViewModel.instruction : ""
                    font.pixelSize: 13
                    font.family: "Segoe UI"
                    color: "#CCCCCC"
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: homeCalibRoot.width - 60
                    lineHeight: 1.3
                }

                // Status text
                Text {
                    width: parent.width
                    text: homeCalibrationViewModel ? homeCalibrationViewModel.status : ""
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                    font.family: "Segoe UI"
                    color: accentColor
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                }

                // Calibration Data Panel
                Rectangle {
                    width: parent.width
                    height: 130
                    color: "#0F0F0F"
                    radius: 5
                    border.color: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.3)
                    border.width: 1

                    Column {
                        anchors.centerIn: parent
                        spacing: 12

                        // Current Encoder Position
                        Row {
                            spacing: 15
                            anchors.horizontalCenter: parent.horizontalCenter

                            Text {
                                text: "Encoder Position:"
                                font.pixelSize: 13
                                font.family: "Segoe UI"
                                color: "#AAAAAA"
                                width: 130
                                horizontalAlignment: Text.AlignRight
                            }

                            Rectangle {
                                width: 120
                                height: 24
                                color: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.1)
                                border.color: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.2)
                                border.width: 1
                                radius: 3

                                Text {
                                    anchors.centerIn: parent
                                    text: {
                                        if (homeCalibrationViewModel && homeCalibrationViewModel.currentEncoderSteps !== undefined) {
                                            return homeCalibrationViewModel.currentEncoderSteps.toFixed(0) + " steps"
                                        }
                                        return "0 steps"
                                    }
                                    font.pixelSize: 12
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

                        // Current Offset
                        Row {
                            spacing: 15
                            anchors.horizontalCenter: parent.horizontalCenter

                            Text {
                                text: "Home Offset:"
                                font.pixelSize: 13
                                font.family: "Segoe UI"
                                color: "#AAAAAA"
                                width: 130
                                horizontalAlignment: Text.AlignRight
                            }

                            Rectangle {
                                width: 120
                                height: 24
                                color: homeCalibrationViewModel && homeCalibrationViewModel.offsetApplied
                                       ? Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.2)
                                       : Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.1)
                                border.color: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.3)
                                border.width: 1
                                radius: 3

                                Text {
                                    anchors.centerIn: parent
                                    text: {
                                        if (homeCalibrationViewModel && homeCalibrationViewModel.currentOffsetSteps !== undefined) {
                                            return homeCalibrationViewModel.currentOffsetSteps.toFixed(0) + " steps"
                                        }
                                        return "0 steps"
                                    }
                                    font.pixelSize: 12
                                    font.weight: Font.Bold
                                    font.family: "Segoe UI Mono", "Consolas", "Segoe UI"
                                    color: homeCalibrationViewModel && homeCalibrationViewModel.offsetApplied
                                           ? accentColor : "#FFFFFF"
                                }
                            }
                        }

                        Rectangle {
                            width: 280
                            height: 1
                            color: "#252525"
                            anchors.horizontalCenter: parent.horizontalCenter
                        }

                        // Current Azimuth Display
                        Row {
                            spacing: 15
                            anchors.horizontalCenter: parent.horizontalCenter

                            Text {
                                text: "Display Azimuth:"
                                font.pixelSize: 13
                                font.family: "Segoe UI"
                                color: "#AAAAAA"
                                width: 130
                                horizontalAlignment: Text.AlignRight
                            }

                            Rectangle {
                                width: 120
                                height: 24
                                color: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.1)
                                border.color: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.2)
                                border.width: 1
                                radius: 3

                                Text {
                                    anchors.centerIn: parent
                                    text: {
                                        if (homeCalibrationViewModel && homeCalibrationViewModel.currentAzimuthDeg !== undefined) {
                                            return homeCalibrationViewModel.currentAzimuthDeg.toFixed(2) + " deg"
                                        }
                                        return "0.00 deg"
                                    }
                                    font.pixelSize: 12
                                    font.weight: Font.Bold
                                    font.family: "Segoe UI Mono", "Consolas", "Segoe UI"
                                    color: "#FFFFFF"
                                }
                            }
                        }
                    }
                }

                // Offset Applied Indicator
                Rectangle {
                    width: 200
                    height: 28
                    anchors.horizontalCenter: parent.horizontalCenter
                    color: homeCalibrationViewModel && homeCalibrationViewModel.offsetApplied
                           ? Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.2)
                           : "transparent"
                    border.color: homeCalibrationViewModel && homeCalibrationViewModel.offsetApplied
                           ? accentColor : "#333333"
                    border.width: 1
                    radius: 4
                    visible: homeCalibrationViewModel ? homeCalibrationViewModel.offsetApplied : false

                    Row {
                        anchors.centerIn: parent
                        spacing: 8

                        Text {
                            text: "[H]"
                            font.pixelSize: 14
                            font.weight: Font.Bold
                            font.family: "Segoe UI"
                            color: accentColor
                        }

                        Text {
                            text: "HOME OFFSET ACTIVE"
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                            font.family: "Segoe UI"
                            color: accentColor
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
                text: "Press MENU/VAL to confirm  |  Use JOYSTICK to slew"
                font.pixelSize: 12
                font.family: "Segoe UI"
                color: "#606060"
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }
}
