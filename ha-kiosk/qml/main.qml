import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.VirtualKeyboard

Window {
    id: root
    visible: true
    width: 960
    height: 540
    color: "#05070c"
    title: "ha-tab kiosk (dev preview — resize me)"

    DashboardData { id: data }

    // Normalizes a tapped item from whichever list it came from
    // (todaySchedule uses "event" for its title field; weekend/upcoming use
    // "title") into the one shape EventEditPopup expects.
    function openEditPopup(item, titleKey) {
        eventEditPopup.isNew = false
        eventEditPopup.event = {
            calendarId: item.calendarId,
            eventId: item.eventId,
            etag: item.etag,
            title: item[titleKey],
            startIso: item.startIso,
            endIso: item.endIso
        }
        eventEditPopup.open()
    }

    function openCreatePopup(personName, approxHour) {
        const person = data.people.find(p => p.name === personName)
        eventEditPopup.isNew = true
        eventEditPopup.newCalendarId = person ? person.calendarId : ""
        eventEditPopup.event = { title: "" }
        eventEditPopup.newStartHour = approxHour
        eventEditPopup.open()
    }

    // Re-finds the same event by id across all three lists — used to keep
    // the open popup's etag current as the snapshot refreshes, so a second
    // action in the same popup session (e.g. Rename then Move) isn't
    // rejected as a conflict against an etag the first action already
    // advanced past.
    function findLiveEvent(eventId) {
        let found = data.todaySchedule.find(e => e.eventId === eventId)
        if (found) return { calendarId: found.calendarId, eventId: found.eventId, etag: found.etag, title: found.event, startIso: found.startIso, endIso: found.endIso }
        found = data.weekend.find(e => e.eventId === eventId)
        if (found) return { calendarId: found.calendarId, eventId: found.eventId, etag: found.etag, title: found.title, startIso: found.startIso, endIso: found.endIso }
        found = data.upcoming.find(e => e.eventId === eventId)
        if (found) return { calendarId: found.calendarId, eventId: found.eventId, etag: found.etag, title: found.title, startIso: found.startIso, endIso: found.endIso }
        return null
    }

    Connections {
        target: calendarBridge
        function onSnapshotChanged() {
            if (eventEditPopup.visible && !eventEditPopup.isNew && eventEditPopup.event) {
                const fresh = findLiveEvent(eventEditPopup.event.eventId)
                if (fresh)
                    eventEditPopup.event = fresh
            }
        }
        function onCommandFailed(commandId, what, errorCode, errorMessage) {
            errorBanner.show(what + ": " + errorMessage)
        }
    }

    // Fixed virtual resolution matching the tablet's portrait mount.
    // Uniformly scaled (never stretched) to fit however this dev window is
    // resized, and letterboxed/centered on whatever's left over.
    Item {
        id: canvas
        width: 1920
        height: 1080
        transformOrigin: Item.TopLeft
        scale: Math.min(root.width / width, root.height / height)
        x: (root.width - width * scale) / 2
        y: (root.height - height * scale) / 2

        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#070b14" }
                GradientStop { position: 1.0; color: "#0e1729" }
            }
        }

        // ---- header: clock + date + settings --------------------------
        Item {
            id: header
            width: parent.width
            height: 72

            Text {
                id: clock
                anchors.left: parent.left
                anchors.leftMargin: 24
                anchors.verticalCenter: parent.verticalCenter
                color: "#eef2f9"
                font.pixelSize: 28
                font.bold: true
            }
            Text {
                id: dateLabel
                anchors.left: clock.right
                anchors.leftMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                color: "#8296b8"
                font.pixelSize: 16
            }
            Text {
                anchors.right: parent.right
                anchors.rightMargin: 24
                anchors.verticalCenter: parent.verticalCenter
                text: "⚙"
                color: "#8296b8"
                font.pixelSize: 22
            }

            Timer {
                interval: 1000
                running: true
                repeat: true
                triggeredOnStart: true
                onTriggered: {
                    const now = new Date()
                    clock.text = Qt.formatDateTime(now, "h:mm AP")
                    dateLabel.text = Qt.formatDateTime(now, "dddd, MMMM d")
                }
            }
        }

        // ---- scrollable dashboard body ----------------------------------
        Flickable {
            id: flick
            anchors.top: header.bottom
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            contentHeight: dashboardGrid.height + 28
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            GridLayout {
                id: dashboardGrid
                x: 18
                y: 8
                width: flick.width - 36
                columns: 2
                columnSpacing: 14
                rowSpacing: 14

                // -- climate: heater + AC as one system. Paired with the
                // shopping list below (both compact, controls-first) so the
                // grid actually earns its two columns instead of every
                // section spanning full width like a single stacked feed --
                DashboardCard {
                    Layout.fillWidth: true
                    Layout.fillHeight: false
                    Layout.alignment: Qt.AlignTop
                    title: "Climate"
                    icon: data.climate.mode === "heating" ? "🔥" : "🧊"
                    accent: data.climate.mode === "heating" ? "#d95926" : "#3987e5"

                    RowLayout {
                        width: parent.width
                        spacing: 24

                        Column {
                            spacing: 2
                            Row {
                                spacing: 8
                                Text {
                                    text: data.climate.mode === "heating" ? "🔥" : "🧊"
                                    font.pixelSize: 28
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Text {
                                    text: data.climate.target.toFixed(1) + "°"
                                    color: data.climate.mode === "heating" ? "#d95926" : "#3987e5"
                                    font.pixelSize: 46
                                    font.bold: true
                                }
                            }
                            Text { text: "target"; color: "#8296b8"; font.pixelSize: 13 }
                        }

                        Column {
                            spacing: 2
                            Row {
                                spacing: 4
                                Text {
                                    text: data.climate.mode === "heating" ? "▲" : "▼"
                                    color: data.climate.mode === "heating" ? "#d95926" : "#3987e5"
                                    font.pixelSize: 15
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Text {
                                    text: data.climate.current.toFixed(1) + "°"
                                    color: "#c7d2e3"
                                    font.pixelSize: 22
                                    font.bold: true
                                }
                            }
                            Text { text: "current"; color: "#8296b8"; font.pixelSize: 13 }
                        }

                        Item { Layout.fillWidth: true }

                        Row {
                            spacing: 10

                            Rectangle {
                                id: powerBtn
                                width: 44; height: 44; radius: 22
                                color: data.climate.power ? "#1c3355" : "#16294a"
                                opacity: powerArea.pressed ? 0.7 : 1.0
                                Text {
                                    anchors.centerIn: parent
                                    text: "⏻"
                                    color: data.climate.power ? "#0ca30c" : "#8296b8"
                                    font.pixelSize: 18
                                }
                                MouseArea { id: powerArea; anchors.fill: parent }
                            }
                            Rectangle {
                                width: 44; height: 44; radius: 22
                                color: "#16294a"
                                opacity: downArea.pressed ? 0.7 : 1.0
                                Text { anchors.centerIn: parent; text: "－"; color: "#eef2f9"; font.pixelSize: 20 }
                                MouseArea { id: downArea; anchors.fill: parent }
                            }
                            Rectangle {
                                width: 44; height: 44; radius: 22
                                color: "#16294a"
                                opacity: upArea.pressed ? 0.7 : 1.0
                                Text { anchors.centerIn: parent; text: "＋"; color: "#eef2f9"; font.pixelSize: 20 }
                                MouseArea { id: upArea; anchors.fill: parent }
                            }
                        }
                    }

                    RowLayout {
                        width: parent.width
                        spacing: 30

                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: "Fan"; color: "#8296b8"; font.pixelSize: 15; Layout.fillWidth: true }
                            Text { text: data.climate.fanSpeed; color: "#eef2f9"; font.pixelSize: 16; font.bold: true }
                        }
                        Rectangle {
                            radius: 12
                            color: "#16294a"
                            implicitWidth: airModeText.implicitWidth + 24
                            implicitHeight: 30
                            Text {
                                id: airModeText
                                anchors.centerIn: parent
                                text: data.climate.airMode
                                color: "#8296b8"
                                font.pixelSize: 13
                            }
                        }
                    }

                    StatRow {
                        label: "Status"
                        value: data.climate.mode === "heating" ? "Heating" : (data.climate.mode === "cooling" ? "Cooling" : "Idle")
                        valueColor: "#0ca30c"
                    }
                }

                // -- shopping list: pairs with Climate above, same reasoning --
                DashboardCard {
                    Layout.fillWidth: true
                    Layout.fillHeight: false
                    Layout.alignment: Qt.AlignTop
                    title: "Shopping List"
                    icon: "🛒"
                    accent: "#c98500"

                    Repeater {
                        model: data.shopping
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

                // -- weather: history flowing into the next-few-hours
                // projection on one chart, 7-day outlook as a strip below --
                DashboardCard {
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    title: "Weather · Ecowitt WH65LP"
                    icon: "🌡️"
                    accent: "#199e70"

                    LineChart {
                        width: parent.width
                        lineColor: "#199e70"
                        labelEvery: 3
                        points: data.weatherSeries()
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
                            model: data.forecast
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

                // -- calendar: today (highlights + hourly grid) beside this
                // weekend + upcoming, so the whole picture is one glance --
                DashboardCard {
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    title: "Calendar"
                    icon: "📅"
                    accent: "#3987e5"
                    showDivider: false

                    Item {
                        id: calSplit
                        width: parent.width
                        height: Math.max(todayCol.height, sideCol.height)

                        // ---- today (left, wider — needs room for the grid) ----
                        Column {
                            id: todayCol
                            width: parent.width * 0.6 - 10
                            spacing: 8

                            Text { text: "TODAY"; color: "#5c6f8f"; font.pixelSize: 11; font.bold: true; font.letterSpacing: 1 }

                            Column {
                                width: parent.width
                                spacing: 4
                                Repeater {
                                    model: data.todayHighlights
                                    delegate: RowLayout {
                                        width: parent.width
                                        spacing: 8
                                        Text { text: modelData.icon; font.pixelSize: 16 }
                                        Text { text: modelData.label; color: "#c7d2e3"; font.pixelSize: 13; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                                        Text { text: modelData.time; color: "#8296b8"; font.pixelSize: 12; visible: !!modelData.time }
                                    }
                                }
                            }
                            
                            AgendaTimeline {
                                width: parent.width
                                people: data.people
                                items: data.todaySchedule
                                startHour: 6
                                endHour: 22
                                rowHeight: 30
                                windowHours: 6
                                gutterWidth: 44
                                onEventTapped: item => openEditPopup(item, "event")
                                onEmptySlotTapped: (person, hour) => openCreatePopup(person, hour)
                            }
                        }

                        Rectangle {
                            x: todayCol.width + 10
                            width: 1
                            height: parent.height
                            gradient: Gradient {
                                GradientStop { position: 0.0; color: "#003987e5" }
                                GradientStop { position: 0.15; color: "#663987e5" }
                                GradientStop { position: 0.85; color: "#663987e5" }
                                GradientStop { position: 1.0; color: "#003987e5" }
                            }
                        }

                        // ---- this weekend + upcoming (right, narrower) ----
                        Column {
                            id: sideCol
                            x: todayCol.width + 21
                            width: parent.width - todayCol.width - 21
                            spacing: 10

                            Text { text: "THIS WEEKEND"; color: "#5c6f8f"; font.pixelSize: 11; font.bold: true; font.letterSpacing: 1 }

                            Column {
                                width: parent.width
                                spacing: 8
                                Repeater {
                                    model: data.weekendGrouped()
                                    delegate: Column {
                                        width: parent.width
                                        spacing: 4

                                        Text { text: modelData.day; color: "#9085e9"; font.pixelSize: 12; font.bold: true }

                                        Repeater {
                                            model: modelData.items
                                            delegate: Item {
                                                width: parent.width
                                                height: weekendRow.implicitHeight
                                                RowLayout {
                                                    id: weekendRow
                                                    width: parent.width
                                                    spacing: 8
                                                    Rectangle { width: 3; height: 24; radius: 2; color: modelData.accent }
                                                    Text {
                                                        text: modelData.title
                                                        color: "#eef2f9"
                                                        font.pixelSize: 13
                                                        wrapMode: Text.WordWrap
                                                        Layout.fillWidth: true
                                                    }
                                                    Text { text: modelData.time; color: "#8296b8"; font.pixelSize: 11 }
                                                }
                                                MouseArea {
                                                    anchors.fill: parent
                                                    onClicked: openEditPopup(modelData, "title")
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            FlowDivider { width: parent.width; color: "#9085e9" }

                            Text { text: "UPCOMING"; color: "#5c6f8f"; font.pixelSize: 11; font.bold: true; font.letterSpacing: 1 }

                            Column {
                                width: parent.width
                                spacing: 6
                                Repeater {
                                    model: data.upcoming
                                    delegate: Item {
                                        width: parent.width
                                        height: upcomingRow.implicitHeight
                                        RowLayout {
                                            id: upcomingRow
                                            width: parent.width
                                            spacing: 10
                                            Item {
                                                width: 36; height: 36
                                                Glow { anchors.fill: parent; color: modelData.accent; intensity: 0.5 }
                                                Column {
                                                    anchors.centerIn: parent
                                                    spacing: 0
                                                    Text { text: modelData.month; color: modelData.accent; font.pixelSize: 9; font.bold: true; anchors.horizontalCenter: parent.horizontalCenter }
                                                    Text { text: modelData.day; color: "#eef2f9"; font.pixelSize: 14; font.bold: true; anchors.horizontalCenter: parent.horizontalCenter }
                                                }
                                            }
                                            Column {
                                                Layout.fillWidth: true
                                                spacing: 0
                                                Text { text: modelData.title; color: "#eef2f9"; font.pixelSize: 13; wrapMode: Text.WordWrap; width: parent.width }
                                                Text { text: modelData.time; color: "#8296b8"; font.pixelSize: 11; visible: !!modelData.time }
                                            }
                                        }
                                        MouseArea {
                                            anchors.fill: parent
                                            onClicked: openEditPopup(modelData, "title")
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        InputPanel {
            id: inputPanel
            z: 99
            y: Qt.inputMethod.visible ? canvas.height - height : canvas.height
            anchors.left: parent.left
            anchors.right: parent.right
        }

        ErrorBanner {
            id: errorBanner
            z: 100
            anchors.top: canvas.top
            anchors.left: canvas.left
            anchors.right: canvas.right
            anchors.topMargin: 84
            anchors.leftMargin: 16
            anchors.rightMargin: 16
        }

        EventEditPopup {
            id: eventEditPopup
            parent: canvas
        }
    }
}
