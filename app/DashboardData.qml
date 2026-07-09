import QtQuick

// Mock data standing in for the real Home Assistant / Ecowitt / calendar
// integrations. Shapes here are what the cards in main.qml bind against —
// swap the source, keep the fields.
QtObject {
    // Fixed per-person color order — never reassigned when the family list
    // changes, so identity stays consistent across cards (accessibility: a
    // legend/color should never be reused for a different person over time).
    readonly property color colorMum: "#3987e5"
    readonly property color colorDad: "#199e70"
    readonly property color colorRobin: "#c98500"
    readonly property color colorSasha: "#008300"
    readonly property color colorOther: "#9085e9"

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
    // lookup key for todaySchedule items.
    readonly property var people: [
        { name: "Mum",    color: colorMum },
        { name: "Dad",    color: colorDad },
        { name: "Robin",  color: colorRobin },
        { name: "Sasha",  color: colorSasha }
    ]

    // Family-wide/all-day things today, not tied to one person's hourly
    // schedule: birthdays (incl. family friends), vet runs, household stuff,
    // departures for a family trip/weekend away.
    property var todayHighlights: [
        { icon: "🎂", label: "Grandma's birthday",                     time: "" },
        { icon: "🐾", label: "Vet appt — Rex",                         time: "2:00 PM" },
        { icon: "🧹", label: "Cleaner",                                time: "1:00 PM" },
        { icon: "🏖️", label: "Depart for the lake house (long weekend)", time: "5:00 PM" }
    ]

    // Per-person timed items for today. start/duration are decimal hours
    // (e.g. 8.25 = 8:15) so the day-grid can position blocks directly
    // without re-parsing a time string.
    property var todaySchedule: [
        { person: "Mum",    color: colorMum,    event: "School drop-off",  start: 8.25,  duration: 0.25 },
        { person: "Dad",    color: colorDad,    event: "Standup",          start: 9.0,   duration: 0.5 },
        { person: "Mum",    color: colorMum,    event: "Dentist checkup",  start: 11.0,  duration: 0.5 },
        { person: "Robin",  color: colorRobin,  event: "School pickup",    start: 15.25, duration: 0.25 },
        { person: "Sasha",  color: colorSasha,  event: "School pickup",    start: 15.25, duration: 0.25 },
        { person: "Robin",  color: colorRobin,  event: "Swimming lessons", start: 16.0,  duration: 1.0 },
        { person: "Sasha",  color: colorSasha,  event: "Piano practice",   start: 16.5,  duration: 0.5 },
        { person: "Dad",    color: colorDad,    event: "Gym",              start: 17.5,  duration: 1.0 },
        { person: "Mum",    color: colorMum,    event: "Yoga class",       start: 18.0,  duration: 1.0 }
    ]

    property var weekend: [
        { day: "Saturday", date: "Jul 11", title: "Robin's swimming carnival",      time: "9:00 AM",  accent: colorRobin },
        { day: "Saturday", date: "Jul 11", title: "Birthday party — Sasha invited", time: "2:00 PM",  accent: colorSasha },
        { day: "Sunday",   date: "Jul 12", title: "Family lunch at Grandma's",      time: "12:30 PM", accent: colorOther }
    ]

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

    property var upcoming: [
        { month: "JUL", day: "18", title: "Dentist — Robin & Sasha",           time: "3:30 PM", accent: colorOther },
        { month: "JUL", day: "22", title: "Parent-teacher interviews",          time: "4:00 PM", accent: colorOther },
        { month: "AUG", day: "02", title: "Mum's work conference (interstate)", time: "All day", accent: colorMum },
        { month: "AUG", day: "05", title: "Council rates due",                  time: "",        accent: colorOther }
    ]

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
