import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// The soundboard: sounds the user plays into the Vox Microphone.
Rectangle {
    id: panel
    signal renameRequested(int index)

    color: "#121214"
    implicitHeight: column.implicitHeight + 24

    DropArea {
        id: drop
        anchors.fill: parent
        onEntered: event => event.accepted = event.hasUrls
        onDropped: event => {
            soundboard.add(event.urls)
            event.acceptProposedAction()
        }
    }
    Rectangle {
        anchors.fill: parent
        anchors.margins: 6
        visible: drop.containsDrag
        color: Qt.alpha(theme.accent, 0.10)
        border.color: theme.accent
        radius: Math.min(4, theme.radius)
    }

    ColumnLayout {
        id: column
        anchors.fill: parent
        anchors.topMargin: 12
        anchors.bottomMargin: 12
        spacing: 8

        // Three buttons and the title only just fit the sidebar.
        RowLayout {
            Layout.leftMargin: 20
            Layout.rightMargin: 6
            Layout.fillWidth: true
            spacing: 0
            Label {
                text: qsTr("Soundboard")
                elide: Text.ElideRight
                font.pixelSize: 13
                font.weight: Font.DemiBold
                font.capitalization: Font.AllUppercase
                font.letterSpacing: 1
                color: "#9b9ba3"
                Layout.fillWidth: true
            }
            ToolButton {
                visible: soundboard.playing
                text: "■"
                focusPolicy: Qt.NoFocus
                Layout.preferredHeight: 32
                Layout.preferredWidth: 28
                topInset: 0
                bottomInset: 0
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Stop all sounds (S)")
                onClicked: soundboard.stopAll()
            }
            ToolButton {
                text: "⋯"
                focusPolicy: Qt.NoFocus
                Layout.preferredHeight: 32
                Layout.preferredWidth: 28
                topInset: 0
                bottomInset: 0
                ToolTip.visible: hovered && !settings.visible
                ToolTip.text: qsTr("Soundboard settings")
                onClicked: settings.open()

                Popup {
                    id: settings
                    y: parent.height
                    x: parent.width - width
                    width: 240
                    padding: 14
                    Material.roundedScale: theme.radius
                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 6
                        Label {
                            text: qsTr("Volume")
                            color: "#9b9ba3"
                            font.pixelSize: 12
                        }
                        Slider {
                            Layout.fillWidth: true
                            focusPolicy: Qt.NoFocus
                            from: 0
                            to: 100
                            value: soundboard.volume
                            onMoved: soundboard.setVolume(Math.round(value))
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Label {
                                text: qsTr("Hear sounds yourself")
                                wrapMode: Text.Wrap
                                Layout.fillWidth: true
                            }
                            Switch {
                                focusPolicy: Qt.NoFocus
                                checked: soundboard.monitor
                                onToggled: soundboard.setMonitor(checked)
                            }
                        }
                    }
                }
            }
            ToolButton {
                text: "+"
                font.pixelSize: 18
                focusPolicy: Qt.NoFocus
                Layout.preferredHeight: 32
                Layout.preferredWidth: 28
                topInset: 0
                bottomInset: 0
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Add sounds")
                onClicked: soundboard.choose()
            }
        }

        Label {
            visible: soundboard.sounds.length === 0
            text: qsTr("Drop sound files here, or press + to add some.")
            color: "#9b9ba3"
            font.pixelSize: 12
            wrapMode: Text.Wrap
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            Layout.fillWidth: true
        }

        GridView {
            id: grid
            visible: soundboard.sounds.length > 0
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            Layout.fillWidth: true
            Layout.fillHeight: true
            // Up to four rows show at once; more, or a short window, scroll.
            Layout.preferredHeight: Math.min(contentHeight, cellHeight * 4)
            Layout.minimumHeight: cellHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            cellWidth: width / 2
            cellHeight: 40
            model: soundboard.sounds

            delegate: Item {
                id: tile
                required property int index
                required property var modelData
                width: grid.cellWidth
                height: grid.cellHeight

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 3
                    radius: Math.min(4, theme.radius)
                    color: tile.modelData.playing ? theme.accent
                         : mouse.containsMouse ? "#24242a" : "#1b1b1f"
                    border.color: tile.modelData.playing ? theme.accent : "#2a2a30"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 6
                        Label {
                            text: tile.index < 9 ? String(tile.index + 1) : ""
                            visible: text.length > 0
                            color: tile.modelData.playing ? theme.accentForeground : "#6b6b73"
                            font.pixelSize: 11
                            font.features: { "tnum": 1 }
                        }
                        Label {
                            text: tile.modelData.name
                            color: tile.modelData.playing ? theme.accentForeground : "#e8e8ea"
                            font.pixelSize: 13
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }
                }
                MouseArea {
                    id: mouse
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onClicked: event => {
                        if (event.button === Qt.RightButton)
                            menu.popup()
                        else
                            soundboard.play(tile.index)
                    }
                }
                Menu {
                    id: menu
                    Material.roundedScale: theme.radius
                    MenuItem {
                        text: qsTr("Rename")
                        onTriggered: panel.renameRequested(tile.index)
                    }
                    MenuItem {
                        text: qsTr("Remove")
                        onTriggered: soundboard.remove(tile.index)
                    }
                }
            }
        }

        Label {
            visible: soundboard.importing > 0
            text: qsTr("Adding sounds…")
            color: "#9b9ba3"
            font.pixelSize: 12
            Layout.leftMargin: 20
        }
    }
}
