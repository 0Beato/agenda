import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 1040
    height: 700
    minimumWidth: 720
    minimumHeight: 520
    visible: true
    title: "Agenda"
    color: palette_.window

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.background: palette_.panel
    Material.roundedScale: theme.radius

    QtObject {
        id: palette_
        readonly property color window: "#0e0e10"
        readonly property color panel: "#161618"
        readonly property color raised: "#1e1e21"
        readonly property color line: "#2a2a2e"
        readonly property color text: "#ececee"
        readonly property color muted: "#8d8d95"
    }

    property bool helpVisible: false
    property int eventIndex: 0
    readonly property bool typing: editor.opened || discard.opened
    readonly property bool idle: !typing && !helpVisible
    readonly property var selectedEvent: backend.dayEvents.length > 0
        ? backend.dayEvents[Math.min(eventIndex, backend.dayEvents.length - 1)] : null

    onSelectedEventChanged: if (eventIndex >= backend.dayEvents.length) eventIndex = Math.max(0, backend.dayEvents.length - 1)

    function newAppointment(isoDate) {
        if (isoDate) backend.select(isoDate)
        editor.openWith(backend.draft())
    }
    function editSelected() {
        if (selectedEvent) editor.openWith(backend.eventById(selectedEvent.id))
    }
    function removeEvent(id, title) {
        if (backend.remove(id))
            toast.show(qsTr("Deleted “%1”").arg(title))
    }
    function quit() {
        if (editor.opened && editor.dirty) discard.ask(function() { Qt.quit() })
        else Qt.quit()
    }

    onClosing: function(close) {
        if (editor.opened && editor.dirty) {
            close.accepted = false
            discard.ask(function() { editor.close(); Qt.quit() })
        }
    }

    // Single keys only act when nothing is being typed into.
    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: win.helpVisible = !win.helpVisible }
    Shortcut { sequence: "Esc"; context: Qt.ApplicationShortcut; enabled: win.helpVisible; onActivated: win.helpVisible = false }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: win.quit() }
    Shortcut { sequences: ["Space", "N"]; context: Qt.ApplicationShortcut; enabled: win.idle; onActivated: win.newAppointment() }
    Shortcut { sequences: ["Return", "Enter", "E"]; context: Qt.ApplicationShortcut; enabled: win.idle; onActivated: win.editSelected() }
    Shortcut { sequences: ["Delete", "X"]; context: Qt.ApplicationShortcut; enabled: win.idle && win.selectedEvent !== null; onActivated: win.removeEvent(win.selectedEvent.id, win.selectedEvent.title) }
    Shortcut { sequence: "Left"; context: Qt.ApplicationShortcut; enabled: win.idle; onActivated: { backend.moveDays(-1); win.eventIndex = 0 } }
    Shortcut { sequence: "Right"; context: Qt.ApplicationShortcut; enabled: win.idle; onActivated: { backend.moveDays(1); win.eventIndex = 0 } }
    Shortcut { sequence: "Up"; context: Qt.ApplicationShortcut; enabled: win.idle; onActivated: { backend.moveDays(-7); win.eventIndex = 0 } }
    Shortcut { sequence: "Down"; context: Qt.ApplicationShortcut; enabled: win.idle; onActivated: { backend.moveDays(7); win.eventIndex = 0 } }
    Shortcut { sequence: "PgUp"; context: Qt.ApplicationShortcut; enabled: win.idle; onActivated: { backend.moveMonths(-1); win.eventIndex = 0 } }
    Shortcut { sequence: "PgDown"; context: Qt.ApplicationShortcut; enabled: win.idle; onActivated: { backend.moveMonths(1); win.eventIndex = 0 } }
    Shortcut { sequence: "T"; context: Qt.ApplicationShortcut; enabled: win.idle; onActivated: { backend.goToday(); win.eventIndex = 0 } }
    Shortcut { sequence: "J"; context: Qt.ApplicationShortcut; enabled: win.idle; onActivated: win.eventIndex = Math.min(win.eventIndex + 1, Math.max(0, backend.dayEvents.length - 1)) }
    Shortcut { sequence: "K"; context: Qt.ApplicationShortcut; enabled: win.idle; onActivated: win.eventIndex = Math.max(0, win.eventIndex - 1) }
    Shortcut { sequences: [StandardKey.Undo]; context: Qt.ApplicationShortcut; enabled: win.idle; onActivated: backend.undo() }
    Shortcut { sequences: [StandardKey.Redo, "Ctrl+Shift+Z"]; context: Qt.ApplicationShortcut; enabled: win.idle; onActivated: backend.redo() }

    // Right-to-left languages mirror the window; the Translator sets the
    // application's layout direction.
    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 12

            Rectangle {
                Layout.fillWidth: true
                visible: backend.problem !== ""
                implicitHeight: problemText.implicitHeight + 20
                radius: theme.radius
                color: "#3a1d1d"
                border.color: "#7a3434"
                Label {
                    id: problemText
                    anchors.fill: parent
                    anchors.margins: 10
                    text: backend.problem
                    color: "#ffb4b4"
                    wrapMode: Text.Wrap
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 16

                // The month and the day share the width 11 to 9.
                ColumnLayout {
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    Layout.preferredWidth: 100
                    Layout.horizontalStretchFactor: 11
                    Layout.minimumWidth: 380
                    spacing: 16

                    CalendarMonth {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        colors: palette_
                        onPicked: function(date) { backend.select(date); win.eventIndex = 0 }
                        onCreateRequested: function(date) { win.newAppointment(date) }
                    }

                    UpcomingList {
                        Layout.fillWidth: true
                        colors: palette_
                        onPicked: function(date) { backend.select(date); win.eventIndex = 0 }
                    }
                }

                DayAgenda {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredWidth: 100
                    Layout.horizontalStretchFactor: 9
                    Layout.minimumWidth: 300
                    colors: palette_
                    currentIndex: win.eventIndex
                    onActivated: function(index) { win.eventIndex = index; win.editSelected() }
                    onCreateRequested: win.newAppointment()
                }
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("Space new · Enter edit · ←→↑↓ move · T today · ? all shortcuts")
                color: palette_.muted
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
            }
        }

        EventEditor {
            id: editor
            colors: palette_
            onDeleteRequested: function(id, title) { editor.close(); win.removeEvent(id, title) }
            onDiscardRequested: discard.ask(function() { editor.close() })
        }

        Dialog {
            id: discard
            property var onConfirm: null
            function ask(action) { onConfirm = action; open() }
            anchors.centerIn: parent
            modal: true
            padding: 20
            background: Rectangle { color: palette_.raised; radius: theme.radius; border.color: palette_.line }
            header: Label {
                text: qsTr("Discard changes?")
                color: palette_.text
                font.pixelSize: 18
                font.weight: Font.DemiBold
                padding: 20
                bottomPadding: 0
            }
            Label { text: qsTr("The changes to this appointment haven't been saved."); color: palette_.muted }
            footer: DialogButtonBox {
                background: null
                Button { text: qsTr("Keep editing"); flat: true; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
                Button { text: qsTr("Discard"); flat: true; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
            }
            onAccepted: if (onConfirm) onConfirm()
        }

        Toast {
            id: toast
            colors: palette_
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 40
        }

        HelpOverlay {
            anchors.fill: parent
            visible: win.helpVisible
            colors: palette_
            onDismissed: win.helpVisible = false
        }
    }
}
