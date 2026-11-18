import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// One version of the test take: its level curve, with the shared playhead.
Rectangle {
    id: track
    property string label
    property string detail
    property var curve: []
    property bool current: false
    property bool playing: false
    property real progress: 0
    property color tint: "#8a8a93"

    signal activated()
    signal seekRequested(real fraction)

    implicitHeight: 112
    radius: Math.min(4, theme.radius)
    color: current ? Qt.alpha(theme.accent, 0.08) : "#141417"
    border.color: current ? Qt.alpha(theme.accent, 0.7) : "#28282e"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            RoundButton {
                Layout.preferredWidth: 34
                Layout.preferredHeight: 34
                padding: 0
                focusPolicy: Qt.NoFocus
                text: track.current && track.playing ? "❚❚" : "▶"
                font.pixelSize: 12
                onClicked: track.activated()
            }
            Label {
                text: track.label
                font.weight: track.current ? Font.DemiBold : Font.Medium
                color: track.current ? "#f2f2f4" : "#c9c9cf"
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            Label {
                text: track.detail
                color: "#9b9ba3"
                font.pixelSize: 12
                font.features: { "tnum": 1 }
            }
        }

        // Time runs left to right in every language, like the meters.
        Item {
            id: area
            Layout.fillWidth: true
            Layout.fillHeight: true
            LayoutMirroring.enabled: false
            LayoutMirroring.childrenInherit: true

            Canvas {
                id: base
                anchors.fill: parent
                property color fill: track.current ? Qt.alpha(track.tint, 0.35) : "#3a3a42"
                onFillChanged: requestPaint()
                onWidthChanged: requestPaint()
                onHeightChanged: requestPaint()
                onPaint: area.paintCurve(getContext("2d"), width, height, fill)
            }
            Item {
                width: parent.width * track.progress
                height: parent.height
                clip: true
                visible: track.current
                Canvas {
                    id: played
                    width: area.width
                    height: area.height
                    property color fill: track.tint
                    onFillChanged: requestPaint()
                    onWidthChanged: requestPaint()
                    onHeightChanged: requestPaint()
                    onPaint: area.paintCurve(getContext("2d"), width, height, fill)
                }
            }
            Rectangle {
                x: Math.round(parent.width * track.progress) - 1
                width: 2
                height: parent.height
                color: track.current ? "#f2f2f4" : "#5a5a62"
            }

            Connections {
                target: track
                function onCurveChanged() {
                    base.requestPaint()
                    played.requestPaint()
                }
            }

            // A filled curve mirrored around the middle, the way level
            // envelopes are usually drawn.
            function paintCurve(ctx, w, h, fill) {
                ctx.reset()
                const points = track.curve
                if (!points || points.length < 2)
                    return
                const mid = h / 2
                const step = w / (points.length - 1)
                ctx.fillStyle = fill
                ctx.beginPath()
                ctx.moveTo(0, mid)
                for (let i = 0; i < points.length; ++i)
                    ctx.lineTo(i * step, mid - Math.max(0.5, points[i] * mid * 0.95))
                for (let i = points.length - 1; i >= 0; --i)
                    ctx.lineTo(i * step, mid + Math.max(0.5, points[i] * mid * 0.95))
                ctx.closePath()
                ctx.fill()
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: mouse => track.seekRequested(Math.max(0, Math.min(1, mouse.x / width)))
            }
        }
    }
}
