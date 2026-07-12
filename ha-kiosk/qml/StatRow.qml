import QtQuick
import QtQuick.Layouts

RowLayout {
    id: root
    property string label: ""
    property string value: ""
    property color valueColor: Theme.textPrimary

    width: parent ? parent.width : implicitWidth
    spacing: 8

    Text { text: root.label; color: Theme.textSecondary; font.pixelSize: 16; Layout.fillWidth: true }
    Text { text: root.value; color: root.valueColor; font.pixelSize: 17; font.bold: true }
}
