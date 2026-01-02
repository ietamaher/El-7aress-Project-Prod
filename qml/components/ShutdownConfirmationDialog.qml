import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../common" as Common

/**
 * ShutdownConfirmationDialog - Confirmation dialog before system shutdown
 *
 * Flow:
 * 1. Navigate to "Shutdown System" in main menu
 * 2. Press MENU/VAL to select
 * 3. This dialog appears with YES/NO options
 * 4. UP/DOWN to navigate between options
 * 5. MENU/VAL to confirm selection
 * 6. If YES: Shows "SHUTDOWN COMPLETE" message, then shuts down
 * 7. If NO: Returns to main menu
 */
Rectangle {
    id: root

    // Accent color (from viewModel or fallback)
    property color accentColor: shutdownConfirmationViewModel && shutdownConfirmationViewModel.accentColor ? shutdownConfirmationViewModel.accentColor : Common.OverlayTheme.accentDefault

    // Warning/danger color for shutdown
    property color dangerColor: Common.OverlayTheme.dangerColor

    // Visible when viewModel.visible is true
    visible: shutdownConfirmationViewModel ? shutdownConfirmationViewModel.visible : false
    anchors.fill: parent

    // Semi-transparent dark overlay
    color: Common.OverlayTheme.modalBackdrop

    // Main dialog container
    Rectangle {
        id: dialogBox
        anchors.centerIn: parent
        width: Math.min(parent.width * 0.6, 500)
        height: Math.min(parent.height * 0.5, 350)

        color: Common.OverlayTheme.backgroundColor
        border.color: dangerColor
        border.width: 2
        radius: Common.OverlayTheme.panelRadius

        layer.enabled: Common.OverlayTheme.shadowEnabled
        layer.effect: MultiEffect {
            shadowEnabled: Common.OverlayTheme.shadowEnabled
            shadowColor: Common.OverlayTheme.shadowColor
            shadowBlur: Common.OverlayTheme.shadowBlur
            shadowVerticalOffset: Common.OverlayTheme.shadowVerticalOffset
        }

        // Prevent click-through
        MouseArea {
            anchors.fill: parent
            onClicked: {}  // eat clicks
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 25
            spacing: 20

            // HEADER - Warning icon and title
            RowLayout {
                Layout.fillWidth: true
                spacing: 15

                // Warning icon (triangle with exclamation)
                Rectangle {
                    width: 60
                    height: 60
                    color: dangerColor
                    radius: Common.OverlayTheme.panelRadius

                    Text {
                        anchors.centerIn: parent
                        text: "!"
                        font.pixelSize: 40
                        font.bold: true
                        font.family: Common.OverlayTheme.fontFamily
                        color: Common.OverlayTheme.textPrimary
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Text {
                        text: shutdownConfirmationViewModel ? shutdownConfirmationViewModel.title : "SHUTDOWN CONFIRMATION"
                        font.pixelSize: 24
                        font.bold: true
                        font.family: Common.OverlayTheme.fontFamily
                        color: dangerColor
                    }

                    Text {
                        text: shutdownConfirmationViewModel ? shutdownConfirmationViewModel.message : "Are you sure you want to shutdown?"
                        font.pixelSize: Common.OverlayTheme.fontSizeBody
                        font.family: Common.OverlayTheme.fontFamily
                        color: Common.OverlayTheme.textTertiary
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
            }

            // Separator
            Rectangle {
                Layout.fillWidth: true
                height: Common.OverlayTheme.separatorHeight
                color: Common.OverlayTheme.dividerMedium
            }

            // OPTIONS - YES and NO buttons
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 12

                // Instruction text
                Text {
                    text: "Use UP/DOWN to select, MENU/VAL to confirm"
                    font.pixelSize: Common.OverlayTheme.fontSizeFooter
                    font.family: Common.OverlayTheme.fontFamily
                    color: Common.OverlayTheme.textMuted
                    Layout.alignment: Qt.AlignHCenter
                }

                Item { Layout.fillHeight: true }  // Spacer

                // YES Option
                Rectangle {
                    id: yesButton
                    Layout.fillWidth: true
                    Layout.preferredHeight: 50
                    Layout.leftMargin: 40
                    Layout.rightMargin: 40

                    property bool isSelected: shutdownConfirmationViewModel && shutdownConfirmationViewModel.selectedOption === 0

                    color: isSelected ? dangerColor : "transparent"
                    border.color: isSelected ? dangerColor : Common.OverlayTheme.textSubtle
                    border.width: isSelected ? 2 : Common.OverlayTheme.panelBorderWidth
                    radius: Common.OverlayTheme.panelRadius

                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 10

                        // Selection indicator
                        Text {
                            text: yesButton.isSelected ? ">" : " "
                            font.pixelSize: Common.OverlayTheme.fontSizeHeading
                            font.bold: true
                            font.family: Common.OverlayTheme.fontFamily
                            color: yesButton.isSelected ? Common.OverlayTheme.textPrimary : Common.OverlayTheme.textSubtle
                        }

                        Text {
                            text: "YES, Shutdown"
                            font.pixelSize: Common.OverlayTheme.fontSizeStatus
                            font.bold: true
                            font.family: Common.OverlayTheme.fontFamily
                            color: yesButton.isSelected ? Common.OverlayTheme.textPrimary : Common.OverlayTheme.textMuted
                        }
                    }
                }

                // NO Option
                Rectangle {
                    id: noButton
                    Layout.fillWidth: true
                    Layout.preferredHeight: 50
                    Layout.leftMargin: 40
                    Layout.rightMargin: 40

                    property bool isSelected: shutdownConfirmationViewModel && shutdownConfirmationViewModel.selectedOption === 1

                    color: isSelected ? accentColor : "transparent"
                    border.color: isSelected ? accentColor : Common.OverlayTheme.textSubtle
                    border.width: isSelected ? 2 : Common.OverlayTheme.panelBorderWidth
                    radius: Common.OverlayTheme.panelRadius

                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 10

                        // Selection indicator
                        Text {
                            text: noButton.isSelected ? ">" : " "
                            font.pixelSize: Common.OverlayTheme.fontSizeHeading
                            font.bold: true
                            font.family: Common.OverlayTheme.fontFamily
                            color: noButton.isSelected ? Common.OverlayTheme.backgroundColor : Common.OverlayTheme.textSubtle
                        }

                        Text {
                            text: "NO, Cancel"
                            font.pixelSize: Common.OverlayTheme.fontSizeStatus
                            font.bold: true
                            font.family: Common.OverlayTheme.fontFamily
                            color: noButton.isSelected ? Common.OverlayTheme.backgroundColor : Common.OverlayTheme.textMuted
                        }
                    }
                }

                Item { Layout.fillHeight: true }  // Spacer
            }

            // Separator
            Rectangle {
                Layout.fillWidth: true
                height: Common.OverlayTheme.separatorHeight
                color: Common.OverlayTheme.dividerMedium
            }

            // STATUS MESSAGE (shows "SHUTDOWN COMPLETE" when confirmed)
            Text {
                id: statusText
                text: shutdownConfirmationViewModel && shutdownConfirmationViewModel.statusMessage ? shutdownConfirmationViewModel.statusMessage : ""
                font.pixelSize: Common.OverlayTheme.fontSizeHeading
                font.bold: true
                font.family: Common.OverlayTheme.fontFamily
                color: dangerColor
                horizontalAlignment: Text.AlignHCenter
                Layout.fillWidth: true
                visible: text.length > 0

                // Blink animation for shutdown message
                SequentialAnimation on opacity {
                    running: statusText.visible
                    loops: Animation.Infinite
                    NumberAnimation { to: 0.3; duration: Common.OverlayTheme.animationDurationSlow }
                    NumberAnimation { to: 1.0; duration: Common.OverlayTheme.animationDurationSlow }
                }
            }

            // Footer hint
            Text {
                text: "Default: NO (safe option)"
                font.pixelSize: Common.OverlayTheme.fontSizeSmall
                font.family: Common.OverlayTheme.fontFamily
                color: Common.OverlayTheme.textSubtle
                Layout.alignment: Qt.AlignHCenter
                visible: !(shutdownConfirmationViewModel && shutdownConfirmationViewModel.statusMessage && shutdownConfirmationViewModel.statusMessage.length > 0)
            }
        }
    }
}
