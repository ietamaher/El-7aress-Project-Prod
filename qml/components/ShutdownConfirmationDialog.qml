import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

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

    // Bind your viewModel here (from C++)
    property var viewModel: shutdownConfirmationViewModel

    // Accent color (from viewModel or fallback)
    property color accentColor: viewModel && viewModel.accentColor ? viewModel.accentColor : "#46E2A5"

    // Warning/danger color for shutdown
    property color dangerColor: "#FF4444"

    // Visible when viewModel.visible is true
    visible: viewModel ? !!viewModel.visible : false
    anchors.fill: parent

    // Semi-transparent dark overlay
    color: "#CC000000"

    Component.onCompleted: {
        console.log("ShutdownConfirmationDialog created. visible=", root.visible)
    }

    // Main dialog container
    Rectangle {
        id: dialogBox
        anchors.centerIn: parent
        width: Math.min(parent.width * 0.6, 500)
        height: Math.min(parent.height * 0.5, 350)

        color: "#1A1A1A"
        border.color: dangerColor  // Red border for danger/warning
        border.width: 2
        radius: 0

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
                    radius: 0

                    Text {
                        anchors.centerIn: parent
                        text: "!"
                        font.pixelSize: 40
                        font.bold: true
                        font.family: "Archivo Narrow"
                        color: "#FFFFFF"
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Text {
                        text: viewModel ? viewModel.title : "SHUTDOWN CONFIRMATION"
                        font.pixelSize: 24
                        font.bold: true
                        font.family: "Archivo Narrow"
                        color: dangerColor
                    }

                    Text {
                        text: viewModel ? viewModel.message : "Are you sure you want to shutdown?"
                        font.pixelSize: 14
                        font.family: "Archivo Narrow"
                        color: "#AAAAAA"
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
            }

            // Separator
            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: "#444444"
            }

            // OPTIONS - YES and NO buttons
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 12

                // Instruction text
                Text {
                    text: "Use UP/DOWN to select, MENU/VAL to confirm"
                    font.pixelSize: 12
                    font.family: "Archivo Narrow"
                    color: "#888888"
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

                    property bool isSelected: viewModel && viewModel.selectedOption === 0

                    color: isSelected ? dangerColor : "transparent"
                    border.color: isSelected ? dangerColor : "#666666"
                    border.width: isSelected ? 2 : 1
                    radius: 0

                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 10

                        // Selection indicator
                        Text {
                            text: yesButton.isSelected ? ">" : " "
                            font.pixelSize: 20
                            font.bold: true
                            font.family: "Archivo Narrow"
                            color: yesButton.isSelected ? "#FFFFFF" : "#666666"
                        }

                        Text {
                            text: "YES, Shutdown"
                            font.pixelSize: 18
                            font.bold: true
                            font.family: "Archivo Narrow"
                            color: yesButton.isSelected ? "#FFFFFF" : "#888888"
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

                    property bool isSelected: viewModel && viewModel.selectedOption === 1

                    color: isSelected ? accentColor : "transparent"
                    border.color: isSelected ? accentColor : "#666666"
                    border.width: isSelected ? 2 : 1
                    radius: 0

                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 10

                        // Selection indicator
                        Text {
                            text: noButton.isSelected ? ">" : " "
                            font.pixelSize: 20
                            font.bold: true
                            font.family: "Archivo Narrow"
                            color: noButton.isSelected ? "#0A0A0A" : "#666666"
                        }

                        Text {
                            text: "NO, Cancel"
                            font.pixelSize: 18
                            font.bold: true
                            font.family: "Archivo Narrow"
                            color: noButton.isSelected ? "#0A0A0A" : "#888888"
                        }
                    }
                }

                Item { Layout.fillHeight: true }  // Spacer
            }

            // Separator
            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: "#444444"
            }

            // STATUS MESSAGE (shows "SHUTDOWN COMPLETE" when confirmed)
            Text {
                id: statusText
                text: viewModel && viewModel.statusMessage ? viewModel.statusMessage : ""
                font.pixelSize: 20
                font.bold: true
                font.family: "Archivo Narrow"
                color: dangerColor
                horizontalAlignment: Text.AlignHCenter
                Layout.fillWidth: true
                visible: text.length > 0

                // Blink animation for shutdown message
                SequentialAnimation on opacity {
                    running: statusText.visible
                    loops: Animation.Infinite
                    NumberAnimation { to: 0.3; duration: 500 }
                    NumberAnimation { to: 1.0; duration: 500 }
                }
            }

            // Footer hint
            Text {
                text: "Default: NO (safe option)"
                font.pixelSize: 11
                font.family: "Archivo Narrow"
                color: "#555555"
                Layout.alignment: Qt.AlignHCenter
                visible: !(viewModel && viewModel.statusMessage && viewModel.statusMessage.length > 0)
            }
        }
    }
}
