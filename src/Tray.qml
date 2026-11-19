import QtQuick
import Qt.labs.platform

// The tray icon: whether Vox is on, and the profiles one click away.
SystemTrayIcon {
    id: tray
    visible: true

    readonly property string status: !backend.running ? "off" : backend.bypass ? "compare" : "on"
    readonly property string profile: backend.profileNames[backend.activeIndex] ?? ""

    icon.source: trayIcons.source(status, theme.accent)
    tooltip: !backend.running ? qsTr("Vox is off")
           : backend.bypass ? qsTr("Vox is comparing: apps hear your raw microphone")
           : backend.isDefault ? qsTr("Vox is on with %1, as your default microphone").arg(profile)
           : qsTr("Vox is on with %1").arg(profile)

    // Fixed for the life of the process: Qt decides it when the app starts.
    Component.onCompleted: shell.trayAvailable = available

    onActivated: reason => {
        if (reason === SystemTrayIcon.MiddleClick)
            backend.setRunning(!backend.running)
        else if (reason === SystemTrayIcon.Trigger || reason === SystemTrayIcon.DoubleClick)
            shell.toggleWindow()
    }

    menu: Menu {
        id: menu
        readonly property int profileStart: 4
        // The background poll is slow, so fetch the state before showing it.
        onAboutToShow: backend.refresh()

        MenuItem {
            enabled: false
            text: !backend.running ? qsTr("The Vox Microphone is off")
                : backend.isDefault ? qsTr("On, as your default microphone")
                : qsTr("On, not your default microphone")
        }
        MenuItem {
            text: qsTr("Vox Microphone on")
            checkable: true
            checked: backend.running
            onTriggered: backend.setRunning(!backend.running)
        }
        MenuItem {
            text: qsTr("Make it the default microphone")
            visible: backend.running && !backend.isDefault
            enabled: !backend.sourceMissing
            onTriggered: backend.makeDefault()
        }
        MenuSeparator {}

        MenuItemGroup { id: profiles; exclusive: true }
        // Profile items go between the two separators, after the status line,
        // the on/off toggle, the default item and the first separator.
        Instantiator {
            model: backend.profileNames
            delegate: MenuItem {
                required property int index
                required property string modelData
                text: modelData
                checkable: true
                checked: index === backend.activeIndex
                group: profiles
                onTriggered: backend.selectProfile(index)
            }
            onObjectAdded: (index, object) => menu.insertItem(menu.profileStart + index, object)
            onObjectRemoved: (index, object) => menu.removeItem(object)
        }

        MenuSeparator {}
        MenuItem {
            text: qsTr("Open Vox")
            onTriggered: shell.showWindow()
        }
        MenuItem {
            text: qsTr("Quit Vox (the microphone keeps running)")
            onTriggered: shell.quit()
        }
    }
}
