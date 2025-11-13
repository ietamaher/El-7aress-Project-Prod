import QtQuick
import QtQuick.Controls
import QtQuick.Effects

Rectangle {
    id: environmentalRoot
    width: 500
    height: 400
    radius: 8
    color: "#0A0A0A"

    border.width: 1
    border.color: "#1A1A1A"
    visible: environmentalViewModel ? environmentalViewModel.visible : false
    property color accentColor: environmentalViewModel ? environmentalViewModel.accentColor : "#46E2A5"

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
                    text: environmentalViewModel ? environmentalViewModel.title : "Environmental Settings"
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
                spacing: 10
                width: parent.width - 60

                Text {
                    text: environmentalViewModel ? environmentalViewModel.instruction : ""
                    font.pixelSize: 14
                    font.family: "Segoe UI"
                    color: "#CCCCCC"
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: environmentalRoot.width - 80
                }

                // Parameter display
                Rectangle {
                    width: parent.width
                    height: 120
                    color: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.05)
                    radius: 5
                    border.color: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.3)
                    border.width: 1
                    visible: environmentalViewModel ? environmentalViewModel.showParameters : false

                    Column {
                        anchors.centerIn: parent
                        spacing: 20

                        Text {
                            text: environmentalViewModel ? environmentalViewModel.parameterLabel : ""
                            font.pixelSize: 20
                            font.weight: Font.Bold
                            font.family: "Segoe UI"
                            color: accentColor
                            anchors.horizontalCenter: parent.horizontalCenter
                        }

                        // Visual indicator based on parameter type
                        Rectangle {
                            width: 300
                            height: 8
                            radius: 4
                            anchors.horizontalCenter: parent.horizontalCenter
                            color: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.15)
                            visible: environmentalViewModel && environmentalViewModel.showParameters

                            Rectangle {
                                height: parent.height
                                radius: 4
                                color: accentColor
                                // Dynamic width based on current parameter (temperature normalized to 0-100°C range)
                                width: {
                                    if (!environmentalViewModel) return 0;
                                    var value = 0;
                                    var label = environmentalViewModel.parameterLabel;
                                    if (label.includes("Temperature")) {
                                        // Normalize -40°C to +60°C as 0 to 100%
                                        value = (environmentalViewModel.temperature + 40) / 100.0;
                                    } else if (label.includes("Altitude")) {
                                        // Normalize -100m to 5000m as 0 to 100%
                                        value = (environmentalViewModel.altitude + 100) / 5100.0;
                                    } else if (label.includes("Crosswind")) {
                                        // Normalize 0 to 25 m/s as 0 to 100%
                                        value = environmentalViewModel.crosswind / 25.0;
                                    }
                                    return Math.max(0, Math.min(1, value)) * parent.width;
                                }

                                Behavior on width {
                                    NumberAnimation { duration: 200 }
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
                text: "Use UP/DOWN to adjust • MENU/VAL to confirm"
                font.pixelSize: 12
                font.family: "Segoe UI"
                color: "#606060"
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }
}
