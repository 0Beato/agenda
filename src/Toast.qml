import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Rectangle {
    id: toast
    required property QtObject colors
    property string message: ""

    function show(text) {
        message = text
        visible = true
        hide.restart()
    }

    visible: false
    width: row.implicitWidth + 24
    height: 44
    radius: theme.radius
    color: colors.raised
    border.color: colors.line

    Timer { id: hide; interval: 6000; onTriggered: toast.visible = false }

    RowLayout {
        id: row
        anchors.centerIn: parent
        spacing: 12
        Label { text: toast.message; color: toast.colors.text }
        Button {
            text: qsTr("Undo")
            flat: true
            Material.foreground: theme.accent
            Material.roundedScale: Math.min(4, theme.radius)
            onClicked: { backend.undo(); toast.visible = false }
        }
    }
}
