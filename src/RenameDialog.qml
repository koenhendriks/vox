import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Dialog {
    id: dialog
    property int index: 0
    property string error

    title: qsTr("Rename profile")
    modal: true
    width: 380
    Material.roundedScale: theme.radius
    standardButtons: Dialog.Cancel | Dialog.Ok

    onAboutToShow: {
        error = ""
        field.text = backend.profileNames[index] ?? ""
        field.selectAll()
        field.forceActiveFocus()
    }
    // Ok only closes the dialog when the name was taken.
    onAccepted: {
        const problem = backend.renameProfile(index, field.text)
        if (problem.length) {
            error = problem
            open()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 8
        TextField {
            id: field
            Layout.fillWidth: true
            placeholderText: qsTr("Name")
            onAccepted: dialog.accept()
        }
        Label {
            visible: dialog.error.length > 0
            text: dialog.error
            color: "#f2a07b"
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
    }
}
