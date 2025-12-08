import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects

Rectangle {
    id: root

    property var viewModel: zoneDefinitionViewModel
    property var mapViewModel: zoneMapViewModel
    property color accentColor: viewModel ? viewModel.accentColor : Qt.rgba(70, 226, 165, 1.0)

    visible: viewModel ? viewModel.visible : false
    color: Qt.rgba(0, 0, 0, 0.7)
    anchors.fill: parent

    // Main content container
    Rectangle {
        id: contentContainer
        anchors.centerIn: parent
        width: Math.min(parent.width - 20, 800)
        height: Math.min(parent.height - 20, 700)
        color: "#0A0A0A"
        border.color: "#1A1A1A"
        border.width: 1
        radius: 0

        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: "#80000000"
            shadowBlur: 0.5
            shadowVerticalOffset: 10
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 0
            spacing: 0

            // Header section
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 80
                color: "transparent"

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 6

                    Text {
                        text: viewModel ? viewModel.title : ""
                        font.pixelSize: 20
                        font.weight: Font.Normal
                        font.family: "Segoe UI"
                        color: "#FFFFFF"
                        horizontalAlignment: Text.AlignHCenter
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Text {
                        text: viewModel ? viewModel.instruction : ""
                        font.pixelSize: 12
                        font.family: "Segoe UI"
                        color: "#808080"
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        Layout.alignment: Qt.AlignHCenter
                    }
                }
            }

            // Gimbal position display
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                color: "#0D0D0D"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 25
                    anchors.rightMargin: 25
                    spacing: 20

                    Text {
                        text: "WS Pos:"
                        font.pixelSize: 14
                        font.family: "Segoe UI"
                        color: "#606060"
                    }

                    Text {
                        text: viewModel ? "Az: " + viewModel.gimbalAz.toFixed(1) + "°" : "Az: ---°"
                        font.pixelSize: 14
                        font.family: "Segoe UI"
                        color: accentColor
                    }

                    Item { Layout.fillWidth: true }

                    Text {
                        text: viewModel ? "El: " + viewModel.gimbalEl.toFixed(1) + "°" : "El: ---°"
                        font.pixelSize: 14
                        font.family: "Segoe UI"
                        color: accentColor
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: "#151515"
            }

            // Main content area (menu OR map)
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                // Main Menu List
                ListView {
                    id: mainMenuList
                    anchors.fill: parent
                    visible: viewModel ? viewModel.showMainMenu : false
                    model: viewModel ? viewModel.menuOptions : []
                    currentIndex: viewModel ? viewModel.currentIndex : 0
                    clip: true
                    spacing: 0

                    delegate: Item {
                        width: mainMenuList.width
                        height: 48
                        enabled: !modelData.startsWith("---")
                        opacity: modelData.startsWith("---") ? 0.4 : 1.0

                        Rectangle {
                            anchors.fill: parent
                            color: mouseArea.containsMouse && !highlighted ? "#15FFFFFF" : "transparent"
                            Behavior on color { ColorAnimation { duration: 150 } }
                        }

                        Rectangle {
                            width: 4
                            height: parent.height
                            anchors.left: parent.left
                            color: accentColor
                            visible: highlighted
                            opacity: highlighted ? 1 : 0
                            Behavior on opacity { NumberAnimation { duration: 200 } }
                        }

                        Text {
                            text: modelData
                            color: highlighted ? "#FFFFFF" : "#CCCCCC"
                            font.pixelSize: 16
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
                            Behavior on color { ColorAnimation { duration: 150 } }
                        }

                        Rectangle {
                            width: parent.width - 28
                            height: 1
                            anchors.bottom: parent.bottom
                            anchors.right: parent.right
                            color: "#151515"
                            visible: index < mainMenuList.count - 1
                        }

                        MouseArea {
                            id: mouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: if (!modelData.startsWith("---")) mainMenuList.currentIndex = index
                        }

                        property bool highlighted: ListView.isCurrentItem
                    }

                    ScrollBar.vertical: ScrollBar {
                        policy: ScrollBar.AsNeeded
                        contentItem: Rectangle {
                            implicitWidth: 6
                            radius: 3
                            color: "#40FFFFFF"
                        }
                    }
                }

                // Zone Selection List
                ListView {
                    id: zoneSelectionList
                    anchors.fill: parent
                    visible: viewModel ? viewModel.showZoneSelectionList : false
                    model: viewModel ? viewModel.menuOptions : []
                    currentIndex: viewModel ? viewModel.currentIndex : 0
                    clip: true
                    spacing: 0

                    delegate: Item {
                        width: zoneSelectionList.width
                        height: 48
                        enabled: !modelData.startsWith("---")
                        opacity: modelData.startsWith("---") ? 0.4 : 1.0

                        Rectangle {
                            anchors.fill: parent
                            color: mouseArea2.containsMouse && !highlighted ? "#15FFFFFF" : "transparent"
                            Behavior on color { ColorAnimation { duration: 150 } }
                        }

                        Rectangle {
                            width: 4
                            height: parent.height
                            anchors.left: parent.left
                            color: accentColor
                            visible: highlighted
                            opacity: highlighted ? 1 : 0
                            Behavior on opacity { NumberAnimation { duration: 200 } }
                        }

                        Text {
                            text: modelData
                            color: highlighted ? "#FFFFFF" : "#CCCCCC"
                            font.pixelSize: 16
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
                            Behavior on color { ColorAnimation { duration: 150 } }
                        }

                        Rectangle {
                            width: parent.width - 28
                            height: 1
                            anchors.bottom: parent.bottom
                            anchors.right: parent.right
                            color: "#151515"
                            visible: index < zoneSelectionList.count - 1
                        }

                        MouseArea {
                            id: mouseArea2
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: if (!modelData.startsWith("---")) zoneSelectionList.currentIndex = index
                        }

                        property bool highlighted: ListView.isCurrentItem
                    }

                    ScrollBar.vertical: ScrollBar {
                        policy: ScrollBar.AsNeeded
                        contentItem: Rectangle {
                            implicitWidth: 6
                            radius: 3
                            color: "#40FFFFFF"
                        }
                    }
                }

                // Confirm Dialog
                ListView {
                    id: confirmList
                    anchors.centerIn: parent
                    width: parent.width * 0.6
                    height: 70
                    visible: viewModel ? viewModel.showConfirmDialog : false
                    model: viewModel ? viewModel.menuOptions : []
                    currentIndex: viewModel ? viewModel.currentIndex : 0
                    clip: true
                    spacing: 15
                    orientation: ListView.Horizontal

                    delegate: Rectangle {
                        width: 140
                        height: 60
                        color: "transparent"
                        border.color: highlighted ? accentColor : "#404040"
                        border.width: highlighted ? 3 : 1
                        radius: 5

                        Behavior on border.color { ColorAnimation { duration: 150 } }
                        Behavior on border.width { NumberAnimation { duration: 150 } }

                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: highlighted ? 0 : 3
                            color: highlighted ? accentColor : "transparent"
                            radius: 5
                            opacity: highlighted ? 0.15 : 0
                            Behavior on opacity { NumberAnimation { duration: 150 } }
                        }

                        Text {
                            anchors.centerIn: parent
                            text: modelData
                            font.pixelSize: 16
                            font.weight: highlighted ? Font.DemiBold : Font.Normal
                            font.family: "Segoe UI"
                            color: highlighted ? "#FFFFFF" : "#CCCCCC"
                            Behavior on color { ColorAnimation { duration: 150 } }
                        }

                        property bool highlighted: ListView.isCurrentItem
                    }
                }
            }

            // Parameter Panel Container
            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: "#151515"
                visible: viewModel ? viewModel.showParameterPanel : false
            }

            Loader {
                id: parameterPanelLoader
                Layout.fillWidth: true
                Layout.preferredHeight: 320
                visible: viewModel ? viewModel.showParameterPanel : false

                sourceComponent: {
                    if (!viewModel || !viewModel.showParameterPanel) return null

                    switch (viewModel.activePanelType) {
                        case 1: return areaZonePanelComponent
                        case 2: return sectorScanPanelComponent
                        case 3: return trpPanelComponent
                        default: return null
                    }
                }
            }

            // Zone Map
            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: "#151515"
                visible: viewModel ? viewModel.showMap : true
            }

            ZoneMapCanvas {
                id: zoneMap
                Layout.fillWidth: true
                Layout.preferredHeight: 250
                visible: viewModel ? viewModel.showMap : true
                viewModel: mapViewModel
            }


        }
    }

    // Parameter Panel Components
    Component {
        id: areaZonePanelComponent
        AreaZoneParameterPanel {
            viewModel: areaZoneParameterViewModel
            accentColor: root.accentColor
        }
    }

    Component {
        id: sectorScanPanelComponent
        SectorScanParameterPanel {
            viewModel: sectorScanParameterViewModel
            accentColor: root.accentColor
        }
    }

    Component {
        id: trpPanelComponent
        TRPParameterPanel {
            viewModel: trpParameterViewModel
            accentColor: root.accentColor
        }
    }
}
