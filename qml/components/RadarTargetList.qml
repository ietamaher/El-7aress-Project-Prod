import QtQuick
import QtQuick.Controls

/**
 * RadarTargetList.qml - Radar target selection list overlay
 *
 * Displays a list of radar targets when radar motion mode is selected.
 * Appears below the STATUS BLOCK in the OSD.
 *
 * Features:
 * - First row is empty "SELECT TARGET" (cancel/no selection)
 * - Scrollable list with UP/DOWN buttons
 * - Validates selection with MENU/VAL button
 * - Shows target ID, azimuth, and range
 */
Rectangle {
    id: radarTargetList

    property var viewModel: null
    property color accentColor: viewModel && viewModel.accentColor ? viewModel.accentColor : "#46E2A5"

    // Visibility bound to viewModel
    visible: viewModel ? viewModel.visible : false

    // Position and size (below status block)
    width: 280
    height: Math.min(listView.contentHeight + titleBar.height + 16, 300)

    color: "#DD000000"
    border.color: accentColor
    border.width: 2
    radius: 4

    // Title bar
    Rectangle {
        id: titleBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 28
        color: accentColor
        radius: 2

        Text {
            anchors.centerIn: parent
            text: viewModel ? viewModel.title : "RADAR TARGETS"
            font.pixelSize: 13
            font.bold: true
            font.family: "Archivo Narrow"
            color: "black"
        }
    }

    // Target list
    ListView {
        id: listView
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: titleBar.bottom
        anchors.bottom: parent.bottom
        anchors.margins: 8

        clip: true
        spacing: 2

        model: viewModel ? viewModel.targetsModel : null
        currentIndex: viewModel ? viewModel.currentIndex : 0

        highlightFollowsCurrentItem: true
        highlightMoveDuration: 150

        delegate: Item {
            width: listView.width
            height: 32
            property bool highlighted: ListView.isCurrentItem

            // Selection highlight background
            Rectangle {
                anchors.fill: parent
                color: highlighted ? Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.3) : "transparent"
                radius: 2

                Behavior on color {
                    ColorAnimation { duration: 100 }
                }
            }

            // Left accent bar for selected item
            Rectangle {
                id: selectionBar
                width: 4
                height: parent.height
                anchors.left: parent.left
                color: accentColor
                visible: highlighted
                radius: 1

                Behavior on opacity {
                    NumberAnimation { duration: 150 }
                }
            }

            // Item text
            Text {
                anchors.left: selectionBar.right
                anchors.leftMargin: 8
                anchors.right: parent.right
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter

                text: model.display
                font.pixelSize: 12
                font.bold: highlighted
                font.family: "Archivo Narrow"
                color: {
                    if (index === 0) {
                        // First row (cancel) - dimmer color
                        return highlighted ? accentColor : "#888888"
                    }
                    return highlighted ? "#FFFFFF" : "#CCCCCC"
                }
                elide: Text.ElideRight
            }

            // Separator line
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: 12
                height: 1
                color: "#333333"
                visible: index < listView.count - 1
            }
        }

        // Custom scrollbar
        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
            active: true

            contentItem: Rectangle {
                implicitWidth: 4
                radius: 2
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

    // Footer hint
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 18
        color: "#20000000"
        visible: viewModel ? viewModel.visible : false

        Text {
            anchors.centerIn: parent
            text: "UP/DOWN: Navigate | VAL: Select"
            font.pixelSize: 9
            font.family: "Archivo Narrow"
            color: "#666666"
        }
    }
}
