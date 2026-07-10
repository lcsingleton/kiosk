import QtQuick

// Mock data standing in for the real Home Assistant / Ecowitt / calendar
// integrations. Shapes here are what the cards in main.qml bind against —
// swap the source, keep the fields. Item rather than QtObject solely so the
// attendeeOverrides expiry Timer below has a default property to attach to
// — never added to a visual tree, so nothing else about being an Item
// applies.
Item {
    // One system at a time in practice (heating XOR cooling), so it's
    // modeled — and displayed — as a single climate system, not two.
    property var climate: ({
        mode: "heating",       // "heating" | "cooling" | "idle"
        power: true,
        current: 19.5,
        target: 21.0,
        fanSpeed: "Auto · Med",
        airMode: "Fresh"        // "Fresh" | "Recirc"
    })

    // The four columns of the day-grid — fixed order, name doubles as the
    // lookup key for todaySchedule items. Sourced live from the
    // calendar-sync daemon's snapshot (see CalendarBridge) — swapped from
    // mock literals, field shapes unchanged.
    readonly property var people: calendarBridge.people

    // Family-wide/all-day things today, not tied to one person's hourly
    // schedule: birthdays (incl. family friends), vet runs, household stuff,
    // departures for a family trip/weekend away.
    property var todayHighlights: calendarBridge.todayHighlights

    // Per-person timed items for today. start/duration are decimal hours
    // (e.g. 8.25 = 8:15) so the day-grid can position blocks directly
    // without re-parsing a time string.
    property var todaySchedule: calendarBridge.todaySchedule

    property var weekend: calendarBridge.weekend

    // Grouped by day so the widget can render one Sailfish-style day header
    // followed by that day's items, instead of repeating the date per row.
    function weekendGrouped() {
        const days = []
        for (const item of weekend) {
            let group = days.find(d => d.day === item.day)
            if (!group) {
                group = { day: item.day, date: item.date, items: [] }
                days.push(group)
            }
            group.items.push(item)
        }
        return days
    }

    property var upcoming: calendarBridge.upcoming

    // Optimistic attendee-invite state, shared by every AttendeeBadges
    // instance (today highlights, weekend/upcoming rows, agenda chips, the
    // edit popup) rather than each keeping its own copy — otherwise tapping
    // "invite" in the popup would only flip that one popup's badge, and the
    // same event's row in weekend/upcoming would sit stale until the
    // daemon's debounce window + PATCH + next snapshot poll all land,
    // several seconds later. Keyed by attendeeOverrideKey(eventId, person).
    property var attendeeOverrides: ({})

    function attendeeOverrideKey(eventId, person) {
        return JSON.stringify([eventId, person])
    }

    function setAttendeeOverride(eventId, person, invited) {
        const next = Object.assign({}, attendeeOverrides)
        // expiresAt: a safety valve for a write that silently failed — no
        // real snapshot will ever arrive to reconcile it otherwise.
        next[attendeeOverrideKey(eventId, person)] = { invited: invited, expiresAt: Date.now() + 10000 }
        attendeeOverrides = next
    }

    function attendeeOverride(eventId, person) {
        const entry = attendeeOverrides[attendeeOverrideKey(eventId, person)]
        return entry ? entry.invited : undefined
    }

    // Called whenever fresh attendee data for one event arrives (any
    // AttendeeBadges showing that event will trigger this) — drops any
    // override the real data now agrees with.
    function reconcileAttendeeOverrides(eventId, attendees) {
        if (!attendees || !attendees.length)
            return
        let changed = false
        const next = Object.assign({}, attendeeOverrides)
        for (const a of attendees) {
            const k = attendeeOverrideKey(eventId, a.name)
            if (k in next && next[k].invited === a.invited) {
                delete next[k]
                changed = true
            }
        }
        if (changed)
            attendeeOverrides = next
    }

    Timer {
        interval: 1000
        running: true
        repeat: true
        onTriggered: {
            const now = Date.now()
            let changed = false
            const next = Object.assign({}, attendeeOverrides)
            for (const k in next) {
                if (next[k].expiresAt <= now) {
                    delete next[k]
                    changed = true
                }
            }
            if (changed)
                attendeeOverrides = next
        }
    }

    // The day-grid's columns are which person a todaySchedule row belongs
    // to — a side effect the daemon computes from the real attendee list
    // (SnapshotBuilder::resolveAttendedPeople), not a flag on the row
    // itself. So showing an invite/uninvite tap's effect on the grid before
    // the real backend round-trips means synthesizing (or dropping) a row
    // client-side: clone an existing row for that event onto the newly
    // invited person's column, or filter out the uninvited person's row.
    // Reconciles away on its own once a real snapshot agrees, the same as
    // attendeeOverrides above.
    function effectiveTodaySchedule() {
        if (Object.keys(attendeeOverrides).length === 0)
            return todaySchedule

        const rowsByEvent = {}
        for (const row of todaySchedule) {
            if (!rowsByEvent[row.eventId])
                rowsByEvent[row.eventId] = []
            rowsByEvent[row.eventId].push(row)
        }

        let result = todaySchedule.slice()
        for (const key in attendeeOverrides) {
            const [eventId, person] = JSON.parse(key)
            const invited = attendeeOverrides[key].invited
            const existingRows = rowsByEvent[eventId] || []
            const personRow = existingRows.find(r => r.person === person)

            if (invited && !personRow) {
                const template = existingRows[0]
                if (template) {
                    const personInfo = people.find(p => p.name === person)
                    result.push(Object.assign({}, template, {
                        person: person,
                        color: personInfo ? personInfo.color : template.color
                    }))
                }
            } else if (!invited && personRow) {
                result = result.filter(r => !(r.eventId === eventId && r.person === person))
            }
        }
        return result
    }

    property var shopping: [
        { item: "Milk", done: false },
        { item: "Bread", done: false },
        { item: "Eggs", done: true },
        { item: "Dishwasher tablets", done: false },
        { item: "Dog food", done: true }
    ]

    // Last 24h, one point/hour, ending at "now" (2pm in this mock) so it
    // butts directly against hourlyForecast below with no gap or overlap.
    // The Ecowitt WH65LP + HA logs at 1-10 min resolution, but plotting raw
    // minute-level noise on a glance-chart this size is unreadable — rolled
    // up to hourly for display.
    property var weatherHistory: [
        { hour: "3pm",  temp: 22.9 }, { hour: "4pm", temp: 22.0 },
        { hour: "5pm",  temp: 20.4 }, { hour: "6pm", temp: 18.7 },
        { hour: "7pm",  temp: 17.2 }, { hour: "8pm", temp: 15.9 },
        { hour: "9pm",  temp: 14.9 }, { hour: "10pm", temp: 13.8 },
        { hour: "11pm", temp: 13.1 }, { hour: "12am", temp: 12.4 },
        { hour: "1am",  temp: 12.0 }, { hour: "2am", temp: 11.6 },
        { hour: "3am",  temp: 11.2 }, { hour: "4am", temp: 10.9 },
        { hour: "5am",  temp: 10.8 }, { hour: "6am", temp: 11.2 },
        { hour: "7am",  temp: 12.6 }, { hour: "8am", temp: 14.8 },
        { hour: "9am",  temp: 16.9 }, { hour: "10am", temp: 18.3 },
        { hour: "11am", temp: 20.1 }, { hour: "12pm", temp: 21.5 },
        { hour: "1pm",  temp: 22.6 }, { hour: "2pm", temp: 23.1 }
    ]

    // Remainder of today, hour by hour, continuing on from weatherHistory's
    // last point ("now") — the two are plotted as one continuous line on
    // the weather chart (see weatherSeries()), solid fading to dashed.
    property var hourlyForecast: [
        { hour: "3pm",  icon: "☀️", temp: 23 },
        { hour: "4pm",  icon: "⛅", temp: 22 },
        { hour: "5pm",  icon: "⛅", temp: 20 },
        { hour: "6pm",  icon: "🌤️", temp: 18 },
        { hour: "7pm",  icon: "🌙", temp: 16 },
        { hour: "8pm",  icon: "🌙", temp: 15 },
        { hour: "9pm",  icon: "🌙", temp: 14 },
        { hour: "10pm", icon: "🌙", temp: 13 },
        { hour: "11pm", icon: "🌙", temp: 12 }
    ]

    // Combined series for the chart: real history (solid) immediately
    // followed by the projected hours (dashed, icon-annotated) — one
    // continuous line rather than two separate widgets to glance at.
    function weatherSeries() {
        const history = weatherHistory.map(p => ({ label: p.hour, value: p.temp }))
        const forecast = hourlyForecast.map(p => ({ label: p.hour, value: p.temp, icon: p.icon, forecast: true }))
        return history.concat(forecast)
    }

    property var forecast: [
        { day: "Today", icon: "☀️", hi: "23°", lo: "11°" },
        { day: "Thu",    icon: "⛅", hi: "21°", lo: "12°" },
        { day: "Fri",    icon: "🌦️", hi: "18°", lo: "13°" },
        { day: "Sat",    icon: "🌧️", hi: "16°", lo: "12°" },
        { day: "Sun",    icon: "⛅", hi: "19°", lo: "10°" },
        { day: "Mon",    icon: "☀️", hi: "22°", lo: "9°"  },
        { day: "Tue",    icon: "☀️", hi: "24°", lo: "11°" },
        { day: "Wed",    icon: "⛅", hi: "22°", lo: "12°" }
    ]
}
