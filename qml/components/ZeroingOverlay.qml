import QtQuick
import QtQuick.Controls
import QtQuick.Effects

Rectangle {
    id: zeroingRoot
    width: 500
    height: 400
    radius: 0
    color: "#0A0A0A"

    border.width: 1
    border.color: "#1A1A1A"
    visible: zeroingViewModel ? zeroingViewModel.visible : false
    property color accentColor: zeroingViewModel ? zeroingViewModel.accentColor : "#46E2A5"

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
                    text: zeroingViewModel ? zeroingViewModel.title : "Weapon Zeroing"
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
                    text: zeroingViewModel ? zeroingViewModel.instruction : ""
                    font.pixelSize: 14
                    font.family: "Segoe UI"
                    color: "#CCCCCC"
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: zeroingRoot.width - 80
                }
                // Status text
                Text {
                    width: parent.width
                    text: zeroingViewModel ? zeroingViewModel.status : ""
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                    font.family: "Segoe UI"
                    color: accentColor
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                }

                Rectangle {
                    width: parent.width
                    height: 120  // Increased height for better spacing
                    color: "#0F0F0F"  // Darker background for better contrast
                    radius: 5
                    border.color: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.3)
                    border.width: 1
                    visible: zeroingViewModel ? zeroingViewModel.showOffsets : false

                    Column {
                        anchors.centerIn: parent
                        spacing: 20  // Increased spacing

                        // Azimuth Row
                        Row {
                            spacing: 20
                            anchors.horizontalCenter: parent.horizontalCenter

                            Text {
                                text: "Azimuth Offset:"
                                font.pixelSize: 14
                                font.family: "Segoe UI"
                                color: "#AAAAAA"  // Lighter gray for labels
                                width: 120  // Fixed width for alignment
                                horizontalAlignment: Text.AlignRight
                            }

                            Rectangle {
                                width: 100  // Fixed width container for value
                                height: 24
                                color: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.1)
                                border.color: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.2)
                                border.width: 1
                                radius: 3

                                Text {
                                    anchors.centerIn: parent
                                    text: {
                                        if (zeroingViewModel && zeroingViewModel.azimuthOffset !== undefined) {
                                            return zeroingViewModel.azimuthOffset.toFixed(2) + "°"
                                        }
                                        return "0.00°"
                                    }
                                    font.pixelSize: 14
                                    font.weight: Font.Bold
                                    font.family: "Segoe UI Mono", "Consolas", "Segoe UI"  // Monospace for numbers
                                    color: "#FFFFFF"  // White for maximum contrast
                                }
                            }
                        }

                        Rectangle {
                            width: 250
                            height: 1
                            color: "#252525"
                            anchors.horizontalCenter: parent.horizontalCenter
                        }

                        // Elevation Row
                        Row {
                            spacing: 20
                            anchors.horizontalCenter: parent.horizontalCenter

                            Text {
                                text: "Elevation Offset:"
                                font.pixelSize: 14
                                font.family: "Segoe UI"
                                color: "#AAAAAA"  // Lighter gray for labels
                                width: 120  // Fixed width for alignment
                                horizontalAlignment: Text.AlignRight
                            }

                            Rectangle {
                                width: 100  // Fixed width container for value
                                height: 24
                                color: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.1)
                                border.color: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.2)
                                border.width: 1
                                radius: 3

                                Text {
                                    anchors.centerIn: parent
                                    text: {
                                        if (zeroingViewModel && zeroingViewModel.elevationOffset !== undefined) {
                                            return zeroingViewModel.elevationOffset.toFixed(2) + "°"
                                        }
                                        return "0.00°"
                                    }
                                    font.pixelSize: 14
                                    font.weight: Font.Bold
                                    font.family: "Segoe UI Mono", "Consolas", "Segoe UI"  // Monospace for numbers
                                    color: "#FFFFFF"  // White for maximum contrast
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
                text: "Press MENU/VAL to confirm"
                font.pixelSize: 12
                font.family: "Segoe UI"
                color: "#606060"
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }
}
