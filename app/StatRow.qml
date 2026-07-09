import QtQuick
import QtQuick.Layouts

RowLayout {
    id: root
    property string label: ""
    property string value: ""
    property color valueColor: "#eef2f9"

    width: parent ? parent.width : implicitWidth
    spacing: 8

    Text { text: root.label; color: "#8296b8"; font.pixelSize: 16; Layout.fillWidth: true }
    Text { text: root.value; color: root.valueColor; font.pixelSize: 17; font.bold: true }
}
