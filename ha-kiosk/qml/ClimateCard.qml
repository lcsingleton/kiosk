import QtQuick
import QtQuick.Layouts

// Heater + AC as one system (only one mode is ever active in practice, so
// it's modeled and displayed as a single climate system, not two).
DashboardCard {
    property var dashboardData

    title: "Climate"
    icon: dashboardData.climate.mode === "heating" ? "🔥" : "🧊"
    accent: dashboardData.climate.mode === "heating" ? Theme.accentOrange : Theme.accentBlue

    RowLayout {
        width: parent.width
        spacing: 24

        Column {
            spacing: 2
            Row {
                spacing: 8
                Text {
                    text: dashboardData.climate.mode === "heating" ? "🔥" : "🧊"
                    font.pixelSize: 28
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    text: dashboardData.climate.target.toFixed(1) + "°"
                    color: dashboardData.climate.mode === "heating" ? Theme.accentOrange : Theme.accentBlue
                    font.pixelSize: 46
                    font.bold: true
                }
            }
            Text { text: "target"; color: Theme.textSecondary; font.pixelSize: 13 }
        }

        Column {
            spacing: 2
            Row {
                spacing: 4
                Text {
                    text: dashboardData.climate.mode === "heating" ? "▲" : "▼"
                    color: dashboardData.climate.mode === "heating" ? Theme.accentOrange : Theme.accentBlue
                    font.pixelSize: 15
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    text: dashboardData.climate.current.toFixed(1) + "°"
                    color: Theme.textTertiary
                    font.pixelSize: 22
                    font.bold: true
                }
            }
            Text { text: "current"; color: Theme.textSecondary; font.pixelSize: 13 }
        }

        Item { Layout.fillWidth: true }

        Row {
            spacing: 10

            Rectangle {
                id: powerBtn
                width: 44; height: 44; radius: 22
                color: dashboardData.climate.power ? Theme.surfaceAlt2 : Theme.surfaceAlt
                opacity: powerArea.pressed ? 0.7 : 1.0
                Text {
                    anchors.centerIn: parent
                    text: "⏻"
                    color: dashboardData.climate.power ? Theme.accentGreen : Theme.textSecondary
                    font.pixelSize: 18
                }
                MouseArea { id: powerArea; anchors.fill: parent }
            }
            Rectangle {
                width: 44; height: 44; radius: 22
                color: Theme.surfaceAlt
                opacity: downArea.pressed ? 0.7 : 1.0
                Text { anchors.centerIn: parent; text: "－"; color: Theme.textPrimary; font.pixelSize: 20 }
                MouseArea { id: downArea; anchors.fill: parent }
            }
            Rectangle {
                width: 44; height: 44; radius: 22
                color: Theme.surfaceAlt
                opacity: upArea.pressed ? 0.7 : 1.0
                Text { anchors.centerIn: parent; text: "＋"; color: Theme.textPrimary; font.pixelSize: 20 }
                MouseArea { id: upArea; anchors.fill: parent }
            }
        }
    }

    RowLayout {
        width: parent.width
        spacing: 30

        RowLayout {
            Layout.fillWidth: true
            Text { text: "Fan"; color: Theme.textSecondary; font.pixelSize: 15; Layout.fillWidth: true }
            Text { text: dashboardData.climate.fanSpeed; color: Theme.textPrimary; font.pixelSize: 16; font.bold: true }
        }
        Rectangle {
            radius: 12
            color: Theme.surfaceAlt
            implicitWidth: airModeText.implicitWidth + 24
            implicitHeight: 30
            Text {
                id: airModeText
                anchors.centerIn: parent
                text: dashboardData.climate.airMode
                color: Theme.textSecondary
                font.pixelSize: 13
            }
        }
    }

    StatRow {
        label: "Status"
        value: dashboardData.climate.mode === "heating" ? "Heating" : (dashboardData.climate.mode === "cooling" ? "Cooling" : "Idle")
        valueColor: Theme.accentGreen
    }
}
