import QtQuick
import QtQuick.Controls
import QtQuick.Effects

Rectangle {
    id: mainmenuroot
    property var osdViewModel: null
    width: 340
    height: 500
    radius: 0
    color: "#0A0A0A"  // Deep black background like BMW
    border.color: "#1A1A1A"
    border.width: 1
    visible: viewModel ? viewModel.visible : false
    property var viewModel: null
    property color accentColor: viewModel ? viewModel.accentColor : "#FF5722"  // BMW-style orange-red

    // Subtle drop shadow
    layer.enabled: true
    layer.effect: MultiEffect {
        shadowEnabled: true
        shadowColor: "#80000000"
        shadowBlur: 0.4
        shadowVerticalOffset: 8
    }

    Column {
        anchors.fill: parent
        anchors.margins: 0
        spacing: 0

        // Header section
        Item {
            width: parent.width
            height: 60

            Column {
                anchors.centerIn: parent
                spacing: 8

                Text {
                    text: viewModel ? viewModel.title : "Main menu"
                    font.pixelSize: 22
                    font.weight: Font.Normal
                    color: "#FFFFFF"
                    font.family: "Segoe UI"
                    horizontalAlignment: Text.AlignHCenter
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                Text {
                    text: viewModel ? viewModel.description : "Select an option"
                    font.pixelSize: 13
                    color: "#808080"
                    font.family: "Segoe UI"
                    horizontalAlignment: Text.AlignHCenter
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }
        }

        // Subtle divider
        Rectangle {
            width: parent.width
            height: 1
            color: "#1A1A1A"
        }

        // Menu items list
        ListView {
            id: listView
            width: parent.width
            height: parent.height - 80
            spacing: 0
            clip: true
            model: viewModel ? viewModel.optionsModel : null
            currentIndex: viewModel ? viewModel.currentIndex : -1
            highlightFollowsCurrentItem: true
            highlightMoveDuration: 250
            highlightMoveVelocity: -1

            delegate: Item {
                width: listView.width
                height: 34
                enabled: !model.display.startsWith("---")
                opacity: model.display.startsWith("---") ? 0.4 : 1.0

                // Background for hover effect
                Rectangle {
                    anchors.fill: parent
                    color: !highlighted ? "#15FFFFFF" : "transparent"

                    Behavior on color {
                        ColorAnimation { duration: 150 }
                    }
                }

                // Left accent bar for selected item (BMW style)
                Rectangle {
                    id: accentBar
                    width: 4
                    height: parent.height
                    anchors.left: parent.left
                    color: accentColor
                    visible: highlighted

                    // Smooth animation when selection changes
                    opacity: highlighted ? 1 : 0
                    Behavior on opacity {
                        NumberAnimation { duration: 200 }
                    }
                }

                // Menu item text
                Text {
                    text: model.display
                    color: highlighted ? "#FFFFFF" : "#CCCCCC"
                    font.pixelSize: 17
                    font.weight: highlighted ? Font.DemiBold : Font.Normal
                    font.family: "Segoe UI"
                    verticalAlignment: Text.AlignVCenter
                    anchors {
                        left: parent.left
                        leftMargin: 28
                        right: parent.right
                        rightMargin: 20
                        verticalCenter: parent.verticalCenter
                    }

                    Behavior on color {
                        ColorAnimation { duration: 150 }
                    }

                    Behavior on font.weight {
                        NumberAnimation { duration: 150 }
                    }
                }

                // Bottom divider line
                Rectangle {
                    width: parent.width - 28
                    height: 1
                    anchors.bottom: parent.bottom
                    anchors.right: parent.right
                    color: "#151515"
                    visible: index < listView.count - 1
                }


                // Property to determine if this item is selected
                property bool highlighted: ListView.isCurrentItem
            }

            // Custom scrollbar styling
            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
                active: true

                contentItem: Rectangle {
                    implicitWidth: 6
                    radius: 0
                    color: "#40FFFFFF"
                    opacity: parent.active ? 1 : 0.5

                    Behavior on opacity {
                        NumberAnimation { duration: 200 }
                    }
                }

                background: Rectangle {
                    color: "transparent"
                }
            }
        }
    }
}
