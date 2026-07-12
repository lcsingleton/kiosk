import QtQuick

// One badge per configured person, driven off an event's `attendees` field
// ([{ name, color, invited }] — see SnapshotBuilder::attendeeStatus).
// Filled = currently an attendee, outline-only = not. Tapping any badge
// toggles that person; callers own the actual invite/uninvite call, this
// component only reports the tap.
//
// Optimistic state lives on `dashboardData` (attendeeOverrides), not here —
// this same event can show badges in several places at once (a today
// highlight, its weekend/upcoming row, an agenda chip, the edit popup), and
// they all need to flip together the instant any one of them is tapped,
// not just the one instance that got the tap.
Row {
    id: badges
    property var attendees: []
    property real size: 20    // diameter — the agenda's compressed chips use a smaller size than the highlights/popup rows
    property var dashboardData: null
    property string eventId: ""
    signal toggled(string person, bool invited)
    spacing: Math.max(2, size / 5)

    function displayAttendees() {
        return badges.attendees.map(a => {
            const override = badges.dashboardData ? badges.dashboardData.attendeeOverride(badges.eventId, a.name) : undefined
            return override === undefined ? a : Object.assign({}, a, { invited: override })
        })
    }

    onAttendeesChanged: {
        if (dashboardData)
            dashboardData.reconcileAttendeeOverrides(eventId, attendees)
    }

    Repeater {
        model: badges.displayAttendees()
        delegate: Rectangle {
            width: badges.size; height: badges.size; radius: badges.size / 2
            color: modelData.invited ? modelData.color : "transparent"
            border.width: Math.max(1, badges.size / 13)
            border.color: modelData.color
            opacity: modelData.invited ? 1.0 : 0.55

            Text {
                anchors.centerIn: parent
                text: modelData.name.charAt(0)
                font.pixelSize: Math.max(7, badges.size / 2)
                font.bold: true
                color: modelData.invited ? "#0a1220" : modelData.color
            }

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    const person = modelData.name
                    const newInvited = !modelData.invited
                    const dashboardData = badges.dashboardData
                    const eventId = badges.eventId
                    // toggled() first: setAttendeeOverride() below mutates
                    // dashboardData.attendeeOverrides, which displayAttendees()
                    // (this delegate's Repeater model, line 36) reads — so it
                    // synchronously tears down and recreates this very
                    // delegate. Nothing after that call may reference `badges`,
                    // so the needed values are captured into locals above first.
                    badges.toggled(person, newInvited)
                    if (dashboardData)
                        dashboardData.setAttendeeOverride(eventId, person, newInvited)
                }
            }
        }
    }
}
