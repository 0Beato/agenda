import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Rectangle {
    id: root
    required property QtObject colors
    signal picked(string date)

    color: colors.panel
    radius: theme.radius
    border.color: colors.line
    implicitHeight: column.implicitHeight + 28

    ColumnLayout {
        id: column
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 14
        spacing: 6

        Label {
            text: qsTr("Coming up")
            font.pixelSize: 13
            font.weight: Font.DemiBold
            color: root.colors.muted
            font.capitalization: Font.AllUppercase
        }

        Label {
            visible: backend.upcoming.length === 0
            text: qsTr("Nothing in the next two weeks.")
            color: root.colors.muted
        }

        Repeater {
            model: backend.upcoming
            delegate: Rectangle {
                id: row
                required property var modelData
                Layout.fillWidth: true
                implicitHeight: 34
                radius: Math.min(4, theme.radius)
                color: rowHover.hovered ? root.colors.raised : "transparent"
                HoverHandler { id: rowHover }
                TapHandler { onTapped: root.picked(row.modelData.date) }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 6
                    anchors.rightMargin: 6
                    spacing: 10
                    Rectangle {
                        width: 3; Layout.preferredHeight: 18
                        color: row.modelData.now ? theme.accent : Qt.alpha(theme.accent, 0.6)
                    }
                    Label {
                        Layout.fillWidth: true
                        text: row.modelData.title
                        color: root.colors.text
                        elide: Text.ElideRight
                    }
                    Label {
                        Layout.maximumWidth: root.width * 0.55
                        text: row.modelData.when
                        color: row.modelData.now ? theme.accent : root.colors.muted
                        font.pixelSize: 13
                        elide: Text.ElideLeft
                    }
                }
            }
        }
    }
}
