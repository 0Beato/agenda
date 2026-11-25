import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// A button that fills with the theme accent when it's the main action or a
// picked choice, drawn here so it looks the same in every Qt build.
Button {
    id: control
    property bool filled: false
    property color idleColor: "transparent"
    property color lineColor: "#2a2a2e"

    topInset: 0
    bottomInset: 0
    Material.roundedScale: Math.min(4, theme.radius)
    Material.foreground: filled ? theme.accentForeground : "#ececee"

    background: Rectangle {
        implicitWidth: 64
        implicitHeight: 34
        radius: Math.min(4, theme.radius)
        color: control.filled ? (control.down ? Qt.darker(theme.accent, 1.15) : control.hovered ? Qt.lighter(theme.accent, 1.08) : theme.accent)
             : control.down ? "#2a2a2e" : control.hovered ? "#232326" : control.idleColor
        border.width: control.filled ? 0 : 1
        border.color: control.lineColor
    }
}
