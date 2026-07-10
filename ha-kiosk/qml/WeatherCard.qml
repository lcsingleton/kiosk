import QtQuick
import QtQuick.Layouts

// History flowing into the next-few-hours projection on one chart, 7-day
// outlook as a strip below.
DashboardCard {
    property var dashboardData

    title: "Weather · Ecowitt WH65LP"
    icon: "🌡️"
    accent: "#199e70"

    LineChart {
        width: parent.width
        lineColor: "#199e70"
        labelEvery: 3
        points: dashboardData.weatherSeries()
    }

    RowLayout {
        width: parent.width
        spacing: 26
        ColumnLayout {
            spacing: 1
            Text { text: "Humidity"; color: "#8296b8"; font.pixelSize: 12 }
            Text { text: "62%"; color: "#eef2f9"; font.pixelSize: 17; font.bold: true }
        }
        ColumnLayout {
            spacing: 1
            Text { text: "Wind"; color: "#8296b8"; font.pixelSize: 12 }
            Text { text: "14 km/h"; color: "#eef2f9"; font.pixelSize: 17; font.bold: true }
        }
        ColumnLayout {
            spacing: 1
            Text { text: "Rain today"; color: "#8296b8"; font.pixelSize: 12 }
            Text { text: "0.0 mm"; color: "#eef2f9"; font.pixelSize: 17; font.bold: true }
        }
    }

    FlowDivider { width: parent.width; color: "#199e70" }

    Flow {
        width: parent.width
        spacing: 6
        Repeater {
            model: dashboardData.forecast
            delegate: Column {
                spacing: 1
                width: 58
                Text { text: modelData.day; color: "#8296b8"; font.pixelSize: 11; anchors.horizontalCenter: parent.horizontalCenter }
                Text { text: modelData.icon; font.pixelSize: 18; anchors.horizontalCenter: parent.horizontalCenter }
                Text { text: modelData.hi; color: "#eef2f9"; font.pixelSize: 13; font.bold: true; anchors.horizontalCenter: parent.horizontalCenter }
                Text { text: modelData.lo; color: "#8296b8"; font.pixelSize: 11; anchors.horizontalCenter: parent.horizontalCenter }
            }
        }
    }
}
