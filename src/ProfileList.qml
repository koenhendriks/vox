import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Rectangle {
    id: list
    signal renameRequested()
    signal deleteRequested()
    signal newRequested()

    color: "#121214"

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: 20
        anchors.bottomMargin: 12
        spacing: 8

        Label {
            Layout.leftMargin: 20
            text: qsTr("Profiles")
            font.pixelSize: 13
            font.weight: Font.DemiBold
            font.capitalization: Font.AllUppercase
            font.letterSpacing: 1
            color: "#9b9ba3"
        }

        ListView {
            id: view
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: backend.profileNames
            currentIndex: backend.activeIndex
            boundsBehavior: Flickable.StopAtBounds

            delegate: Item {
                id: row
                required property int index
                required property string modelData
                readonly property bool current: index === backend.activeIndex
                width: ListView.view.width
                height: 44

                Rectangle {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    radius: Math.min(4, theme.radius)
                    color: row.current ? Qt.alpha(theme.accent, 0.16)
                         : mouse.containsMouse ? "#1d1d21" : "transparent"
                    border.color: row.current ? Qt.alpha(theme.accent, 0.6) : "transparent"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 10
                        Label {
                            text: row.index < 9 ? String(row.index + 1) : ""
                            color: row.current ? theme.accent : "#6b6b73"
                            font.features: { "tnum": 1 }
                            Layout.preferredWidth: 12
                        }
                        Label {
                            text: row.modelData
                            elide: Text.ElideRight
                            font.weight: row.current ? Font.DemiBold : Font.Normal
                            Layout.fillWidth: true
                        }
                    }
                }
                MouseArea {
                    id: mouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: backend.selectProfile(row.index)
                    onDoubleClicked: {
                        backend.selectProfile(row.index)
                        list.renameRequested()
                    }
                }
            }
        }

        // Wraps instead of truncating, since some languages' words are long.
        Flow {
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            Layout.fillWidth: true
            spacing: 0
            Button {
                text: qsTr("New")
                flat: true
                focusPolicy: Qt.NoFocus
                leftPadding: 10
                rightPadding: 10
                onClicked: list.newRequested()
            }
            Button {
                text: qsTr("Rename")
                flat: true
                focusPolicy: Qt.NoFocus
                leftPadding: 10
                rightPadding: 10
                onClicked: list.renameRequested()
            }
            Button {
                text: qsTr("Delete")
                flat: true
                focusPolicy: Qt.NoFocus
                leftPadding: 10
                rightPadding: 10
                enabled: backend.profileNames.length > 1
                onClicked: list.deleteRequested()
            }
        }
    }
}
