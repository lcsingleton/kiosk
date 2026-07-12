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

    // Last 24h, one point/hour, ending at "now" so it butts directly
    // against hourlyForecast below with no gap or overlap. The Ecowitt
    // WH65LP is logged via Telegraf into a local InfluxDB at ~15s
    // resolution, but plotting raw noise on a glance-chart this size is
    // unreadable — rolled up to hourly by the weather-sync daemon (see
    // InfluxClient/SnapshotBuilder) before it ever reaches here. Sourced
    // live from the daemon's snapshot (see WeatherBridge), same
    // "swap the source, keep the fields" treatment as hourlyForecast below.
    readonly property var weatherHistory: weatherBridge.weatherHistory

    // Remainder of today, hour by hour, continuing on from weatherHistory's
    // last point ("now") — the two are plotted as one continuous line on
    // the weather chart (see weatherSeries()), solid fading to dashed.
    // Sourced live from the weather-sync daemon's snapshot (see
    // WeatherBridge) — swapped from mock literals, field shapes unchanged.
    readonly property var hourlyForecast: weatherBridge.hourlyForecast

    // Combined series for the chart: real history (solid) immediately
    // followed by the projected hours (dashed, icon-annotated) — one
    // continuous line rather than two separate widgets to glance at.
    function weatherSeries() {
        const history = weatherHistory.map(p => ({ label: p.hour, value: p.temp }))
        const forecast = hourlyForecast.map(p => ({ label: p.hour, value: p.temp, icon: p.icon, forecast: true }))
        return history.concat(forecast)
    }

    readonly property var forecast: weatherBridge.forecast

    // Current-conditions stats shown alongside the chart — pre-formatted
    // strings (e.g. "62%", "14 km/h") straight from the Ecowitt station's
    // own latest reading (via Telegraf/InfluxDB, see InfluxClient), same
    // treatment as forecast/hourlyForecast above.
    readonly property var currentConditions: weatherBridge.observations
    readonly property string humidity: currentConditions.humidity || ""
    readonly property string windSpeed: currentConditions.windSpeed || ""
    readonly property string rainToday: currentConditions.rainToday || ""
}
