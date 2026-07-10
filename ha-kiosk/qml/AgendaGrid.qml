import QtQuick

// Hourly grid: one column per participant, one row per hour. Modeled on
// Sailfish Calendar's day view — thin hour rules and a live "now" line, with
// event blocks positioned/sized directly from start+duration rather than
// laid out by a list delegate. The Flickable's height caps the visible span
// to windowHours (the "6hr window"); the rest of the day is a scroll away.
//
// EARLIER (above the table) and LATER (below) are a direct visual extension
// of it — same gutter/column grammar, same glossy chips — always present,
// holding whatever is currently scrolled out of the live window. Items
// migrate between the table and these sections live as the window scrolls,
// classified by full overlap with the window so nothing is ever shown in
// both places at once. Within a section, events are grouped into shared
// chronological rows — overlapping/simultaneous events across different
// people share a row (rendered side by side in their own columns), so
// ordering and same-time alignment survive even though the dead time
// between rows is collapsed away. The "now" line follows along: if the live
// window doesn't currently cover the present moment, the red line relocates
// into whichever section does.
Item {
    id: grid
    property var people: []    // [{ name, color }]
    property var items: []     // [{ person, color, event, start, duration }] — hours, decimal
    property real startHour: 6
    property real endHour: 22
    property real rowHeight: 44
    property int windowHours: 6
    property real gutterWidth: 60
    property real overflowHourScale: 26   // px per hour of duration, compressed (no dead-time gaps)
    property real overflowMinHeight: 20
    property real overflowInsetMinHeight: 30   // sections hold this much height even when empty, so layout doesn't jump

    readonly property real colWidth: (width - gutterWidth) / Math.max(people.length, 1)
    readonly property real hourCount: endHour - startHour
    readonly property real effectiveWindowHours: Math.min(windowHours, hourCount)

    // Live scroll position, expressed as hours — recomputes continuously as
    // flick.contentY moves, so the overflow lists track the visible window
    // immediately, with no wait for scrolling to settle.
    readonly property real windowStart: startHour + flick.contentY / rowHeight
    readonly property real windowEnd: windowStart + effectiveWindowHours

    // earlierItems/laterItems only take on a *new* array reference when
    // membership actually changes (an item crosses the window edge) — not
    // on every pixel of a drag, since windowStart changes continuously but
    // the set of items on either side of it usually doesn't. That keeps
    // downstream repeaters from rebuilding (and re-fading-in) every frame
    // while still reacting the instant a real crossing happens.
    property var earlierItems: []
    property var laterItems: []

    function sameItems(a, b) {
        if (a.length !== b.length) return false
        for (let i = 0; i < a.length; i++) if (a[i] !== b[i]) return false
        return true
    }
    function recomputeOverflowMembership() {
        // Mutually exclusive with what the table shows: an item that started
        // before the window but is still running when it begins (or one that
        // starts inside the window and runs past its end) overlaps the
        // window, so it stays a table-only item — classifying it by start
        // time alone would also drop it into the overflow, showing it twice.
        const nextEarlier = items.filter(it => it.start + it.duration <= windowStart).slice().sort((a, b) => a.start - b.start)
        const nextLater = items.filter(it => it.start >= windowEnd).slice().sort((a, b) => a.start - b.start)
        if (!sameItems(earlierItems, nextEarlier)) earlierItems = nextEarlier
        if (!sameItems(laterItems, nextLater)) laterItems = nextLater
    }
    onWindowStartChanged: recomputeOverflowMembership()
    onItemsChanged: recomputeOverflowMembership()
    Component.onCompleted: recomputeOverflowMembership()

    // Group chronologically into rows: items whose time ranges overlap share
    // a row (rendered side by side, one per person's column), so ordering —
    // and same-time alignment across people — survives even though the dead
    // time between rows is collapsed away.
    function buildOverflowRows(sortedList) {
        const rows = []
        for (const it of sortedList) {
            const end = it.start + it.duration
            const last = rows.length ? rows[rows.length - 1] : null
            if (last && it.start < last.end) {
                last.items.push(it)
                last.end = Math.max(last.end, end)
            } else {
                rows.push({ items: [it], start: it.start, end: end })
            }
        }
        return rows
    }
    function rowSpan(row) {
        return Math.max(grid.overflowMinHeight, Math.max(...row.items.map(it => it.duration * grid.overflowHourScale)))
    }
    function placeRows(rows) {
        let y = 0
        const placed = []
        for (const row of rows) {
            const h = rowSpan(row)
            for (const it of row.items) placed.push({ item: it, y: y, height: h })
            y += h
        }
        return placed
    }
    // where "now" falls among a section's compressed rows — used to plant
    // the now-line there when the live grid window doesn't cover "now"
    function nowMarkerOffset(rows) {
        let y = 0
        for (const row of rows) {
            if (row.end <= grid.nowDecimal) y += rowSpan(row)
            else break
        }
        return y
    }

    readonly property var earlierRows: buildOverflowRows(earlierItems)
    readonly property var laterRows: buildOverflowRows(laterItems)
    readonly property var earlierPlaced: placeRows(earlierRows)
    readonly property var laterPlaced: placeRows(laterRows)
    readonly property real earlierMaxHeight: earlierRows.reduce((sum, row) => sum + rowSpan(row), 0)
    readonly property real laterMaxHeight: laterRows.reduce((sum, row) => sum + rowSpan(row), 0)

    readonly property bool showEarlierNow: nowDecimal < windowStart
    readonly property bool showLaterNow: nowDecimal >= windowEnd
    readonly property real earlierNowY: nowMarkerOffset(earlierRows)
    readonly property real laterNowY: nowMarkerOffset(laterRows)

    property real nowDecimal: {
        const d = new Date()
        return d.getHours() + d.getMinutes() / 60
    }

    // Where the window starts out, before the user scrolls: centered a touch
    // behind "now" so there's a little past context as well as what's ahead.
    property real initialScrollHour: Math.max(startHour, Math.min(Math.floor(nowDecimal) - 1, endHour - windowHours))

    function formatHour(decimal) {
        const h = Math.floor(decimal)
        const m = Math.round((decimal - h) * 60)
        return Qt.formatTime(new Date(2000, 0, 1, h, m), "h:mm AP")
    }

    implicitHeight: header.height + earlierSection.height + flick.height + laterSection.height

    Timer {
        interval: 60000
        running: true
        repeat: true
        onTriggered: {
            const d = new Date()
            grid.nowDecimal = d.getHours() + d.getMinutes() / 60
        }
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
                Text {
                    text: modelData.name
                    color: "#c7d2e3"
                    font.pixelSize: 11
                    font.bold: true
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }
    }

    // overflow: anything currently scrolled out of the window, always shown
    // above (earlier) / below (later) the table — styled as a direct
    // continuation of it (same gutter label + rule-line grammar as an hour
    // row, same glossy chip as a live event, no box/border of its own) so
    // it reads as more of the same columns rather than a separate control.
    // Rows: no gaps, each sized to its own duration rather than real
    // elapsed time, so dead time collapses.
    Column {
        id: earlierSection
        anchors.top: header.bottom
        width: grid.width
        spacing: 0

        Item {
            width: grid.width
            height: 20
            Text {
                text: "EARLIER"
                color: "#5c6f8f"
                font.pixelSize: 10
                font.bold: true
                font.letterSpacing: 1
                width: grid.gutterWidth - 10
                horizontalAlignment: Text.AlignRight
                anchors.verticalCenter: parent.verticalCenter
            }
            Rectangle { x: grid.gutterWidth; width: parent.width - grid.gutterWidth; height: 1; color: "#1c2c48"; anchors.verticalCenter: parent.verticalCenter }
        }
        OverflowColumns {
            width: grid.width
            height: Math.max(grid.earlierMaxHeight, grid.showEarlierNow ? 4 : 0, grid.overflowInsetMinHeight)
            Behavior on height { NumberAnimation { duration: 220; easing.type: Easing.OutQuad } }
            placed: grid.earlierPlaced
            nowY: grid.earlierNowY
            showNow: grid.showEarlierNow
        }
    }

    Flickable {
        id: flick
        anchors.top: earlierSection.bottom
        width: grid.width
        height: grid.effectiveWindowHours * grid.rowHeight
        contentWidth: width
        contentHeight: grid.hourCount * grid.rowHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        Component.onCompleted: contentY = (grid.initialScrollHour - grid.startHour) * grid.rowHeight

        Item {
            width: flick.width
            height: flick.contentHeight

            // hour rules + labels
            Repeater {
                model: grid.hourCount
                delegate: Item {
                    y: index * grid.rowHeight
                    width: flick.width
                    height: grid.rowHeight
                    Text {
                        text: Qt.formatTime(new Date(2000, 0, 1, grid.startHour + index, 0), "h AP")
                        color: "#5c6f8f"
                        font.pixelSize: 11
                        width: grid.gutterWidth - 10
                        horizontalAlignment: Text.AlignRight
                        y: 2
                    }
                    Rectangle { x: grid.gutterWidth; width: parent.width - grid.gutterWidth; height: 1; color: "#1c2c48" }
                }
            }

            // column separators
            Repeater {
                model: Math.max(grid.people.length - 1, 0)
                delegate: Rectangle {
                    x: grid.gutterWidth + (index + 1) * grid.colWidth
                    width: 1
                    height: parent.height
                    color: "#1c2c48"
                }
            }

            // events — glossy chip: gradient fill, tinted border, top highlight
            Repeater {
                model: grid.items
                delegate: Rectangle {
                    property int colIndex: grid.people.findIndex(p => p.name === modelData.person)
                    visible: colIndex >= 0
                    x: grid.gutterWidth + colIndex * grid.colWidth + 3
                    y: (modelData.start - grid.startHour) * grid.rowHeight + 1
                    width: grid.colWidth - 6
                    height: Math.max(modelData.duration * grid.rowHeight - 2, 18)
                    radius: 6
                    border.width: 1
                    border.color: Qt.rgba(modelData.color.r, modelData.color.g, modelData.color.b, 0.5)
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: Qt.rgba(modelData.color.r, modelData.color.g, modelData.color.b, 0.34) }
                        GradientStop { position: 1.0; color: Qt.rgba(modelData.color.r, modelData.color.g, modelData.color.b, 0.14) }
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
                    Text {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 4
                        text: modelData.event
                        color: "#eef2f9"
                        font.pixelSize: 10
                        font.bold: true
                        elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }

            // live "now" line, with a small pin-head and drop shadow
            Rectangle {
                visible: grid.nowDecimal >= grid.startHour && grid.nowDecimal <= grid.endHour
                x: grid.gutterWidth
                y: (grid.nowDecimal - grid.startHour) * grid.rowHeight
                width: parent.width - grid.gutterWidth
                height: 2
                color: "#e5484d"
                Rectangle {
                    width: 10; height: 10; radius: 5
                    color: "#00000055"
                    anchors.centerIn: pin
                    anchors.verticalCenterOffset: 1
                }
                Rectangle {
                    id: pin
                    width: 7; height: 7; radius: 3.5
                    color: "#ff6b6f"
                    border.width: 1
                    border.color: "#ffb3b5"
                    anchors.verticalCenter: parent.verticalCenter
                    x: -3.5
                }
            }
        }
    }

    // inset shadows hint that there's more to scroll — fixed to the
    // viewport (not the content), fading in only when relevant. Capped
    // well under full opacity: an event sitting in this strip hasn't
    // actually crossed into earlier/later yet, so it must stay legible
    // (just dimmed), not get erased before its real reclassification.
    Rectangle {
        anchors.top: flick.top
        anchors.left: flick.left
        anchors.right: flick.right
        height: 12
        visible: flick.contentY > 1
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#8c0b1220" }
            GradientStop { position: 1.0; color: "#000b1220" }
        }
    }
    Rectangle {
        anchors.bottom: flick.bottom
        anchors.left: flick.left
        anchors.right: flick.right
        height: 12
        visible: flick.contentY < flick.contentHeight - flick.height - 1
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#000b1220" }
            GradientStop { position: 1.0; color: "#8c0b1220" }
        }
    }

    Column {
        id: laterSection
        anchors.top: flick.bottom
        width: grid.width
        spacing: 0

        Item {
            width: grid.width
            height: 20
            Text {
                text: "LATER"
                color: "#5c6f8f"
                font.pixelSize: 10
                font.bold: true
                font.letterSpacing: 1
                width: grid.gutterWidth - 10
                horizontalAlignment: Text.AlignRight
                anchors.verticalCenter: parent.verticalCenter
            }
            Rectangle { x: grid.gutterWidth; width: parent.width - grid.gutterWidth; height: 1; color: "#1c2c48"; anchors.verticalCenter: parent.verticalCenter }
        }
        OverflowColumns {
            width: grid.width
            height: Math.max(grid.laterMaxHeight, grid.showLaterNow ? 4 : 0, grid.overflowInsetMinHeight)
            Behavior on height { NumberAnimation { duration: 220; easing.type: Easing.OutQuad } }
            placed: grid.laterPlaced
            nowY: grid.laterNowY
            showNow: grid.showLaterNow
        }
    }

    // one column per person, aligned under the table's own columns. Items
    // are pre-placed (grid.placeRows) onto shared chronological rows, so
    // simultaneous events across people land on the same row, and later
    // events always sit lower — even across different people's columns.
    component OverflowColumns: Item {
        property var placed: []
        property real nowY: 0
        property bool showNow: false
        // clipped to its own (Behavior-animated) height, so a chip that's
        // just entered this section is revealed by the boundary sliding
        // over/off it, rather than by fading in in place
        clip: true

        Repeater {
            model: placed
            delegate: OverflowChip {
                property int colIndex: grid.people.findIndex(p => p.name === modelData.item.person)
                visible: colIndex >= 0
                x: grid.gutterWidth + colIndex * grid.colWidth + 3
                y: modelData.y
                width: grid.colWidth - 6
                height: modelData.height
            }
        }
        Repeater {
            model: Math.max(grid.people.length - 1, 0)
            delegate: Rectangle {
                x: grid.gutterWidth + (index + 1) * grid.colWidth
                width: 1
                height: parent.height
                color: "#1c2c48"
            }
        }
        // "now" migrated here because the live window doesn't currently
        // cover it — slides to its new spot rather than fading
        Rectangle {
            visible: showNow
            x: grid.gutterWidth
            y: nowY - 1
            Behavior on y { NumberAnimation { duration: 220; easing.type: Easing.OutQuad } }
            width: parent.width - grid.gutterWidth
            height: 2
            color: "#e5484d"
            Rectangle {
                width: 10; height: 10; radius: 5
                color: "#00000055"
                anchors.centerIn: ovPin
                anchors.verticalCenterOffset: 1
            }
            Rectangle {
                id: ovPin
                width: 7; height: 7; radius: 3.5
                color: "#ff6b6f"
                border.width: 1
                border.color: "#ffb3b5"
                anchors.verticalCenter: parent.verticalCenter
                x: -3.5
            }
        }
    }

    // same glossy-chip recipe as the table's live events (gradient, tinted
    // border, top highlight, left accent bar) so overflow chips read as the
    // same kind of thing, just compressed, rather than a different widget
    component OverflowChip: Rectangle {
        radius: 6
        border.width: 1
        border.color: Qt.rgba(modelData.item.color.r, modelData.item.color.g, modelData.item.color.b, 0.5)
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.rgba(modelData.item.color.r, modelData.item.color.g, modelData.item.color.b, 0.34) }
            GradientStop { position: 1.0; color: Qt.rgba(modelData.item.color.r, modelData.item.color.g, modelData.item.color.b, 0.14) }
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
            color: modelData.item.color
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.margins: 2
        }
        Text {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 4
            text: grid.formatHour(modelData.item.start) + " " + modelData.item.event
            color: "#eef2f9"
            font.pixelSize: 9
            font.bold: true
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }
    }
}
