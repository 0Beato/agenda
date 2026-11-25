import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Rectangle {
    id: root
    required property QtObject colors
    signal picked(string date)
    signal createRequested(string date)

    color: colors.panel
    radius: theme.radius
    border.color: colors.line

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 4

            Label {
                Layout.fillWidth: true
                text: backend.monthTitle
                font.pixelSize: 20
                font.weight: Font.DemiBold
                color: root.colors.text
                elide: Text.ElideRight
            }
            Button {
                text: qsTr("Today")
                flat: true
                Material.roundedScale: Math.min(4, theme.radius)
                onClicked: backend.goToday()
            }
            ToolButton {
                text: LayoutMirroring.enabled ? "›" : "‹"
                font.pixelSize: 22
                onClicked: backend.moveMonths(-1)
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Previous month (PgUp)")
            }
            ToolButton {
                text: LayoutMirroring.enabled ? "‹" : "›"
                font.pixelSize: 22
                onClicked: backend.moveMonths(1)
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Next month (PgDn)")
            }
        }

        Row {
            Layout.fillWidth: true
            Repeater {
                model: backend.weekdays
                Label {
                    required property string modelData
                    width: parent.width / 7
                    text: modelData
                    horizontalAlignment: Text.AlignHCenter
                    color: root.colors.muted
                    font.pixelSize: 12
                    font.capitalization: Font.Capitalize
                }
            }
        }

        Grid {
            id: grid
            Layout.fillWidth: true
            Layout.fillHeight: true
            columns: 7
            spacing: 4
            readonly property real cellWidth: (width - spacing * 6) / 7
            readonly property real cellHeight: (height - spacing * 5) / 6

            Repeater {
                model: backend.monthCells
                Rectangle {
                    id: cell
                    required property var modelData
                    width: grid.cellWidth
                    height: grid.cellHeight
                    radius: Math.min(4, theme.radius)
                    color: modelData.selected ? Qt.alpha(theme.accent, 0.16)
                         : hover.hovered ? root.colors.raised : "transparent"
                    border.width: modelData.selected ? 1 : 0
                    border.color: theme.accent

                    HoverHandler { id: hover }
                    TapHandler {
                        onTapped: root.picked(cell.modelData.date)
                        onDoubleTapped: root.createRequested(cell.modelData.date)
                    }

                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        anchors.topMargin: 6
                        width: 28
                        height: 28
                        radius: 14
                        color: cell.modelData.today ? theme.accent : "transparent"
                        Label {
                            anchors.centerIn: parent
                            text: cell.modelData.day
                            font.pixelSize: 14
                            font.weight: cell.modelData.today || cell.modelData.selected ? Font.Bold : Font.Normal
                            color: cell.modelData.today ? theme.accentForeground
                                 : cell.modelData.inMonth ? root.colors.text : Qt.alpha(root.colors.muted, 0.55)
                        }
                    }

                    Row {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 8
                        spacing: 4
                        visible: cell.modelData.count > 0
                        Repeater {
                            model: Math.min(cell.modelData.count, 3)
                            Rectangle {
                                width: 6; height: 6; radius: 3
                                color: cell.modelData.inMonth ? theme.accent : Qt.alpha(theme.accent, 0.45)
                            }
                        }
                        Label {
                            visible: cell.modelData.count > 3
                            text: "+"
                            color: theme.accent
                            font.pixelSize: 10
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }
            }
        }
    }
}
