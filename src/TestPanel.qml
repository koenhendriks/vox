import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Rectangle {
    id: panel
    readonly property bool recording: tester.state === "recording"
    readonly property bool ready: tester.state === "ready"
    readonly property real progress: tester.duration > 0 ? tester.position / tester.duration : 0

    function time(seconds) {
        const s = Math.max(0, Math.floor(seconds))
        return Math.floor(s / 60) + ":" + String(s % 60).padStart(2, "0")
    }
    function levels(loudness, peak) {
        return qsTr("Average %1 dB · peak %2 dB")
            .arg(win.format(loudness, 0)).arg(win.format(peak, 0))
    }

    implicitHeight: column.implicitHeight + 40
    color: "#17171a"
    border.color: recording ? "#e5484d" : "#28282e"
    radius: theme.radius

    ColumnLayout {
        id: column
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 20
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            Label {
                text: qsTr("Test recording")
                font.pixelSize: 13
                font.weight: Font.DemiBold
                font.capitalization: Font.AllUppercase
                font.letterSpacing: 1
                color: theme.accent
            }
            Label {
                visible: panel.ready
                text: qsTr("Recorded with %1").arg(tester.profileName)
                color: "#9b9ba3"
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            Item { Layout.fillWidth: true; visible: !panel.ready }
            ToolButton {
                text: "✕"
                focusPolicy: Qt.NoFocus
                Layout.preferredHeight: 32
                Layout.preferredWidth: 32
                topInset: 0
                bottomInset: 0
                onClicked: tester.close()
            }
        }

        Label {
            visible: tester.error.length > 0
            text: tester.error
            color: "#f2a07b"
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }

        // While recording: a clock, and the meters above show both levels.
        ColumnLayout {
            visible: panel.recording
            Layout.fillWidth: true
            spacing: 10
            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                Rectangle {
                    width: 10; height: 10; radius: 5
                    color: "#e5484d"
                    SequentialAnimation on opacity {
                        running: panel.recording
                        loops: Animation.Infinite
                        NumberAnimation { to: 0.25; duration: 600 }
                        NumberAnimation { to: 1; duration: 600 }
                    }
                }
                Label {
                    text: qsTr("Recording. Say a few sentences in your normal voice.")
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
                Label {
                    text: panel.time(tester.elapsed) + " / " + panel.time(tester.maxSeconds)
                    font.features: { "tnum": 1 }
                    color: "#d6d6dc"
                }
            }
            ProgressBar {
                Layout.fillWidth: true
                from: 0
                to: tester.maxSeconds
                value: tester.elapsed
            }
            RowLayout {
                Button {
                    text: qsTr("Stop")
                    focusPolicy: Qt.NoFocus
                    Material.background: theme.accent
                    Material.foreground: theme.accentForeground
                    onClicked: tester.stop()
                }
                Label {
                    text: qsTr("Your raw microphone and the Vox Microphone are recorded at the same time.")
                    color: "#9b9ba3"
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
            }
        }

        // When there's a take: both versions, one playhead.
        WaveformTrack {
            visible: panel.ready
            Layout.fillWidth: true
            label: qsTr("Raw microphone")
            detail: panel.levels(tester.rawLoudness, tester.rawPeak)
            curve: tester.rawCurve
            current: tester.track === 0
            playing: tester.playing
            progress: panel.progress
            tint: "#c9c9cf"
            onActivated: tester.track === 0 && tester.playing ? tester.togglePlay() : tester.playTrack(0)
            onSeekRequested: fraction => tester.playFrom(0, fraction)
        }
        WaveformTrack {
            visible: panel.ready
            Layout.fillWidth: true
            label: qsTr("Vox · %1").arg(tester.profileName)
            detail: panel.levels(tester.voxLoudness, tester.voxPeak)
            curve: tester.voxCurve
            current: tester.track === 1
            playing: tester.playing
            progress: panel.progress
            tint: theme.accent
            onActivated: tester.track === 1 && tester.playing ? tester.togglePlay() : tester.playTrack(1)
            onSeekRequested: fraction => tester.playFrom(1, fraction)
        }

        RowLayout {
            visible: panel.ready
            Layout.fillWidth: true
            spacing: 8
            Button {
                text: tester.playing ? qsTr("Pause") : qsTr("Play")
                focusPolicy: Qt.NoFocus
                Material.background: theme.accent
                Material.foreground: theme.accentForeground
                onClicked: tester.togglePlay()
            }
            Button {
                text: tester.track === 0 ? qsTr("Switch to Vox") : qsTr("Switch to raw")
                flat: true
                focusPolicy: Qt.NoFocus
                onClicked: tester.switchTrack()
            }
            Label {
                text: panel.time(tester.position) + " / " + panel.time(tester.duration)
                font.features: { "tnum": 1 }
                color: "#d6d6dc"
            }
            Label {
                text: qsTr("Click a curve to hear that version from there.")
                color: "#9b9ba3"
                font.pixelSize: 12
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            Button {
                text: qsTr("Record again")
                flat: true
                focusPolicy: Qt.NoFocus
                enabled: backend.running
                onClicked: tester.record()
            }
        }
    }
}
