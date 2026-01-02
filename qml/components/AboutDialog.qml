import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../common" as Common

Rectangle {
    id: root

    // Bind your aboutViewModel here (from C++ / JS). If null, use debugShow to test.
    property var viewModel: aboutViewModel

    // Debug toggle: set true to force-visible while wiring viewModel
    property bool debugShow: false

    // Accent color fallback. If viewModel provides accentColor (string), it overrides.
    property color accentColor: viewModel && viewModel.accentColor ? viewModel.accentColor : Common.OverlayTheme.accentDefault

    // Visible only when either viewModel.visible is true OR debugShow = true
    visible: viewModel ? !!viewModel.visible : debugShow
    anchors.fill: parent

    // Semi-transparent black overlay
    color: Common.OverlayTheme.modalBackdrop

    // Clicking outside closes the dialog (if viewModel supplied)
    MouseArea {
        anchors.fill: parent
        onClicked: {
            if (viewModel) viewModel.visible = false
        }
    }

    // Main dialog
    Rectangle {
        id: aboutDialog
        anchors.centerIn: parent
        width: Math.min(parent.width * 0.8, 700)
        height: Math.min(parent.height * 0.85, 650)

        color: Common.OverlayTheme.backgroundColor
        border.color: accentColor
        border.width: Common.OverlayTheme.panelBorderWidth
        radius: Common.OverlayTheme.panelRadius

        layer.enabled: Common.OverlayTheme.shadowEnabled
        layer.effect: MultiEffect {
            shadowEnabled: Common.OverlayTheme.shadowEnabled
            shadowColor: Common.OverlayTheme.shadowColor
            shadowBlur: Common.OverlayTheme.shadowBlur
            shadowVerticalOffset: Common.OverlayTheme.shadowVerticalOffset
        }

        // Prevent click-through: eat clicks on the dialog itself
        MouseArea {
            anchors.fill: parent
            onClicked: {} // eat clicks
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 14

            // HEADER
            RowLayout {
                Layout.fillWidth: true
                spacing: 14

                // Logo rectangle
                Rectangle {
                    width: 80
                    height: 80
                    color: accentColor
                    radius: Common.OverlayTheme.panelRadius

                    Text {
                        anchors.centerIn: parent
                        text: "RCWS"
                        font.pixelSize: 24
                        font.bold: true
                        font.family: Common.OverlayTheme.fontFamily
                        color: Common.OverlayTheme.backgroundColor
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Text {
                        text: viewModel && viewModel.appName ? viewModel.appName : "El 7arress RCWS"
                        font.pixelSize: 28
                        font.bold: true
                        font.family: Common.OverlayTheme.fontFamily
                        color: accentColor
                    }

                    Text {
                        text: viewModel && viewModel.appVersion ? viewModel.appVersion : "Version 4.5"
                        font.pixelSize: Common.OverlayTheme.fontSizeBody
                        font.family: Common.OverlayTheme.fontFamily
                        color: Common.OverlayTheme.textTertiary
                    }

                    Text {
                        text: viewModel && viewModel.buildDate ? viewModel.buildDate : ""
                        font.pixelSize: Common.OverlayTheme.fontSizeFooter
                        font.family: Common.OverlayTheme.fontFamily
                        color: Common.OverlayTheme.textSubtle
                    }
                }
            }

            // Separator
            Rectangle {
                Layout.fillWidth: true
                height: Common.OverlayTheme.separatorHeight
                color: accentColor
            }

            // SCROLLABLE CONTENT
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                ColumnLayout {
                    id: scrollContent
                    width: parent.width
                    spacing: 18
                    anchors.margins: 4

                    // Credits Section
                    InfoSection {
                        title: "Credits"
                        content: viewModel && viewModel.credits ? viewModel.credits : ""
                        Layout.fillWidth: true
                        accent: accentColor
                    }

                    // Copyright Section
                    InfoSection {
                        title: "Copyright"
                        content: viewModel && viewModel.copyright ? viewModel.copyright : ""
                        Layout.fillWidth: true
                        accent: accentColor
                    }

                    // License & System Info
                    InfoSection {
                        title: "License & System Information"
                        content: viewModel && viewModel.license ? viewModel.license : ""
                        Layout.fillWidth: true
                        accent: accentColor
                    }

                    // Qt / Build Info
                    Text {
                        text: viewModel && viewModel.qtVersion ? viewModel.qtVersion : ""
                        font.pixelSize: Common.OverlayTheme.fontSizeFooter
                        font.family: Common.OverlayTheme.fontFamily
                        color: Common.OverlayTheme.textSubtle
                        horizontalAlignment: Text.AlignHCenter
                        Layout.alignment: Qt.AlignHCenter
                    }
                }
            }

            // Footer separator
            Rectangle {
                Layout.fillWidth: true
                height: Common.OverlayTheme.separatorHeight
                color: accentColor
            }

            // BUTTONS ROW
            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Button {
                    text: "View Documentation"
                    Layout.preferredWidth: 180

                    onClicked: {
                        Qt.openUrlExternally("file:///path/to/README.md")
                    }

                    background: Rectangle {
                        color: parent.hovered ? Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.08) : "transparent"
                        border.color: accentColor
                        border.width: Common.OverlayTheme.panelBorderWidth
                        radius: Common.OverlayTheme.panelRadius
                    }

                    contentItem: Text {
                        text: parent.text
                        font.pixelSize: Common.OverlayTheme.fontSizeBody
                        font.family: Common.OverlayTheme.fontFamily
                        color: accentColor
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                Item { Layout.fillWidth: true } // spacer

                Button {
                    text: "Close"
                    Layout.preferredWidth: 120

                    onClicked: {
                        if (viewModel) viewModel.visible = false
                    }

                    background: Rectangle {
                        color: accentColor
                        radius: Common.OverlayTheme.panelRadius
                    }

                    contentItem: Text {
                        text: parent.text
                        font.pixelSize: Common.OverlayTheme.fontSizeStatusLarge
                        font.bold: true
                        font.family: Common.OverlayTheme.fontFamily
                        color: Common.OverlayTheme.backgroundColor
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
    }

    // InfoSection component (themed)
    component InfoSection: ColumnLayout {
        property string title: "Section"
        property string content: ""
        property color accent: Common.OverlayTheme.accentDefault

        spacing: 8

        Text {
            text: title
            font.pixelSize: Common.OverlayTheme.fontSizeStatus
            font.bold: true
            font.family: Common.OverlayTheme.fontFamily
            color: accent
        }

        Rectangle {
            Layout.fillWidth: true
            height: Common.OverlayTheme.separatorHeight
            color: Common.OverlayTheme.dividerMedium
        }

        Text {
            text: content
            font.pixelSize: Common.OverlayTheme.fontSizeBody
            font.family: Common.OverlayTheme.fontFamily
            color: Common.OverlayTheme.textSecondary
            textFormat: Text.RichText
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }
}
