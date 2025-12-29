import QtQuick
import QtQuick.Controls
import QtQuick.Effects

Rectangle {
    id: brightnessRoot
    width: 500
    height: 350
    radius: 0
    color: "#0A0A0A"

    border.width: 1
    border.color: "#1A1A1A"
    visible: brightnessViewModel ? brightnessViewModel.visible : false
    property color accentColor: brightnessViewModel ? brightnessViewModel.accentColor : "#46E2A5"

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
                    text: brightnessViewModel ? brightnessViewModel.title : "Display Brightness"
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
                    text: brightnessViewModel ? brightnessViewModel.instruction : ""
                    font.pixelSize: 14
                    font.family: "Segoe UI"
                    color: "#CCCCCC"
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: brightnessRoot.width - 80
                }

                // Parameter display
                Rectangle {
                    width: parent.width
                    height: 120
                    color: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.05)
                    radius: 5
                    border.color: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.3)
                    border.width: 1
                    visible: brightnessViewModel ? brightnessViewModel.showParameters : false

                    Column {
                        anchors.centerIn: parent
                        spacing: 20

                        Text {
                            text: brightnessViewModel ? brightnessViewModel.parameterLabel : ""
                            font.pixelSize: 20
                            font.weight: Font.Bold
                            font.family: "Segoe UI"
                            color: accentColor
                            anchors.horizontalCenter: parent.horizontalCenter
                        }

                        // Visual brightness bar
                        Rectangle {
                            width: 300
                            height: 8
                            radius: 4
                            anchors.horizontalCenter: parent.horizontalCenter
                            color: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.15)
                            visible: brightnessViewModel && brightnessViewModel.showParameters

                            Rectangle {
                                height: parent.height
                                radius: 4
                                color: accentColor
                                // Width based on brightness percentage (10-100% mapped to bar width)
                                width: {
                                    if (!brightnessViewModel) return 0;
                                    // Normalize 10% to 100% as bar width
                                    var value = (brightnessViewModel.brightness - 10) / 90.0;
                                    return Math.max(0, Math.min(1, value)) * parent.width;
                                }

                                Behavior on width {
                                    NumberAnimation { duration: 200 }
                                }
                            }
                        }

                        // Brightness level indicators
                        Row {
                            anchors.horizontalCenter: parent.horizontalCenter
                            spacing: 26

                            Repeater {
                                model: 10 // 10%, 20%, ..., 100%

                                Rectangle {
                                    width: 4
                                    height: 12
                                    radius: 2
                                    color: {
                                        if (!brightnessViewModel) return "#333333";
                                        var level = (index + 1) * 10;
                                        return level <= brightnessViewModel.brightness ? accentColor : "#333333";
                                    }

                                    Behavior on color {
                                        ColorAnimation { duration: 150 }
                                    }
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
