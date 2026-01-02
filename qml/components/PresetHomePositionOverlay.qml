import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import "../common" as Common

/**
 * PresetHomePositionOverlay.qml
 *
 * UI overlay for the Preset Home Position procedure.
 * Guides the operator through setting the current gimbal position
 * as the home reference point for the motor controller.
 */
Rectangle {
    id: presetHomeRoot
    width: Common.OverlayTheme.panelWidth
    height: Common.OverlayTheme.panelHeightLarge
    radius: Common.OverlayTheme.panelRadius
    color: Common.OverlayTheme.backgroundColor

    border.width: Common.OverlayTheme.panelBorderWidth
    border.color: Common.OverlayTheme.borderColor
    visible: presetHomePositionViewModel ? presetHomePositionViewModel.visible : false
    property color accentColor: presetHomePositionViewModel ? presetHomePositionViewModel.accentColor : Common.OverlayTheme.accentDefault

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
                    text: presetHomePositionViewModel ? presetHomePositionViewModel.title : "Preset Home Position"
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
                spacing: Common.OverlayTheme.contentSpacingLarge
                width: parent.width - (2 * Common.OverlayTheme.contentMargin)

                // Instruction text
                Text {
                    text: presetHomePositionViewModel ? presetHomePositionViewModel.instruction : ""
                    font.pixelSize: Common.OverlayTheme.fontSizeBody
                    font.family: Common.OverlayTheme.fontFamily
                    color: Common.OverlayTheme.textSecondary
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: presetHomeRoot.width - (2 * Common.OverlayTheme.contentMargin)
                    lineHeight: 1.3
                }

                // Status text
                Text {
                    width: parent.width
                    text: presetHomePositionViewModel ? presetHomePositionViewModel.status : ""
                    font.pixelSize: Common.OverlayTheme.fontSizeStatusLarge
                    font.weight: Common.OverlayTheme.fontWeightDemiBold
                    font.family: Common.OverlayTheme.fontFamily
                    color: accentColor
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                }

                // Position Data Panel
                Rectangle {
                    width: parent.width
                    height: 100
                    color: Common.OverlayTheme.contentPanelSolidBg
                    radius: Common.OverlayTheme.contentPanelRadius
                    border.color: Common.OverlayTheme.contentPanelBorder(accentColor)
                    border.width: Common.OverlayTheme.panelBorderWidth

                    Column {
                        anchors.centerIn: parent
                        spacing: 12

                        // Current Azimuth
                        Row {
                            spacing: 15
                            anchors.horizontalCenter: parent.horizontalCenter

                            Text {
                                text: "Azimuth:"
                                font.pixelSize: Common.OverlayTheme.fontSizeLabel
                                font.family: Common.OverlayTheme.fontFamily
                                color: Common.OverlayTheme.textTertiary
                                width: 100
                                horizontalAlignment: Text.AlignRight
                            }

                            Rectangle {
                                width: Common.OverlayTheme.valueBoxWidthLarge
                                height: Common.OverlayTheme.valueBoxHeightLarge
                                color: Common.OverlayTheme.valueBoxBg(accentColor)
                                border.color: Common.OverlayTheme.valueBoxBorder(accentColor)
                                border.width: Common.OverlayTheme.panelBorderWidth
                                radius: Common.OverlayTheme.valueBoxRadius

                                Text {
                                    anchors.centerIn: parent
                                    text: {
                                        if (presetHomePositionViewModel && presetHomePositionViewModel.currentAzimuthDeg !== undefined) {
                                            return presetHomePositionViewModel.currentAzimuthDeg.toFixed(2) + " deg"
                                        }
                                        return "0.00 deg"
                                    }
                                    font.pixelSize: Common.OverlayTheme.fontSizeBody
                                    font.weight: Common.OverlayTheme.fontWeightBold
                                    font.family: Common.OverlayTheme.fontFamilyMono
                                    color: Common.OverlayTheme.textPrimary
                                }
                            }
                        }

                        Rectangle {
                            width: 280
                            height: Common.OverlayTheme.separatorHeight
                            color: Common.OverlayTheme.dividerLight
                            anchors.horizontalCenter: parent.horizontalCenter
                        }

                        // Current Elevation
                        Row {
                            spacing: 15
                            anchors.horizontalCenter: parent.horizontalCenter

                            Text {
                                text: "Elevation:"
                                font.pixelSize: Common.OverlayTheme.fontSizeLabel
                                font.family: Common.OverlayTheme.fontFamily
                                color: Common.OverlayTheme.textTertiary
                                width: 100
                                horizontalAlignment: Text.AlignRight
                            }

                            Rectangle {
                                width: Common.OverlayTheme.valueBoxWidthLarge
                                height: Common.OverlayTheme.valueBoxHeightLarge
                                color: Common.OverlayTheme.valueBoxBg(accentColor)
                                border.color: Common.OverlayTheme.valueBoxBorder(accentColor)
                                border.width: Common.OverlayTheme.panelBorderWidth
                                radius: Common.OverlayTheme.valueBoxRadius

                                Text {
                                    anchors.centerIn: parent
                                    text: {
                                        if (presetHomePositionViewModel && presetHomePositionViewModel.currentElevationDeg !== undefined) {
                                            return presetHomePositionViewModel.currentElevationDeg.toFixed(2) + " deg"
                                        }
                                        return "0.00 deg"
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
                text: "Press MENU/VAL to confirm  |  Use JOYSTICK to position"
                font.pixelSize: Common.OverlayTheme.fontSizeFooter
                font.family: Common.OverlayTheme.fontFamily
                color: Common.OverlayTheme.textMuted
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }
}
