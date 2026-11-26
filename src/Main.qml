import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 1040
    height: 720
    minimumWidth: 780
    minimumHeight: 560
    visible: true
    title: "Vox"
    color: "#0e0e10"

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius

    readonly property color card: "#17171a"
    readonly property color line: "#28282e"
    readonly property color dim: "#9b9ba3"
    property bool helpVisible: false
    property string toast: ""

    // Meters run only while the window is in front, so Vox doesn't keep the
    // microphone open in the background just to draw bars nobody sees.
    readonly property bool metering: win.visible && win.active && win.visibility !== Window.Minimized
    onMeteringChanged: backend.setMetering(metering)
    Component.onCompleted: {
        backend.setMetering(metering)
        backend.setForeground(visible)
    }

    // With a tray icon, closing only hides the window; Vox lives on in the
    // tray. A test in progress stops, and playback pauses.
    onClosing: close => {
        if (shell.trayAvailable) {
            close.accepted = false
            shell.hideWindow()
        }
    }
    onVisibleChanged: {
        backend.setForeground(visible)
        if (visible)
            return
        if (tester.state === "recording")
            tester.stop()
        if (tester.playing)
            tester.togglePlay()
    }

    function showToast(text) {
        toast = text
        toastTimer.restart()
    }
    function format(value, decimals) {
        return Number(value).toLocaleString(Qt.locale(), 'f', decimals)
    }
    function renameCurrent() {
        renameDialog.kind = "profile"
        renameDialog.index = backend.activeIndex
        renameDialog.open()
    }
    function deleteCurrent() {
        const name = backend.profileNames[backend.activeIndex]
        if (backend.deleteProfile(backend.activeIndex))
            showToast(qsTr("Deleted %1. Ctrl+Z brings it back.").arg(name))
        else
            showToast(qsTr("Vox needs at least one profile."))
    }
    function selectNumber(n) {
        if (n <= backend.profileNames.length)
            backend.selectProfile(n - 1)
    }

    readonly property bool typing: renameDialog.visible

    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: win.helpVisible = !win.helpVisible }
    Shortcut { sequence: "Esc"; context: Qt.ApplicationShortcut; enabled: win.helpVisible || tester.state !== "idle"; onActivated: win.helpVisible ? win.helpVisible = false : tester.close() }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: shell.hideWindow() }
    Shortcut { sequence: "Space"; context: Qt.ApplicationShortcut; enabled: !win.typing && tester.state !== "recording"; onActivated: backend.toggleBypass() }
    Shortcut { sequence: "T"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: tester.toggleRecording() }
    Shortcut { sequence: "P"; context: Qt.ApplicationShortcut; enabled: !win.typing && tester.state === "ready"; onActivated: tester.togglePlay() }
    Shortcut { sequence: "Tab"; context: Qt.ApplicationShortcut; enabled: !win.typing && tester.state === "ready"; onActivated: tester.switchTrack() }
    Shortcut { sequence: "L"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: backend.toggleListening() }
    Shortcut { sequence: "E"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: backend.edit("effects", !backend.current.effects) }
    Shortcut { sequence: "S"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: soundboard.stopAll() }
    Repeater {
        model: 9
        Item {
            Shortcut { sequence: "Ctrl+" + (index + 1); context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: soundboard.play(index) }
        }
    }
    Shortcut { sequence: "O"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: backend.setRunning(!backend.running) }
    Shortcut { sequence: "D"; context: Qt.ApplicationShortcut; enabled: !win.typing && backend.running; onActivated: backend.makeDefault() }
    Shortcut { sequence: "N"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: { backend.newProfile(); win.renameCurrent() } }
    Shortcut { sequences: ["R", "F2"]; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: win.renameCurrent() }
    Shortcut { sequence: "Delete"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: win.deleteCurrent() }
    Shortcut { sequence: "Up"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: backend.selectProfile(Math.max(0, backend.activeIndex - 1)) }
    Shortcut { sequence: "Down"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: backend.selectProfile(Math.min(backend.profileNames.length - 1, backend.activeIndex + 1)) }
    Shortcut { sequences: [StandardKey.Undo]; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: backend.undo() }
    Shortcut { sequences: [StandardKey.Redo, "Ctrl+Shift+Z"]; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: backend.redo() }
    Repeater {
        model: 9
        Item {
            Shortcut { sequence: String(index + 1); context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: win.selectNumber(index + 1) }
        }
    }

    Timer { id: toastTimer; interval: 3500; onTriggered: win.toast = "" }

    // Right-to-left languages mirror the window; the Translator sets the
    // application's layout direction.
    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // Header: what Vox is doing, and the switch that does it.
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 76
                color: win.card

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 24
                    anchors.rightMargin: 24
                    spacing: 16

                    Rectangle {
                        width: 12; height: 12; radius: 6
                        color: backend.running ? (backend.bypass ? "#d0a030" : theme.accent) : "#55555c"
                    }
                    ColumnLayout {
                        spacing: 2
                        Layout.fillWidth: true
                        Label {
                            text: backend.running
                                  ? (backend.bypass ? qsTr("Comparing: apps hear your raw microphone")
                                                    : qsTr("The Vox Microphone is on"))
                                  : qsTr("The Vox Microphone is off")
                            font.pixelSize: 18
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        Label {
                            text: !backend.running
                                  ? qsTr("Turn it on to use your improved voice in any app.")
                                  : backend.isDefault
                                    ? qsTr("It's your default microphone, so apps use it automatically.")
                                    : qsTr("Pick \"Vox Microphone\" in your apps, or make it the default.")
                            color: win.dim
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }
                    Button {
                        visible: backend.running && !backend.isDefault
                        enabled: !backend.sourceMissing
                        text: qsTr("Make default")
                        Material.background: theme.accent
                        Material.foreground: theme.accentForeground
                        focusPolicy: Qt.NoFocus
                        onClicked: backend.makeDefault()
                    }
                    Button {
                        text: backend.running ? qsTr("Turn off") : qsTr("Turn on")
                        flat: backend.running
                        Material.background: backend.running ? "transparent" : theme.accent
                        Material.foreground: backend.running ? "#e8e8ea" : theme.accentForeground
                        focusPolicy: Qt.NoFocus
                        onClicked: backend.setRunning(!backend.running)
                    }
                }
                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: win.line }
            }

            // Problems are said in the window, never worked around quietly.
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: problemText.implicitHeight + 24
                visible: backend.problem.length > 0
                color: "#2a1b14"
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 24
                    anchors.rightMargin: 12
                    Label {
                        id: problemText
                        text: backend.problem
                        wrapMode: Text.Wrap
                        color: "#f2c6a8"
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                    }
                    ToolButton {
                        text: "✕"
                        focusPolicy: Qt.NoFocus
                        onClicked: backend.dismissProblem()
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                ColumnLayout {
                    Layout.preferredWidth: 230
                    Layout.maximumWidth: 230
                    Layout.fillHeight: true
                    spacing: 0
                    ProfileList {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        // Room for the header, three profiles and the buttons
                        // however many sounds there are.
                        Layout.minimumHeight: 260
                        onRenameRequested: win.renameCurrent()
                        onDeleteRequested: win.deleteCurrent()
                        onNewRequested: { backend.newProfile(); win.renameCurrent() }
                    }
                    Rectangle { Layout.fillWidth: true; height: 1; color: win.line }
                    SoundboardPanel {
                        Layout.fillWidth: true
                        Layout.preferredHeight: implicitHeight
                        Layout.minimumHeight: 100
                        onRenameRequested: index => {
                            renameDialog.kind = "sound"
                            renameDialog.index = index
                            renameDialog.open()
                        }
                    }
                }
                Rectangle { Layout.fillHeight: true; width: 1; color: win.line }

                ScrollView {
                    id: scroller
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    contentWidth: availableWidth
                    clip: true

                    ColumnLayout {
                        width: scroller.availableWidth
                        spacing: 16

                        Item { Layout.preferredHeight: 4 }

                        TestPanel {
                            visible: tester.state !== "idle"
                            Layout.leftMargin: 24
                            Layout.rightMargin: 24
                            Layout.fillWidth: true
                        }
                        Connections {
                            target: tester
                            function onStateChanged() {
                                if (tester.state === "recording")
                                    scroller.contentItem.contentY = 0
                            }
                        }

                        GridLayout {
                            Layout.leftMargin: 24
                            Layout.rightMargin: 24
                            Layout.fillWidth: true
                            columns: scroller.availableWidth > 700 ? 2 : 1
                            columnSpacing: 16
                            rowSpacing: 16
                            LevelMeter {
                                Layout.fillWidth: true
                                label: qsTr("Your microphone")
                                level: backend.micLevel
                                active: backend.microphoneConnected && win.metering
                                idleText: win.metering ? qsTr("Not connected") : qsTr("Paused")

                                ComboBox {
                                    id: microphonePicker
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 40
                                    focusPolicy: Qt.NoFocus
                                    model: backend.microphones
                                    textRole: "description"
                                    valueRole: "name"
                                    displayText: currentIndex >= 0 ? currentText : qsTr("Pick a microphone")
                                    currentIndex: {
                                        const list = backend.microphones
                                        for (let i = 0; i < list.length; ++i)
                                            if (list[i].name === backend.microphone)
                                                return i
                                        return -1
                                    }
                                    onActivated: index => backend.setMicrophone(valueAt(index))
                                }
                            }
                            LevelMeter {
                                Layout.fillWidth: true
                                label: qsTr("Vox Microphone")
                                level: backend.voxLevel
                                active: backend.running && win.metering
                                idleText: !backend.running ? qsTr("Off") : qsTr("Paused")
                                highlighted: true
                            }
                        }

                        GridLayout {
                            Layout.leftMargin: 24
                            Layout.rightMargin: 24
                            Layout.bottomMargin: 24
                            Layout.fillWidth: true
                            id: cards
                            columns: scroller.availableWidth > 740 ? 3 : 2
                            columnSpacing: 16
                            rowSpacing: 16

                            ControlGroup {
                                title: qsTr("Clean up")
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                Layout.preferredWidth: 1

                                RowLayout {
                                    Layout.fillWidth: true
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2
                                        Label { text: qsTr("Noise removal"); font.weight: Font.Medium }
                                        Label {
                                            text: qsTr("Removes fans, typing and room noise.")
                                            color: win.dim
                                            font.pixelSize: 12
                                            wrapMode: Text.Wrap
                                            Layout.fillWidth: true
                                        }
                                    }
                                    Switch {
                                        focusPolicy: Qt.NoFocus
                                        checked: backend.current.denoise
                                        onToggled: backend.edit("denoise", checked)
                                    }
                                }
                                ControlSlider {
                                    key: "gate"; label: qsTr("Voice gate"); unit: "%"
                                    hint: qsTr("Mutes the gaps between words. Raise it in loud rooms.")
                                    enabled: backend.current.denoise
                                }
                                ControlSlider {
                                    key: "rumble"; label: qsTr("Rumble filter"); unit: "Hz"
                                    hint: qsTr("Cuts low rumble from desks, traffic and air conditioning.")
                                }
                            }

                            ControlGroup {
                                title: qsTr("Tone")
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                Layout.preferredWidth: 1

                                ControlSlider {
                                    key: "bass"; label: qsTr("Bass"); unit: "dB"; signed: true
                                    hint: qsTr("Adds depth and warmth for a fuller voice.")
                                }
                                ControlSlider {
                                    key: "mud"; label: qsTr("Mud"); unit: "dB"; signed: true
                                    hint: qsTr("Cut it to sound less boxy and muffled.")
                                }
                                ControlSlider {
                                    key: "presence"; label: qsTr("Presence"); unit: "dB"; signed: true
                                    hint: qsTr("Brings your voice forward so words are easy to follow.")
                                }
                                ControlSlider {
                                    key: "air"; label: qsTr("Air"); unit: "dB"; signed: true
                                    hint: qsTr("Adds openness and sparkle on top.")
                                }
                            }

                            // Loudness is short, so Effects stacks under it in the third
                            // column and shows without scrolling; with two columns the
                            // stack spans the second row.
                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                Layout.preferredWidth: 1
                                Layout.alignment: Qt.AlignTop
                                Layout.columnSpan: cards.columns === 3 ? 1 : 2
                                spacing: 16

                                ControlGroup {
                                    title: qsTr("Loudness")
                                    Layout.fillWidth: true
                                    Layout.alignment: Qt.AlignTop

                                    ControlSlider {
                                        key: "compression"; label: qsTr("Compression"); unit: "%"
                                        hint: qsTr("Evens out loud and quiet moments, like a radio voice.")
                                    }
                                    ControlSlider {
                                        key: "output"; label: qsTr("Output"); unit: "dB"; signed: true
                                        hint: qsTr("Makes Vox louder or quieter. A limiter keeps it from clipping.")
                                    }
                                }

                                ControlGroup {
                                    title: qsTr("Effects")
                                    toggleable: true
                                    checked: backend.current.effects
                                    onToggled: checked => backend.edit("effects", checked)
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    Layout.preferredWidth: 1

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 18
                                        enabled: backend.current.effects
                                        opacity: enabled ? 1 : 0.45

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 2
                                            Label { text: qsTr("Auto-tune"); font.weight: Font.Medium }
                                            Label {
                                                text: qsTr("Snaps your voice to a few notes. Fewer notes make bigger, wilder jumps.")
                                                color: win.dim
                                                font.pixelSize: 12
                                                wrapMode: Text.Wrap
                                                Layout.fillWidth: true
                                            }
                                        }
                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 4
                                            Label { text: qsTr("Notes"); font.weight: Font.Medium }
                                            ComboBox {
                                                Layout.fillWidth: true
                                                Layout.preferredHeight: 40
                                                focusPolicy: Qt.NoFocus
                                                // Chain::noteMask's order: the fewer notes, the bigger the
                                                // jumps, so the wildest come first.
                                                model: [qsTr("Robot: one note"), qsTr("Major chord: three notes"), qsTr("Minor chord: three notes"),
                                                        qsTr("Major key: seven notes"), qsTr("Minor key: seven notes"), qsTr("Every note: subtle")]
                                                currentIndex: backend.current.autotuneNotes ?? 1
                                                onActivated: index => backend.edit("autotuneNotes", index)
                                            }
                                        }
                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 4
                                            enabled: (backend.current.autotuneNotes ?? 1) !== 5
                                            Label { text: qsTr("Root note"); font.weight: Font.Medium }
                                            ComboBox {
                                                Layout.fillWidth: true
                                                Layout.preferredHeight: 40
                                                focusPolicy: Qt.NoFocus
                                                model: ["C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"]
                                                currentIndex: backend.current.autotuneRoot ?? 9
                                                onActivated: index => backend.edit("autotuneRoot", index)
                                            }
                                        }
                                        ControlSlider {
                                            key: "autotuneSpeed"; label: qsTr("Retune speed"); unit: "%"
                                            hint: qsTr("At 100% it jumps between notes at once: the over-the-top sound. Lower glides more naturally.")
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Footer: the two ways to hear what Vox does.
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 56
                color: win.card
                Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: win.line }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    spacing: 8

                    Button {
                        text: backend.listening ? qsTr("Stop listening") : qsTr("Listen to yourself")
                        checkable: true
                        checked: backend.listening
                        Material.background: checked ? Qt.alpha(theme.accent, 0.22) : "#232328"
                        enabled: backend.running
                        focusPolicy: Qt.NoFocus
                        onClicked: backend.toggleListening()
                    }
                    Button {
                        text: backend.bypass ? qsTr("Back to Vox") : qsTr("Compare with raw")
                        checkable: true
                        checked: backend.bypass
                        Material.background: checked ? Qt.alpha(theme.accent, 0.22) : "#232328"
                        enabled: backend.running
                        focusPolicy: Qt.NoFocus
                        onClicked: backend.toggleBypass()
                    }
                    Button {
                        text: tester.state === "recording" ? qsTr("Stop recording") : qsTr("Record a test")
                        enabled: backend.running
                        focusPolicy: Qt.NoFocus
                        Material.background: tester.state === "recording" ? "#e5484d" : "#232328"
                        Material.foreground: "#f2f2f4"
                        onClicked: tester.toggleRecording()
                    }
                    Label {
                        visible: backend.listening
                        text: qsTr("Use headphones, or your speakers will echo.")
                        color: win.dim
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Label {
                        visible: !backend.listening
                        text: qsTr("Press ? for shortcuts")
                        color: win.dim
                        elide: Text.ElideRight
                        horizontalAlignment: Text.AlignRight
                        Layout.fillWidth: true
                    }
                }
            }
        }

        Rectangle {
            visible: win.toast.length > 0
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 72
            width: toastLabel.implicitWidth + 32
            height: toastLabel.implicitHeight + 20
            radius: Math.min(4, theme.radius)
            color: "#2b2b31"
            border.color: win.line
            Label { id: toastLabel; anchors.centerIn: parent; text: win.toast }
        }

        HelpOverlay {
            anchors.fill: parent
            visible: win.helpVisible
            onCloseRequested: win.helpVisible = false
        }
    }

    RenameDialog {
        id: renameDialog
        anchors.centerIn: parent
    }
}
