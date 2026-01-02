import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import "../common" as Common

Rectangle {
    id: windageRoot
    width: Common.OverlayTheme.panelWidth
    height: Common.OverlayTheme.panelHeightLarge
    radius: Common.OverlayTheme.panelRadius
    color: Common.OverlayTheme.backgroundColor

    border.width: Common.OverlayTheme.panelBorderWidth
    border.color: Common.OverlayTheme.borderColor
    visible: windageViewModel ? windageViewModel.visible : false
    property color accentColor: windageViewModel ? windageViewModel.accentColor : Common.OverlayTheme.accentDefault

    layer.enabled: Common.OverlayTheme.shadowEnabled
    layer.effect: MultiEffect {
        shadowEnabled: Common.OverlayTheme.shadowEnabled
        shadowColor: Common.OverlayTheme.shadowColor
        shadowBlur: Common.OverlayTheme.shadowBlur
        shadowVerticalOffset: Common.OverlayTheme.shadowVerticalOffset
    }

    Column {
        anchors.fill: parent
        anchors.margins: 0
        spacing: 0

        // Header section
        Rectangle {
            width: parent.width
            height: Common.OverlayTheme.headerHeight
            color: Common.OverlayTheme.headerBackground

            Column {
                anchors.centerIn: parent
                spacing: Common.OverlayTheme.headerSpacing

                Text {
                    text: windageViewModel ? windageViewModel.title : "Windage Setting"
                    font.pixelSize: Common.OverlayTheme.fontSizeTitle
                    font.weight: Common.OverlayTheme.fontWeightNormal
                    font.family: Common.OverlayTheme.fontFamily
                    color: Common.OverlayTheme.textPrimary
                    horizontalAlignment: Text.AlignHCenter
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }
        }

        Rectangle {
            width: parent.width
            height: Common.OverlayTheme.separatorHeight
            color: Common.OverlayTheme.separatorColor
        }

        // Content section
        Item {
            width: parent.width
            height: parent.height - Common.OverlayTheme.headerHeight - Common.OverlayTheme.footerHeight - (2 * Common.OverlayTheme.separatorHeight)

            Column {
                anchors.centerIn: parent
                spacing: Common.OverlayTheme.contentSpacing
                width: parent.width - (2 * Common.OverlayTheme.contentMargin)

                Text {
                    text: windageViewModel ? windageViewModel.instruction : ""
                    font.pixelSize: Common.OverlayTheme.fontSizeBody
                    font.family: Common.OverlayTheme.fontFamily
                    color: Common.OverlayTheme.textSecondary
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: windageRoot.width - Common.OverlayTheme.contentPadding
                }

                // Wind direction display
                Rectangle {
                    width: parent.width
                    height: 60
                    color: Common.OverlayTheme.contentPanelBg(accentColor)
                    radius: Common.OverlayTheme.contentPanelRadius
                    border.color: Common.OverlayTheme.contentPanelBorder(accentColor)
                    border.width: Common.OverlayTheme.panelBorderWidth
                    visible: windageViewModel ? windageViewModel.showWindDirection : false

                    Text {
                        anchors.centerIn: parent
                        text: windageViewModel ? windageViewModel.windDirectionLabel : "Wind FROM: 0°"
                        font.pixelSize: Common.OverlayTheme.fontSizeStatus
                        font.weight: Common.OverlayTheme.fontWeightBold
                        font.family: Common.OverlayTheme.fontFamily
                        color: accentColor
                    }
                }

                // Wind speed display
                Rectangle {
                    width: parent.width
                    height: 120
                    color: Common.OverlayTheme.contentPanelBg(accentColor)
                    radius: Common.OverlayTheme.contentPanelRadius
                    border.color: Common.OverlayTheme.contentPanelBorder(accentColor)
                    border.width: Common.OverlayTheme.panelBorderWidth
                    visible: windageViewModel ? windageViewModel.showWindSpeed : false

                    Column {
                        anchors.centerIn: parent
                        spacing: Common.OverlayTheme.dataRowSpacing

                        Text {
                            text: windageViewModel ? windageViewModel.windSpeedLabel : "Headwind: 0 knots"
                            font.pixelSize: Common.OverlayTheme.fontSizeHeading
                            font.weight: Common.OverlayTheme.fontWeightBold
                            font.family: Common.OverlayTheme.fontFamily
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
                                        NumberAnimation { duration: Common.OverlayTheme.animationDuration }
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
            height: Common.OverlayTheme.separatorHeight
            color: Common.OverlayTheme.separatorColor
        }

        Rectangle {
            width: parent.width
            height: Common.OverlayTheme.footerHeight
            color: Common.OverlayTheme.footerBackground

            Text {
                anchors.centerIn: parent
                text: windageViewModel && windageViewModel.showWindSpeed ?
                      "Use UP/DOWN to adjust • MENU/VAL to confirm" :
                      "Press MENU/VAL when aligned"
                font.pixelSize: Common.OverlayTheme.fontSizeFooter
                font.family: Common.OverlayTheme.fontFamily
                color: Common.OverlayTheme.textMuted
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }
}
