import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import "../common" as Common

Rectangle {
    id: zeroingRoot
    width: Common.OverlayTheme.panelWidth
    height: Common.OverlayTheme.panelHeightLarge
    radius: Common.OverlayTheme.panelRadius
    color: Common.OverlayTheme.backgroundColor

    border.width: Common.OverlayTheme.panelBorderWidth
    border.color: Common.OverlayTheme.borderColor
    visible: zeroingViewModel ? zeroingViewModel.visible : false
    property color accentColor: zeroingViewModel ? zeroingViewModel.accentColor : Common.OverlayTheme.accentDefault

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
                    text: zeroingViewModel ? zeroingViewModel.title : "Weapon Zeroing"
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
                    text: zeroingViewModel ? zeroingViewModel.instruction : ""
                    font.pixelSize: Common.OverlayTheme.fontSizeBody
                    font.family: Common.OverlayTheme.fontFamily
                    color: Common.OverlayTheme.textSecondary
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: zeroingRoot.width - Common.OverlayTheme.contentPadding
                }

                // Status text
                Text {
                    width: parent.width
                    text: zeroingViewModel ? zeroingViewModel.status : ""
                    font.pixelSize: Common.OverlayTheme.fontSizeStatus
                    font.weight: Common.OverlayTheme.fontWeightDemiBold
                    font.family: Common.OverlayTheme.fontFamily
                    color: accentColor
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                }

                // Offset values panel
                Rectangle {
                    width: parent.width
                    height: 120
                    color: Common.OverlayTheme.contentPanelSolidBg
                    radius: Common.OverlayTheme.contentPanelRadius
                    border.color: Common.OverlayTheme.contentPanelBorder(accentColor)
                    border.width: Common.OverlayTheme.panelBorderWidth
                    visible: zeroingViewModel ? zeroingViewModel.showOffsets : false

                    Column {
                        anchors.centerIn: parent
                        spacing: Common.OverlayTheme.dataRowSpacing

                        // Azimuth Row
                        Row {
                            spacing: Common.OverlayTheme.dataRowSpacing
                            anchors.horizontalCenter: parent.horizontalCenter

                            Text {
                                text: "Azimuth Offset:"
                                font.pixelSize: Common.OverlayTheme.fontSizeBody
                                font.family: Common.OverlayTheme.fontFamily
                                color: Common.OverlayTheme.textTertiary
                                width: Common.OverlayTheme.labelWidth
                                horizontalAlignment: Text.AlignRight
                            }

                            Rectangle {
                                width: Common.OverlayTheme.valueBoxWidth
                                height: Common.OverlayTheme.valueBoxHeight
                                color: Common.OverlayTheme.valueBoxBg(accentColor)
                                border.color: Common.OverlayTheme.valueBoxBorder(accentColor)
                                border.width: Common.OverlayTheme.panelBorderWidth
                                radius: Common.OverlayTheme.valueBoxRadius

                                Text {
                                    anchors.centerIn: parent
                                    text: {
                                        if (zeroingViewModel && zeroingViewModel.azimuthOffset !== undefined) {
                                            return zeroingViewModel.azimuthOffset.toFixed(2) + "°"
                                        }
                                        return "0.00°"
                                    }
                                    font.pixelSize: Common.OverlayTheme.fontSizeBody
                                    font.weight: Common.OverlayTheme.fontWeightBold
                                    font.family: Common.OverlayTheme.fontFamilyMono
                                    color: Common.OverlayTheme.textPrimary
                                }
                            }
                        }

                        Rectangle {
                            width: 250
                            height: Common.OverlayTheme.separatorHeight
                            color: Common.OverlayTheme.dividerLight
                            anchors.horizontalCenter: parent.horizontalCenter
                        }

                        // Elevation Row
                        Row {
                            spacing: Common.OverlayTheme.dataRowSpacing
                            anchors.horizontalCenter: parent.horizontalCenter

                            Text {
                                text: "Elevation Offset:"
                                font.pixelSize: Common.OverlayTheme.fontSizeBody
                                font.family: Common.OverlayTheme.fontFamily
                                color: Common.OverlayTheme.textTertiary
                                width: Common.OverlayTheme.labelWidth
                                horizontalAlignment: Text.AlignRight
                            }

                            Rectangle {
                                width: Common.OverlayTheme.valueBoxWidth
                                height: Common.OverlayTheme.valueBoxHeight
                                color: Common.OverlayTheme.valueBoxBg(accentColor)
                                border.color: Common.OverlayTheme.valueBoxBorder(accentColor)
                                border.width: Common.OverlayTheme.panelBorderWidth
                                radius: Common.OverlayTheme.valueBoxRadius

                                Text {
                                    anchors.centerIn: parent
                                    text: {
                                        if (zeroingViewModel && zeroingViewModel.elevationOffset !== undefined) {
                                            return zeroingViewModel.elevationOffset.toFixed(2) + "°"
                                        }
                                        return "0.00°"
                                    }
                                    font.pixelSize: Common.OverlayTheme.fontSizeBody
                                    font.weight: Common.OverlayTheme.fontWeightBold
                                    font.family: Common.OverlayTheme.fontFamilyMono
                                    color: Common.OverlayTheme.textPrimary
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
                text: "Press MENU/VAL to confirm"
                font.pixelSize: Common.OverlayTheme.fontSizeFooter
                font.family: Common.OverlayTheme.fontFamily
                color: Common.OverlayTheme.textMuted
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }
}
