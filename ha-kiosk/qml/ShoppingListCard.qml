import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

DashboardCard {
    property var dashboardData

    title: "Shopping List"
    icon: "🛒"
    accent: Theme.accentAmber

    Repeater {
        model: dashboardData.shopping
        delegate: RowLayout {
            width: parent.width
            spacing: 12
            Rectangle {
                width: 20
                height: 20
                radius: 10
                color: modelData.done ? Theme.accentGreen : "transparent"
                border.color: modelData.done ? Theme.accentGreen : Theme.textSecondary
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
                color: modelData.done ? Theme.textSecondary : Theme.textPrimary
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
