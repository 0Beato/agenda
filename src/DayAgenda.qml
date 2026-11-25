import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Rectangle {
    id: root
    required property QtObject colors
    property int currentIndex: 0
    signal activated(int index)
    signal createRequested()

    color: colors.panel
    radius: theme.radius
    border.color: colors.line

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            Label {
                Layout.fillWidth: true
                text: backend.dayTitle
                font.pixelSize: 18
                font.weight: Font.DemiBold
                color: root.colors.text
                wrapMode: Text.Wrap
            }
            ChoiceButton {
                text: qsTr("New")
                filled: true
                leftPadding: 16; rightPadding: 16
                onClicked: root.createRequested()
                ToolTip.visible: hovered
                ToolTip.text: qsTr("New appointment (Space)")
            }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 8
            model: backend.dayEvents
            currentIndex: root.currentIndex
            boundsBehavior: Flickable.StopAtBounds

            delegate: Item {
                id: slot
                required property var modelData
                required property int index
                width: ListView.view.width
                height: card.implicitHeight

                Rectangle {
                    id: card
                    anchors.left: parent.left
                    anchors.right: parent.right
                    implicitHeight: content.implicitHeight + 20
                    radius: theme.radius
                    color: cardHover.hovered || slot.index === root.currentIndex ? root.colors.raised : Qt.alpha(root.colors.raised, 0.5)
                    border.width: slot.index === root.currentIndex ? 1 : 0
                    border.color: Qt.alpha(theme.accent, 0.7)
                    opacity: slot.modelData.past ? 0.55 : 1

                    HoverHandler { id: cardHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: root.activated(slot.index) }

                    Rectangle {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.margins: 8
                        width: 3
                        radius: 1
                        color: theme.accent
                    }

                    ColumnLayout {
                        id: content
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.leftMargin: 22
                        anchors.rightMargin: 12
                        anchors.topMargin: 10
                        spacing: 3

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Label {
                                text: slot.modelData.time
                                color: slot.modelData.now ? theme.accent : root.colors.muted
                                font.pixelSize: 13
                                font.weight: Font.Medium
                            }
                            Label {
                                visible: slot.modelData.now
                                text: qsTr("now")
                                color: theme.accentForeground
                                font.pixelSize: 11
                                font.weight: Font.Bold
                                leftPadding: 6; rightPadding: 6; topPadding: 1; bottomPadding: 1
                                background: Rectangle { color: theme.accent; radius: Math.min(4, theme.radius) }
                            }
                        }
                        Label {
                            Layout.fillWidth: true
                            text: slot.modelData.title
                            color: root.colors.text
                            font.pixelSize: 16
                            font.weight: Font.DemiBold
                            wrapMode: Text.Wrap
                        }
                        Label {
                            Layout.fillWidth: true
                            visible: slot.modelData.location !== ""
                            text: "  " + slot.modelData.location
                            color: root.colors.muted
                            elide: Text.ElideRight
                        }
                        Label {
                            Layout.fillWidth: true
                            text: "  " + slot.modelData.reminders
                                  + (slot.modelData.repeats ? "       " + slot.modelData.repeat : "")
                            color: slot.modelData.hasReminders ? Qt.alpha(theme.accent, 0.9) : root.colors.muted
                            font.pixelSize: 13
                            elide: Text.ElideRight
                        }
                        Label {
                            Layout.fillWidth: true
                            visible: slot.modelData.notes !== ""
                            text: slot.modelData.notes
                            color: root.colors.muted
                            font.pixelSize: 13
                            wrapMode: Text.Wrap
                            maximumLineCount: 3
                            elide: Text.ElideRight
                        }
                    }
                }
            }

            ColumnLayout {
                anchors.centerIn: parent
                visible: list.count === 0
                spacing: 8
                width: parent.width - 40
                Label {
                    Layout.fillWidth: true
                    text: qsTr("Nothing scheduled")
                    color: root.colors.text
                    font.pixelSize: 16
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                }
                Label {
                    Layout.fillWidth: true
                    text: qsTr("Press Space to add an appointment, or double-click a day.")
                    color: root.colors.muted
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                }
            }
        }
    }
}
