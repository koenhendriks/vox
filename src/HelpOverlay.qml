import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: overlay
    signal closeRequested()
    color: "#cc0b0b0d"

    readonly property var shortcuts: [
        ["Space", qsTr("Compare with your raw microphone")],
        ["L", qsTr("Listen to yourself")],
        ["O", qsTr("Turn the Vox Microphone on or off")],
        ["D", qsTr("Make Vox the default microphone")],
        ["1 – 9", qsTr("Switch profile")],
        ["↑ ↓", qsTr("Previous or next profile")],
        ["N", qsTr("New profile from this one")],
        ["R", qsTr("Rename profile")],
        ["Delete", qsTr("Delete profile")],
        ["Ctrl+Z", qsTr("Undo")],
        ["Ctrl+Shift+Z", qsTr("Redo")],
        ["?", qsTr("Show or hide shortcuts")],
        ["Q", qsTr("Quit (Vox keeps running)")],
    ]

    MouseArea { anchors.fill: parent; onClicked: overlay.closeRequested() }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width - 48, 520)
        height: content.implicitHeight + 48
        color: "#17171a"
        border.color: "#28282e"
        radius: theme.radius

        ColumnLayout {
            id: content
            anchors.fill: parent
            anchors.margins: 24
            spacing: 10

            Label {
                text: qsTr("Keyboard shortcuts")
                font.pixelSize: 18
                font.weight: Font.DemiBold
                Layout.bottomMargin: 6
            }
            Repeater {
                model: overlay.shortcuts
                RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: 16
                    Rectangle {
                        Layout.preferredWidth: 120
                        Layout.preferredHeight: 26
                        radius: Math.min(4, theme.radius)
                        color: "#24242a"
                        Label {
                            anchors.centerIn: parent
                            text: modelData[0]
                            font.family: "monospace"
                            color: theme.accent
                        }
                    }
                    Label {
                        text: modelData[1]
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                    }
                }
            }
        }
    }
}
