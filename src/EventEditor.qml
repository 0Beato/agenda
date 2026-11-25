import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Dialog {
    id: editor
    required property QtObject colors
    signal deleteRequested(string id, string title)
    signal discardRequested()

    property string eventId: ""
    property var reminders: []
    property string initial: ""
    readonly property bool dirty: opened && JSON.stringify(collect()) !== initial

    anchors.centerIn: parent
    width: Math.min(600, parent.width - 48)
    modal: true
    closePolicy: Popup.NoAutoClose
    padding: 20
    topPadding: 8

    background: Rectangle {
        color: editor.colors.panel
        radius: theme.radius
        border.color: editor.colors.line
    }
    header: Label {
        text: editor.eventId === "" ? qsTr("New appointment") : qsTr("Edit appointment")
        color: editor.colors.text
        font.pixelSize: 18
        font.weight: Font.DemiBold
        padding: 20
        bottomPadding: 4
    }

    function openWith(fields) {
        eventId = fields.id
        titleField.text = fields.title
        dateField.text = fields.date
        allDaySwitch.checked = fields.allDay
        startField.text = fields.start
        endField.text = fields.end
        locationField.text = fields.location
        notesField.text = fields.notes
        reminders = fields.reminders.slice()
        repeatBox.currentIndex = Math.max(0, backend.repeatChoices.findIndex(c => c.value === fields.repeat))
        errorLabel.text = ""
        initial = JSON.stringify(collect())
        open()
        titleField.forceActiveFocus()
    }

    function collect() {
        return {
            id: eventId,
            title: titleField.text,
            date: dateField.text,
            allDay: allDaySwitch.checked,
            start: startField.text,
            end: endField.text,
            location: locationField.text,
            notes: notesField.text,
            reminders: reminders.slice().sort((a, b) => a - b),
            repeat: backend.repeatChoices[repeatBox.currentIndex] ? backend.repeatChoices[repeatBox.currentIndex].value : "none"
        }
    }

    function save() {
        const error = backend.save(collect())
        if (error === "") close()
        else errorLabel.text = error
    }

    function cancel() {
        if (dirty) discardRequested()
        else close()
    }

    function toggle(minutes) {
        const next = reminders.slice()
        const at = next.indexOf(minutes)
        if (at >= 0) next.splice(at, 1)
        else next.push(minutes)
        reminders = next
    }

    // The standard choices, plus any other offset this appointment already
    // has (set from the command line), so nothing is hidden or lost.
    readonly property var reminderModel: {
        const list = backend.reminderChoices.slice()
        for (const m of reminders)
            if (!list.some(c => c.minutes === m))
                list.push({ minutes: m, label: backend.reminderLabel(m) })
        return list.sort((a, b) => a.minutes - b.minutes)
    }

    Shortcut { sequence: "Esc"; enabled: editor.opened; onActivated: editor.cancel() }
    Shortcut { sequences: ["Ctrl+Return", "Ctrl+Enter", StandardKey.Save]; enabled: editor.opened; onActivated: editor.save() }

    contentItem: ColumnLayout {
        spacing: 10

        TextField {
            id: titleField
            Layout.fillWidth: true
            placeholderText: qsTr("Title")
            font.pixelSize: 17
            onAccepted: editor.save()
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            TextField {
                id: dateField
                Layout.fillWidth: true
                Layout.preferredWidth: 140
                placeholderText: qsTr("Date")
                onAccepted: editor.save()
            }
            TextField {
                id: startField
                visible: !allDaySwitch.checked
                Layout.fillWidth: true
                Layout.preferredWidth: 90
                placeholderText: qsTr("Starts")
                inputMethodHints: Qt.ImhPreferNumbers
                onAccepted: editor.save()
            }
            TextField {
                id: endField
                visible: !allDaySwitch.checked
                Layout.fillWidth: true
                Layout.preferredWidth: 90
                placeholderText: qsTr("Ends")
                inputMethodHints: Qt.ImhPreferNumbers
                onAccepted: editor.save()
            }
            Switch {
                id: allDaySwitch
                text: qsTr("All day")
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            TextField {
                id: locationField
                Layout.fillWidth: true
                Layout.preferredWidth: 260
                placeholderText: qsTr("Place or link")
                onAccepted: editor.save()
            }
            ComboBox {
                id: repeatBox
                Layout.fillWidth: true
                Layout.preferredWidth: 160
                model: backend.repeatChoices
                textRole: "label"
            }
        }

        Label {
            text: allDaySwitch.checked ? qsTr("Remind me (counting from 9:00 on the day)") : qsTr("Remind me")
            color: editor.colors.muted
            font.pixelSize: 13
        }

        Flow {
            Layout.fillWidth: true
            spacing: 6
            Repeater {
                model: editor.reminderModel
                delegate: ChoiceButton {
                    required property var modelData
                    readonly property bool on: editor.reminders.indexOf(modelData.minutes) >= 0
                    text: modelData.label
                    filled: on
                    lineColor: editor.colors.line
                    font.pixelSize: 12
                    font.capitalization: Font.MixedCase
                    leftPadding: 10; rightPadding: 10
                    onClicked: editor.toggle(modelData.minutes)
                }
            }
        }

        TextArea {
            id: notesField
            Layout.fillWidth: true
            Layout.preferredHeight: 64
            placeholderText: qsTr("Notes")
            wrapMode: TextEdit.Wrap
        }

        Label {
            id: errorLabel
            Layout.fillWidth: true
            visible: text !== ""
            color: "#ff8a80"
            wrapMode: Text.Wrap
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Button {
                visible: editor.eventId !== ""
                text: qsTr("Delete")
                flat: true
                Material.foreground: "#ff8a80"
                Material.roundedScale: Math.min(4, theme.radius)
                onClicked: editor.deleteRequested(editor.eventId, titleField.text)
            }
            Item { Layout.fillWidth: true }
            Button {
                text: qsTr("Cancel")
                flat: true
                Material.roundedScale: Math.min(4, theme.radius)
                onClicked: editor.cancel()
            }
            ChoiceButton {
                text: qsTr("Save")
                filled: true
                leftPadding: 20; rightPadding: 20
                onClicked: editor.save()
            }
        }
    }
}
