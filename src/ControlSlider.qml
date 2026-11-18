import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ColumnLayout {
    id: control
    property string key
    property string label
    property string hint
    property string unit
    property bool signed: false

    readonly property var range: {
        const list = backend.controls
        for (let i = 0; i < list.length; ++i)
            if (list[i].key === key)
                return list[i]
        return { min: 0, max: 1, step: 0.1 }
    }
    readonly property real value: backend.current[key] ?? 0
    readonly property int decimals: range.step < 1 ? 1 : 0

    Layout.fillWidth: true
    spacing: 0
    opacity: enabled ? 1 : 0.45

    RowLayout {
        Layout.fillWidth: true
        Label {
            text: control.label
            font.weight: Font.Medium
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
        Label {
            // Re-reads the language so separators follow a switch.
            text: (translator.language, (control.signed && control.value > 0 ? "+" : "")
                  + win.format(control.value, control.decimals) + " " + control.unit)
            color: "#d6d6dc"
            font.features: { "tnum": 1 }
        }
    }
    Slider {
        Layout.fillWidth: true
        Layout.leftMargin: -6
        Layout.rightMargin: -6
        Material.roundedScale: theme.radius
        focusPolicy: Qt.NoFocus
        from: control.range.min
        to: control.range.max
        // Rounded here rather than with stepSize, which makes Material draw
        // a tick for every step.
        value: control.value
        onPressedChanged: if (pressed) backend.beginEdit()
        onMoved: backend.setValue(control.key, Math.round(value / control.range.step) * control.range.step)
    }
    Label {
        text: control.hint
        color: "#9b9ba3"
        font.pixelSize: 12
        wrapMode: Text.Wrap
        Layout.fillWidth: true
    }
}
