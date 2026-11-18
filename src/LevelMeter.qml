import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: meter
    property string label
    property real level: -90
    property bool active: false
    property bool highlighted: false
    property string idleText
    // Extra controls for the header row, such as the microphone picker.
    default property alias accessory: accessoryRow.data

    // -60 dBFS reads as empty; speech usually sits in the top third.
    readonly property real fraction: active ? Math.max(0, Math.min(1, (level + 60) / 60)) : 0

    implicitHeight: 92
    color: "#17171a"
    border.color: "#28282e"
    radius: theme.radius

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            // A picker in the header names the source itself.
            Label {
                visible: accessoryRow.children.length === 0
                text: meter.label
                font.weight: Font.Medium
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
            RowLayout {
                id: accessoryRow
                visible: children.length > 0
                Layout.fillWidth: true
            }
            Label {
                text: !meter.active ? meter.idleText
                      : meter.level <= -89 ? "−∞ dB"
                      : (translator.language, win.format(meter.level, 0) + " dB")
                color: "#9b9ba3"
                font.features: { "tnum": 1 }
            }
        }
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 10
            radius: Math.min(4, theme.radius)
            color: "#26262b"
            clip: true

            Rectangle {
                height: parent.height
                width: parent.width * meter.fraction
                radius: parent.radius
                color: meter.level > -3 ? "#e5484d" : meter.highlighted ? theme.accent : "#8a8a93"
                Behavior on width { NumberAnimation { duration: 60 } }
            }
        }
    }
}
