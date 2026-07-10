import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// One popup for every touch interaction the calendar supports — schedule,
// rename, reschedule, change location, invite/uninvite, cancel — rather
// than a single form with one "Save": each action button fires its own command
// immediately, using whatever etag is currently bound (kept fresh by the
// caller re-assigning `event` whenever the snapshot updates while this is
// open). Batching several field edits behind one Save would let a stale
// etag from an earlier field reject a later one, since every successful
// write changes the event's etag.
Popup {
    id: popup
    modal: true
    focus: true
    x: (parent ? (parent.width - width) / 2 : 0)
    y: (parent ? (parent.height - height) / 2 : 0)
    width: 460
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    padding: 20

    // { calendarId, eventId, etag, title, startIso, endIso, attendees } for
    // edit mode; isNew + newCalendarId + newStartHour drive create mode
    // instead (attendees is meaningless there — nothing to invite yet).
    property var event: null
    property bool isNew: false
    property string newCalendarId: ""
    property real newStartHour: 9
    // Passed through to AttendeeBadges so a tap here shows up immediately
    // wherever else this event's badges are showing (weekend/upcoming row,
    // agenda chip) — see AttendeeBadges.qml.
    property var dashboardData: null

    background: Rectangle {
        color: "#101a2e"
        radius: 14
        border.color: "#2a3c5c"
        border.width: 1
    }

    function pad2(n) { return (n < 10 ? "0" : "") + n }

    function isoAtTime(referenceDate, hhmm) {
        const parts = hhmm.split(":")
        const h = parseInt(parts[0], 10) || 0
        const m = parts.length > 1 ? (parseInt(parts[1], 10) || 0) : 0
        return new Date(referenceDate.getFullYear(), referenceDate.getMonth(), referenceDate.getDate(), h, m, 0)
    }

    onOpened: {
        titleField.text = event ? (event.title || "") : ""
        locationField.text = ""
        if (isNew) {
            const h = Math.floor(newStartHour)
            const m = Math.round((newStartHour - h) * 60)
            startField.text = pad2(h) + ":" + pad2(m)
            durationField.text = "60"
        } else if (event && event.startIso) {
            const s = new Date(event.startIso)
            const e = new Date(event.endIso)
            startField.text = pad2(s.getHours()) + ":" + pad2(s.getMinutes())
            durationField.text = String(Math.max(5, Math.round((e - s) / 60000)))
        }
    }

    contentItem: ColumnLayout {
        spacing: 14
        width: popup.width - popup.padding * 2

        Text {
            text: popup.isNew ? "New event" : "Edit event"
            color: "#eef2f9"
            font.pixelSize: 16
            font.bold: true
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            TextField {
                id: titleField
                Layout.fillWidth: true
                placeholderText: "Title"
            }
            Button {
                text: "Rename"
                visible: !popup.isNew
                enabled: titleField.text.length > 0
                onClicked: calendarBridge.renameEvent(popup.event.calendarId, popup.event.eventId,
                                                       popup.event.etag, titleField.text)
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            TextField {
                id: locationField
                Layout.fillWidth: true
                placeholderText: "Location"
            }
            Button {
                text: "Update"
                visible: !popup.isNew
                enabled: locationField.text.length > 0
                onClicked: calendarBridge.changeEventLocation(popup.event.calendarId, popup.event.eventId,
                                                                popup.event.etag, locationField.text)
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Text { text: "Start"; color: "#8296b8"; font.pixelSize: 13 }
            TextField {
                id: startField
                Layout.preferredWidth: 90
                placeholderText: "HH:mm"
            }
            Text { text: "Duration (min)"; color: "#8296b8"; font.pixelSize: 13 }
            TextField {
                id: durationField
                Layout.preferredWidth: 70
                placeholderText: "60"
                validator: IntValidator { bottom: 5; top: 1440 }
            }
            Button {
                text: popup.isNew ? "Create" : "Move"
                enabled: startField.text.length > 0 && durationField.text.length > 0
                          && (!popup.isNew || titleField.text.length > 0)
                onClicked: {
                    const refDate = popup.isNew ? new Date() : new Date(popup.event.startIso)
                    const newStart = popup.isoAtTime(refDate, startField.text)
                    const newEnd = new Date(newStart.getTime() + parseInt(durationField.text, 10) * 60000)
                    if (popup.isNew) {
                        calendarBridge.scheduleEvent(popup.newCalendarId, titleField.text,
                                                      newStart.toISOString(), newEnd.toISOString())
                        popup.close()
                    } else {
                        calendarBridge.rescheduleEvent(popup.event.calendarId, popup.event.eventId,
                                                        popup.event.etag, newStart.toISOString(), newEnd.toISOString())
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            visible: !popup.isNew
            Text { text: "Attendees"; color: "#8296b8"; font.pixelSize: 13 }
            AttendeeBadges {
                attendees: (popup.event && popup.event.attendees) || []
                dashboardData: popup.dashboardData
                eventId: popup.event ? popup.event.eventId : ""
                onToggled: (person, invited) => {
                    if (invited)
                        calendarBridge.inviteParticipant(popup.event.calendarId, popup.event.eventId, popup.event.etag, person)
                    else
                        calendarBridge.uninviteParticipant(popup.event.calendarId, popup.event.eventId, popup.event.etag, person)
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Button {
                text: "Cancel event"
                visible: !popup.isNew
                onClicked: {
                    calendarBridge.cancelEvent(popup.event.calendarId, popup.event.eventId, popup.event.etag)
                    popup.close()
                }
            }
            Item { Layout.fillWidth: true }
            Button {
                text: "Close"
                onClicked: popup.close()
            }
        }
    }
}
