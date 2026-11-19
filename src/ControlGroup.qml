import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: group
    property string title
    // A switch in the header turns the whole group on or off.
    property bool toggleable: false
    property bool checked: false
    signal toggled(bool checked)
    default property alias content: column.data

    implicitHeight: column.implicitHeight + 40
    implicitWidth: 280
    color: "#17171a"
    border.color: "#28282e"
    radius: theme.radius

    ColumnLayout {
        id: column
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 20
        spacing: 18

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: group.title
                font.pixelSize: 13
                font.weight: Font.DemiBold
                font.capitalization: Font.AllUppercase
                font.letterSpacing: 1
                color: theme.accent
                Layout.fillWidth: true
            }
            Switch {
                visible: group.toggleable
                checked: group.checked
                focusPolicy: Qt.NoFocus
                Layout.preferredHeight: 24
                topPadding: 0
                bottomPadding: 0
                onToggled: group.toggled(checked)
            }
        }
    }
}
