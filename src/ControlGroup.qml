import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: group
    property string title
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

        Label {
            text: group.title
            font.pixelSize: 13
            font.weight: Font.DemiBold
            font.capitalization: Font.AllUppercase
            font.letterSpacing: 1
            color: theme.accent
        }
    }
}
