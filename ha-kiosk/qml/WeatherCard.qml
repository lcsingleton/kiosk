import QtQuick
import QtQuick.Layouts

// History flowing into the next-few-hours projection on one chart, 7-day
// outlook as a strip below.
DashboardCard {
    property var dashboardData

    title: "Weather · Ecowitt WH65LP"
    icon: "🌡️"
    accent: Theme.accentTeal

    LineChart {
        width: parent.width
        lineColor: Theme.accentTeal
        points: dashboardData.weatherSeries()
    }

    RowLayout {
        width: parent.width
        spacing: 26
        ColumnLayout {
            spacing: 1
            Text { text: "Humidity"; color: Theme.textSecondary; font.pixelSize: 12 }
            Text { text: dashboardData.humidity; color: Theme.textPrimary; font.pixelSize: 17; font.bold: true }
        }
        ColumnLayout {
            spacing: 1
            Text { text: "Wind"; color: Theme.textSecondary; font.pixelSize: 12 }
            Text { text: dashboardData.windSpeed; color: Theme.textPrimary; font.pixelSize: 17; font.bold: true }
        }
        ColumnLayout {
            spacing: 1
            Text { text: "Rain today"; color: Theme.textSecondary; font.pixelSize: 12 }
            Text { text: dashboardData.rainToday; color: Theme.textPrimary; font.pixelSize: 17; font.bold: true }
        }
    }

    FlowDivider { width: parent.width; color: Theme.accentTeal }

    Flow {
        width: parent.width
        spacing: 6
        Repeater {
            model: dashboardData.forecast
            delegate: Column {
                spacing: 1
                width: 58
                Text { text: modelData.day; color: Theme.textSecondary; font.pixelSize: 11; anchors.horizontalCenter: parent.horizontalCenter }
                Text { text: modelData.icon; font.pixelSize: 18; anchors.horizontalCenter: parent.horizontalCenter }
                Text { text: modelData.hi; color: Theme.textPrimary; font.pixelSize: 13; font.bold: true; anchors.horizontalCenter: parent.horizontalCenter }
                Text { text: modelData.lo; color: Theme.textSecondary; font.pixelSize: 11; anchors.horizontalCenter: parent.horizontalCenter }
            }
        }
    }
}
