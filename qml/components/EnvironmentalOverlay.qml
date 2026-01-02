import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import "../common" as Common

Rectangle {
    id: environmentalRoot
    width: Common.OverlayTheme.panelWidth
    height: Common.OverlayTheme.panelHeightLarge
    radius: Common.OverlayTheme.panelRadius
    color: Common.OverlayTheme.backgroundColor

    border.width: Common.OverlayTheme.panelBorderWidth
    border.color: Common.OverlayTheme.borderColor
    visible: environmentalViewModel ? environmentalViewModel.visible : false
    property color accentColor: environmentalViewModel ? environmentalViewModel.accentColor : Common.OverlayTheme.accentDefault

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
                    text: environmentalViewModel ? environmentalViewModel.title : "Environmental Settings"
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
                    text: environmentalViewModel ? environmentalViewModel.instruction : ""
                    font.pixelSize: Common.OverlayTheme.fontSizeBody
                    font.family: Common.OverlayTheme.fontFamily
                    color: Common.OverlayTheme.textSecondary
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: environmentalRoot.width - Common.OverlayTheme.contentPadding
                }

                // Parameter display
                Rectangle {
                    width: parent.width
                    height: 120
                    color: Common.OverlayTheme.contentPanelBg(accentColor)
                    radius: Common.OverlayTheme.contentPanelRadius
                    border.color: Common.OverlayTheme.contentPanelBorder(accentColor)
                    border.width: Common.OverlayTheme.panelBorderWidth
                    visible: environmentalViewModel ? environmentalViewModel.showParameters : false

                    Column {
                        anchors.centerIn: parent
                        spacing: Common.OverlayTheme.dataRowSpacing

                        Text {
                            text: environmentalViewModel ? environmentalViewModel.parameterLabel : ""
                            font.pixelSize: Common.OverlayTheme.fontSizeHeading
                            font.weight: Common.OverlayTheme.fontWeightBold
                            font.family: Common.OverlayTheme.fontFamily
                            color: accentColor
                            anchors.horizontalCenter: parent.horizontalCenter
                        }

                        // Visual indicator based on parameter type
                        Rectangle {
                            width: Common.OverlayTheme.progressBarWidth
                            height: Common.OverlayTheme.progressBarHeight
                            radius: Common.OverlayTheme.progressBarRadius
                            anchors.horizontalCenter: parent.horizontalCenter
                            color: Common.OverlayTheme.progressTrackColor(accentColor)
                            visible: environmentalViewModel && environmentalViewModel.showParameters

                            Rectangle {
                                height: parent.height
                                radius: Common.OverlayTheme.progressBarRadius
                                color: accentColor
                                // Dynamic width based on current parameter
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
                                    }
                                    // NOTE: Crosswind removed - use Windage menu for wind
                                    return Math.max(0, Math.min(1, value)) * parent.width;
                                }

                                Behavior on width {
                                    NumberAnimation { duration: Common.OverlayTheme.animationDuration }
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
                text: "Use UP/DOWN to adjust • MENU/VAL to confirm"
                font.pixelSize: Common.OverlayTheme.fontSizeFooter
                font.family: Common.OverlayTheme.fontFamily
                color: Common.OverlayTheme.textMuted
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }
}
