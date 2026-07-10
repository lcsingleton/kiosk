import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

DashboardCard {
    property var dashboardData

    title: "Shopping List"
    icon: "🛒"
    accent: "#c98500"

    Repeater {
        model: dashboardData.shopping
        delegate: RowLayout {
            width: parent.width
            spacing: 12
            Rectangle {
                width: 20
                height: 20
                radius: 10
                color: modelData.done ? "#0ca30c" : "transparent"
                border.color: modelData.done ? "#0ca30c" : "#8296b8"
                border.width: 2
                Text {
                    visible: modelData.done
                    anchors.centerIn: parent
                    text: "✓"
                    color: "white"
                    font.pixelSize: 12
                }
            }
            Text {
                text: modelData.item
                color: modelData.done ? "#8296b8" : "#eef2f9"
                font.strikeout: modelData.done
                font.pixelSize: 16
                Layout.fillWidth: true
            }
        }
    }

    TextField {
        width: parent.width
        placeholderText: "+ Add item"
    }
}
