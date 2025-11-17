import QtQuick
import QtQuick.Controls
import QtQuick.Effects

Rectangle {
    id: windageRoot
    width: 500
    height: 400
    radius: 8
    color: "#0A0A0A"

    border.width: 1
    border.color: "#1A1A1A"
    visible: windageViewModel ? windageViewModel.visible : false
    property color accentColor: windageViewModel ? windageViewModel.accentColor : "#46E2A5"

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
                    text: windageViewModel ? windageViewModel.title : "Windage Setting"
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
                    text: windageViewModel ? windageViewModel.instruction : ""
                    font.pixelSize: 14
                    font.family: "Segoe UI"
                    color: "#CCCCCC"
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: windageRoot.width - 80
                }

                // Wind direction display
                Rectangle {
                    width: parent.width
                    height: 60
                    color: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.05)
                    radius: 5
                    border.color: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.3)
                    border.width: 1
                    visible: windageViewModel ? windageViewModel.showWindDirection : false

                    Text {
                        anchors.centerIn: parent
                        text: windageViewModel ? windageViewModel.windDirectionLabel : "Wind FROM: 0°"
                        font.pixelSize: 18
                        font.weight: Font.Bold
                        font.family: "Segoe UI"
                        color: accentColor
                    }
                }

                // Wind speed display
                Rectangle {
                    width: parent.width
                    height: 120
                    color: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.05)
                    radius: 5
                    border.color: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.3)
                    border.width: 1
                    visible: windageViewModel ? windageViewModel.showWindSpeed : false

                    Column {
                        anchors.centerIn: parent
                        spacing: 20

                        Text {
                            text: windageViewModel ? windageViewModel.windSpeedLabel : "Headwind: 0 knots"
                            font.pixelSize: 20
                            font.weight: Font.Bold
                            font.family: "Segoe UI"
                            color: accentColor
                            anchors.horizontalCenter: parent.horizontalCenter
                        }

                        // Visual wind indicator bars
                        Row {
                            spacing: 6
                            anchors.horizontalCenter: parent.horizontalCenter

                            Repeater {
                                model: windageViewModel ? Math.min(10, Math.floor(windageViewModel.windSpeed / 5)) : 0

                                Rectangle {
                                    width: 10
                                    height: 28
                                    color: accentColor
                                    radius: 2
                                    opacity: 0.7 + (index * 0.03)

                                    Behavior on opacity {
                                        NumberAnimation { duration: 200 }
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
                text: windageViewModel && windageViewModel.showWindSpeed ?
                      "Use UP/DOWN to adjust • MENU/VAL to confirm" :
                      "Press MENU/VAL when aligned"
                font.pixelSize: 12
                font.family: "Segoe UI"
                color: "#606060"
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }
}
