import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Rectangle {
    id: overlay
    required property QtObject colors
    signal dismissed()

    color: Qt.rgba(0, 0, 0, 0.6)

    TapHandler { onTapped: overlay.dismissed() }

    readonly property var shortcuts: [
        ["Space / N", qsTr("New appointment on the selected day")],
        ["Enter / E", qsTr("Edit the selected appointment")],
        ["J / K", qsTr("Next / previous appointment of the day")],
        ["Delete / X", qsTr("Delete the selected appointment")],
        ["← →", qsTr("Previous / next day")],
        ["↑ ↓", qsTr("Previous / next week")],
        ["PgUp / PgDn", qsTr("Previous / next month")],
        ["T", qsTr("Go to today")],
        ["Ctrl+Z", qsTr("Undo")],
        ["Ctrl+Shift+Z", qsTr("Redo")],
        ["Ctrl+Enter", qsTr("Save the appointment being edited")],
        ["Esc", qsTr("Close")],
        ["?", qsTr("Show or hide these shortcuts")],
        ["Q", qsTr("Quit")]
    ]

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(520, parent.width - 48)
        height: column.implicitHeight + 40
        radius: theme.radius
        color: overlay.colors.panel
        border.color: overlay.colors.line

        ColumnLayout {
            id: column
            anchors.fill: parent
            anchors.margins: 20
            spacing: 8

            Label {
                text: qsTr("Keyboard shortcuts")
                font.pixelSize: 18
                font.weight: Font.DemiBold
                color: overlay.colors.text
                Layout.bottomMargin: 6
            }

            Repeater {
                model: overlay.shortcuts
                RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: 16
                    Label {
                        Layout.preferredWidth: 130
                        text: modelData[0]
                        color: theme.accent
                        font.family: "monospace"
                    }
                    Label {
                        Layout.fillWidth: true
                        text: modelData[1]
                        color: overlay.colors.text
                        wrapMode: Text.Wrap
                    }
                }
            }
        }
    }
}
