import QtQuick
import QtQuick.Layouts

// Today (highlights + hourly grid) beside this weekend + upcoming, so the
// whole picture is one glance. Doesn't own popup state — it just reports
// taps back up to whoever owns the EventEditPopup.
DashboardCard {
    property var dashboardData

    signal editRequested(var item, string titleKey)
    signal createRequested(string person, real hour)

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
                    model: dashboardData.todayHighlights
                    delegate: RowLayout {
                        width: parent.width
                        spacing: 8
                        Text { text: modelData.icon; font.pixelSize: 16 }
                        Text { text: modelData.label; color: "#c7d2e3"; font.pixelSize: 13; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                        Text { text: modelData.time; color: "#8296b8"; font.pixelSize: 12; visible: !!modelData.time }
                        AttendeeBadges {
                            attendees: modelData.attendees || []
                            dashboardData: dashboardData
                            eventId: modelData.eventId
                            onToggled: (person, invited) => {
                                if (invited)
                                    calendarBridge.inviteParticipant(modelData.calendarId, modelData.eventId, modelData.etag, person)
                                else
                                    calendarBridge.uninviteParticipant(modelData.calendarId, modelData.eventId, modelData.etag, person)
                            }
                        }
                    }
                }
            }

            AgendaTimeline {
                width: parent.width
                people: dashboardData.people
                // effectiveTodaySchedule(), not the raw todaySchedule
                // property: it overlays any pending invite/uninvite tap so
                // a person's column shows/hides immediately instead of
                // waiting for the daemon's round trip and next snapshot.
                items: dashboardData.effectiveTodaySchedule()
                dashboardData: dashboardData
                startHour: 6
                endHour: 22
                rowHeight: 30
                windowHours: 6
                gutterWidth: 44
                onEventTapped: item => editRequested(item, "event")
                onEmptySlotTapped: (person, hour) => createRequested(person, hour)
                onAttendeeToggled: (item, person, invited) => {
                    if (invited)
                        calendarBridge.inviteParticipant(item.calendarId, item.eventId, item.etag, person)
                    else
                        calendarBridge.uninviteParticipant(item.calendarId, item.eventId, item.etag, person)
                }
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
                    model: dashboardData.weekendGrouped()
                    delegate: Column {
                        width: parent.width
                        spacing: 4

                        Text { text: modelData.day; color: "#9085e9"; font.pixelSize: 12; font.bold: true }

                        Repeater {
                            model: modelData.items
                            delegate: Item {
                                width: parent.width
                                height: weekendRow.implicitHeight
                                // Declared before weekendRow so its badges'
                                // own MouseAreas (added below, on top in
                                // z-order) intercept a tap before this
                                // whole-row one does — same layering
                                // AgendaTimeline uses for its chips over its
                                // background pan MouseArea.
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: editRequested(modelData, "title")
                                }
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
                                    AttendeeBadges {
                                        size: 16
                                        attendees: modelData.attendees || []
                                        dashboardData: dashboardData
                                        eventId: modelData.eventId
                                        onToggled: (person, invited) => {
                                            if (invited)
                                                calendarBridge.inviteParticipant(modelData.calendarId, modelData.eventId, modelData.etag, person)
                                            else
                                                calendarBridge.uninviteParticipant(modelData.calendarId, modelData.eventId, modelData.etag, person)
                                        }
                                    }
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
                    model: dashboardData.upcoming
                    delegate: Item {
                        width: parent.width
                        height: upcomingRow.implicitHeight
                        // Declared before upcomingRow — same reasoning as
                        // the weekend delegate above.
                        MouseArea {
                            anchors.fill: parent
                            onClicked: editRequested(modelData, "title")
                        }
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
                            AttendeeBadges {
                                size: 16
                                attendees: modelData.attendees || []
                                dashboardData: dashboardData
                                eventId: modelData.eventId
                                onToggled: (person, invited) => {
                                    if (invited)
                                        calendarBridge.inviteParticipant(modelData.calendarId, modelData.eventId, modelData.etag, person)
                                    else
                                        calendarBridge.uninviteParticipant(modelData.calendarId, modelData.eventId, modelData.etag, person)
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
