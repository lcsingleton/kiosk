import QtQuick

// Every event lives in exactly one Repeater, bound directly to the stable
// `items` prop — never a filtered copy used as a model (filtering only
// feeds the layout math, never the Repeater itself).
//
// Default rendering, for every event, is the same: dead-time-collapsed
// overflow styling. buildTimeline sorts every event that's outside the
// window into ONE chronological sequence (no separate before/after
// computation) and clusters overlapping ones into rows (so simultaneous
// events across people share a row, and later events always land on a
// lower row), stacking rows back to back — dead time with nothing
// scheduled takes ~0 space. The window's real-time span is injected as a
// single gap wherever the sequence crosses it.
//
// Only events that actually intersect the window get the "adornments":
// real time-scale for the part inside it, plus a simple compressed
// extension for whatever sticks out past either edge — a continuous
// blended box rather than a clip. Because every event is the same
// persistent delegate regardless of where it falls, and that boundary math
// is continuous, dragging the window just works — no add/remove, no fade
// tricks, one chip style.
Item {
    id: grid
    property var people: []    // [{ name, color }]
    property var items: []     // [{ person, color, event, start, duration }] — hours, decimal
    property real startHour: 6
    property real endHour: 22
    property real rowHeight: 44        // px per hour, inside the window
    property real windowHours: 6
    property real gutterWidth: 60
    property real overflowHourScale: 26 // px per hour of an event's own duration, outside the window
    property real minChipHeight: 16     // legibility floor for compressed/short events
    // Passed straight through to each chip's AttendeeBadges — needed so an
    // invite/uninvite tap here shows up immediately in every other place
    // that same event's badges appear (see AttendeeBadges.qml). The grid
    // itself never reads from this beyond that passthrough.
    property var dashboardData: null

    signal eventTapped(var item)                  // one entry from `items`, tapped to edit
    signal emptySlotTapped(string person, real hour) // tapped empty space inside the live window, to create
    // A chip's own attendee badge was tapped — grid stays free of any
    // calendarBridge dependency (same as eventTapped/emptySlotTapped),
    // caller performs the actual invite/uninvite.
    signal attendeeToggled(var item, string person, bool invited)

    readonly property real colWidth: (width - gutterWidth) / Math.max(people.length, 1)
    readonly property real hourCount: endHour - startHour
    readonly property real effectiveWindowHours: Math.min(windowHours, hourCount)

    // the only scroll state there is — everything else derives from this
    property real windowStart: Math.max(startHour, Math.min(Math.floor(nowDecimal) - 1, endHour - effectiveWindowHours))
    readonly property real windowEnd: windowStart + effectiveWindowHours

    property real nowDecimal: {
        const d = new Date()
        return d.getHours() + d.getMinutes() / 60
    }
    Timer {
        interval: 60000
        running: true
        repeat: true
        onTriggered: {
            const d = new Date()
            grid.nowDecimal = d.getHours() + d.getMinutes() / 60
        }
    }

    // ---- dead-time collapsing. Every item's span is split into three
    // lengths by plain interval math (Math.max/min, no branching) against
    // the window: beforePortion (clipped to [start, windowStart]),
    // windowPortion (its overlap with the window), afterPortion (clipped to
    // [windowEnd, end]). A pure in-window event has only a windowPortion; a
    // pure earlier/later event has only a before/afterPortion; a straddling
    // event has two, and its box is their sum — continuous as the window
    // moves, since the portions themselves shrink/grow continuously,
    // there's no separate "is it still straddling" cutover.
    //
    // The one thing that ISN'T pure per-item math: simultaneous events
    // need to land on the same row/height as each other (so they read as
    // "at the same time" side by side), which inherently depends on their
    // row-mates, not just their own span. So before/after rows are built by
    // clustering *beforePortion/afterPortion* intervals (not full spans —
    // that's what keeps a straddling event's row contribution growing
    // smoothly instead of popping in fully-formed the instant it crosses),
    // keyed by each item's own INDEX into `items` rather than matched back
    // up by object-reference equality (a plain integer key can't silently
    // miss the way a reference comparison across independently-rebuilt
    // arrays could).
    function placeFold(entries) {
        // entries: [{ idx, start, end }], already the clipped portions
        entries.sort((a, b) => a.start - b.start)
        const rows = []
        for (const e of entries) {
            const last = rows.length ? rows[rows.length - 1] : null
            if (last && e.start < last.end) {
                last.entries.push(e)
                last.end = Math.max(last.end, e.end)
            } else {
                rows.push({ entries: [e], start: e.start, end: e.end })
            }
        }
        let y = 0
        const byIndex = {}   // item index -> {y, height}, for layoutForIndex
        const rowMeta = []   // ordered rows, for locating an arbitrary time (nowY)
        for (const row of rows) {
            // Unclamped: a row's contribution to the running offset is the
            // true (possibly near-zero) clipped span. If this clamped up to
            // a legibility floor immediately, a row that's just starting to
            // appear (near-zero span) would shove every already-placed
            // sibling after it down by a full floor's worth in one step —
            // that's what read as a pop. The floor is applied later, per
            // item, only to what's actually rendered, and never feeds back
            // into anyone else's position.
            const span = row.end - row.start
            const h = span * grid.overflowHourScale
            for (const e of row.entries) {
                byIndex[e.idx] = { y: y + (e.start - row.start) * grid.overflowHourScale, height: (e.end - e.start) * grid.overflowHourScale }
            }
            rowMeta.push({ end: row.end, y: y, height: h })
            y += h
        }
        return { byIndex: byIndex, rowMeta: rowMeta, height: y }
    }
    function buildTimeline() {
        const beforeEntries = [], afterEntries = []
        for (let i = 0; i < items.length; i++) {
            const it = items[i]
            const end = it.start + it.duration
            // Same comparisons layoutForIndex uses to decide whether to
            // look an item up here (beforeEnd > it.start / afterStart <
            // end) — not the algebraically-equivalent "it.start <
            // windowStart" — so the two can't disagree by a floating-point
            // hair right at the moment an item crosses the boundary and
            // send layoutForIndex looking for an entry that was never added.
            const beforeEnd = Math.min(end, windowStart)
            if (beforeEnd > it.start) beforeEntries.push({ idx: i, start: it.start, end: beforeEnd })
            const afterStart = Math.max(it.start, windowEnd)
            if (afterStart < end) afterEntries.push({ idx: i, start: afterStart, end: end })
        }
        const before = placeFold(beforeEntries)
        const after = placeFold(afterEntries)
        const topY = before.height
        return { before: before, after: after, windowTopY: topY, totalHeight: topY + grid.effectiveWindowHours * grid.rowHeight + after.height }
    }

    readonly property var timeline: buildTimeline()
    readonly property real windowTopY: timeline.windowTopY
    readonly property real windowBottomY: windowTopY + effectiveWindowHours * rowHeight
    readonly property real totalHeight: timeline.totalHeight

    // "now" is just a point, not a span — it's unambiguously in exactly one
    // of the three regions, so it only needs to locate itself among an
    // already-built fold's rows, not build/blend anything of its own
    function foldOffsetAt(rowMeta, t) {
        let y = 0
        for (const row of rowMeta) {
            if (row.end <= t) y = row.y + row.height
            else break
        }
        return y
    }
    readonly property real nowY: {
        if (nowDecimal < windowStart) return foldOffsetAt(timeline.before.rowMeta, nowDecimal)
        if (nowDecimal >= windowEnd) return windowBottomY + foldOffsetAt(timeline.after.rowMeta, nowDecimal)
        return windowTopY + (nowDecimal - windowStart) * rowHeight
    }

    // an event's height is pure interval math against the window — three
    // overlap lengths via Math.max/min, summed. No branch decides "is this
    // event straddling" because the portions themselves are already 0 when
    // they don't apply; the box grows/shrinks continuously as the window
    // moves, right through the moment a portion reaches zero.
    //
    // top is the one place this stays piecewise: an event's start is in
    // exactly one of the three regions, so that's what anchors it — its
    // before-fold row, the real-time window, or its after-fold row.
    function layoutForIndex(idx) {
        const it = items[idx]
        const start = it.start, end = it.start + it.duration
        // hasBefore/hasAfter use the exact same comparison buildTimeline
        // used to decide whether this item got an entry in before/after —
        // not a re-derived "portion > 0", which could round differently
        const beforeEnd = Math.min(end, windowStart)
        const afterStart = Math.max(start, windowEnd)
        const hasBefore = beforeEnd > start
        const hasAfter = afterStart < end

        const beforePortion = hasBefore ? beforeEnd - start : 0
        const afterPortion = hasAfter ? end - afterStart : 0
        const windowPortion = Math.max(0, Math.min(end, windowEnd) - Math.max(start, windowStart))
        const height = beforePortion * overflowHourScale + windowPortion * rowHeight + afterPortion * overflowHourScale

        let top
        if (hasBefore) {
            top = timeline.before.byIndex[idx].y
        } else if (windowPortion > 0) {
            top = windowTopY + (Math.max(start, windowStart) - windowStart) * rowHeight
        } else {
            top = windowBottomY + timeline.after.byIndex[idx].y
        }
        return { y: top, height: Math.max(height, minChipHeight), compressed: hasBefore || hasAfter }
    }

    implicitHeight: header.height + totalHeight

    function formatHour(decimal) {
        const h = Math.floor(decimal)
        const m = Math.round((decimal - h) * 60)
        return Qt.formatTime(new Date(2000, 0, 1, h, m), "h:mm AP")
    }

    Row {
        id: header
        x: grid.gutterWidth
        width: grid.width - grid.gutterWidth
        height: 26
        Repeater {
            model: grid.people
            delegate: Row {
                width: grid.colWidth
                spacing: 5
                Rectangle {
                    width: 17; height: 17; radius: 9
                    color: modelData.color
                    anchors.verticalCenter: parent.verticalCenter
                    Text { anchors.centerIn: parent; text: modelData.name.charAt(0); font.pixelSize: 10; font.bold: true; color: "#0a1220" }
                }
                Text { text: modelData.name; color: Theme.textTertiary; font.pixelSize: 11; font.bold: true; anchors.verticalCenter: parent.verticalCenter }
            }
        }
    }

    Item {
        id: body
        anchors.top: header.bottom
        width: grid.width
        height: grid.totalHeight
        // no clip: totalHeight tracks the same continuous math as every
        // item's own y/height, but items ease toward their target over 80ms
        // while this container's height applies instantly — clipping to it
        // would chop an easing item off right as it's mid-transition.

        // drag anywhere to move the conceptual window — deliberately no
        // momentum/inertia, kept simple since it's just a comparison prototype.
        // Also doubles as tap-to-create on empty space: onClicked always
        // fires on a clean press/release regardless of the manual panning
        // above, so a real pan is told apart from a tap by accumulated
        // drag distance, not by suppressing the panning itself.
        MouseArea {
            anchors.fill: parent
            property real pressMouseY: 0
            property real pressWindowStart: 0
            property real totalDrag: 0
            onPressed: mouse => { pressMouseY = mouse.y; pressWindowStart = grid.windowStart; totalDrag = 0 }
            onPositionChanged: mouse => {
                if (pressed) {
                    const dy = mouse.y - pressMouseY
                    totalDrag = Math.max(totalDrag, Math.abs(dy))
                    grid.windowStart = Math.max(grid.startHour, Math.min(pressWindowStart - dy / grid.rowHeight, grid.endHour - grid.effectiveWindowHours))
                }
            }
            onClicked: mouse => {
                if (totalDrag > 8)
                    return // was a pan, not a tap
                // Only the live time-scaled window has a clean position ->
                // hour mapping; the before/after folds are dead-time-
                // collapsed and don't correspond to a real time at all.
                if (mouse.y < grid.windowTopY || mouse.y > grid.windowBottomY)
                    return
                const colIndex = Math.floor((mouse.x - grid.gutterWidth) / grid.colWidth)
                if (colIndex < 0 || colIndex >= grid.people.length)
                    return
                const hour = grid.windowStart + (mouse.y - grid.windowTopY) / grid.rowHeight
                grid.emptySlotTapped(grid.people[colIndex].name, hour)
            }
        }

        // hour ticks — only inside the window, where time is actually at a
        // clock scale; outside it, position is driven by events, not hours
        Repeater {
            model: Math.ceil(grid.effectiveWindowHours) + 2
            delegate: Item {
                property int hour: Math.floor(grid.windowStart) + index
                visible: hour >= grid.windowStart && hour <= grid.windowEnd
                x: 0
                y: grid.windowTopY + (hour - grid.windowStart) * grid.rowHeight - 8
                width: body.width
                height: 16
                Text {
                    text: Qt.formatTime(new Date(2000, 0, 1, hour, 0), "h AP")
                    color: Theme.textMuted
                    font.pixelSize: 11
                    width: grid.gutterWidth - 10
                    horizontalAlignment: Text.AlignRight
                    anchors.verticalCenter: parent.verticalCenter
                }
                Rectangle { x: grid.gutterWidth; width: parent.width - grid.gutterWidth; height: 1; color: Theme.divider; anchors.verticalCenter: parent.verticalCenter }
            }
        }

        // column separators — one repeater, full height, no duplication
        // between "windowed" and "folded" regions since it's all one space
        Repeater {
            model: Math.max(grid.people.length - 1, 0)
            delegate: Rectangle {
                x: grid.gutterWidth + (index + 1) * grid.colWidth
                width: 1
                height: body.height
                color: Theme.divider
            }
        }

        // every event, one style, one repeater over the untouched `items`
        // model — so scrolling the window reflows each affected chip's
        // y/height instead of destroying and recreating anything
        Repeater {
            model: grid.items
            delegate: Rectangle {
                property int colIndex: grid.people.findIndex(p => p.name === modelData.person)
                property var layout: grid.layoutForIndex(index)
                // modelData.color arrives as a "#rrggbb" string from
                // CalendarBridge's JSON, not a QML color value — assigning
                // it to a `color`-typed property is what performs the
                // string-to-color coercion; .r/.g/.b below need that,
                // they don't exist on the raw string.
                property color chipColor: modelData.color
                visible: colIndex >= 0
                x: grid.gutterWidth + colIndex * grid.colWidth + 3
                width: grid.colWidth - 6
                y: layout.y
                height: layout.height
                // the underlying math is already continuous (no discrete
                // reclassification to animate away) — this just smooths
                // over the gap between mouse-move ticks during a drag, so
                // it has to stay short or it reads as lag instead of polish
                Behavior on y { NumberAnimation { duration: 80; easing.type: Easing.OutQuad } }
                Behavior on height { NumberAnimation { duration: 80; easing.type: Easing.OutQuad } }
                radius: 6
                border.width: 1
                border.color: Qt.rgba(chipColor.r, chipColor.g, chipColor.b, 0.5)
                gradient: Gradient {
                    GradientStop { position: 0.0; color: Qt.rgba(chipColor.r, chipColor.g, chipColor.b, 0.34) }
                    GradientStop { position: 1.0; color: Qt.rgba(chipColor.r, chipColor.g, chipColor.b, 0.14) }
                }
                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 1
                    height: Math.min(parent.height * 0.45, 8)
                    radius: parent.radius
                    color: "#ffffff"
                    opacity: 0.10
                }
                Rectangle {
                    width: 3
                    radius: 2
                    color: modelData.color
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.margins: 2
                }
                // Sits above the background pan MouseArea in z-order (this
                // Repeater is declared after it), so a tap landing on a
                // chip reaches this one first and never falls through to
                // emptySlotTapped underneath.
                MouseArea {
                    anchors.fill: parent
                    onClicked: grid.eventTapped(modelData)
                }
                Text {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 4
                    text: layout.compressed ? (grid.formatHour(modelData.start) + " " + modelData.event) : modelData.event
                    color: Theme.textPrimary
                    font.pixelSize: layout.compressed ? 9 : 10
                    font.bold: true
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                }
                // Only for a chip roomy enough to take a row of badges
                // without fighting the centered title text above — a
                // compressed (folded dead-time) or short chip skips them
                // entirely rather than cramming them in.
                AttendeeBadges {
                    visible: !layout.compressed && parent.height >= 40 && (modelData.attendees || []).length > 0
                    size: 12
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: 3
                    attendees: modelData.attendees || []
                    dashboardData: grid.dashboardData
                    eventId: modelData.eventId
                    onToggled: (person, invited) => grid.attendeeToggled(modelData, person, invited)
                }
            }
        }

        // "now" — a single persistent line whose y is just another read of
        // the same layout math, so it always slides rather than jumps
        Rectangle {
            visible: grid.nowDecimal >= grid.startHour && grid.nowDecimal <= grid.endHour
            x: grid.gutterWidth
            y: grid.nowY
            Behavior on y { NumberAnimation { duration: 80; easing.type: Easing.OutQuad } }
            width: body.width - grid.gutterWidth
            height: 2
            color: Theme.errorBorder
            Rectangle {
                width: 10; height: 10; radius: 5
                color: "#00000055"
                anchors.centerIn: pin
                anchors.verticalCenterOffset: 1
            }
            Rectangle {
                id: pin
                width: 7; height: 7; radius: 3.5
                color: Theme.errorIcon
                border.width: 1
                border.color: "#ffb3b5"
                anchors.verticalCenter: parent.verticalCenter
                x: -3.5
            }
        }

        // thin accent marking the window's edges — where the axis changes speed
        Rectangle { x: grid.gutterWidth; y: grid.windowTopY; width: parent.width - grid.gutterWidth; height: 1; color: Theme.accentBlue; opacity: 0.5 }
        Rectangle { x: grid.gutterWidth; y: grid.windowBottomY; width: parent.width - grid.gutterWidth; height: 1; color: Theme.accentBlue; opacity: 0.5 }
    }
}
