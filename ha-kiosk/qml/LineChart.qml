import QtQuick

// Minimal single-hue trend line — no chart library dependency, just Canvas.
// Thin 2px line, soft area fill, small dots at each sample, hour labels
// below. Points past the last non-forecast entry are drawn as a dashed
// projection (no fill, hollow dots) with an icon glyph above each one, and
// a faint vertical rule marks the history/forecast boundary — one
// continuous timeline instead of stitching a separate forecast strip on.
Item {
    id: chart
    property var points: []           // [{ label, value, icon (optional), forecast (optional bool) }]
    property color lineColor: "#199e70"
    property color forecastColor: "#4d6f96"
    property int labelEvery: 1         // show every Nth label, to avoid crowding

    implicitHeight: 160

    readonly property real minValue: {
        if (points.length === 0) return 0
        let m = points[0].value
        for (let i = 1; i < points.length; i++) if (points[i].value < m) m = points[i].value
        return m
    }
    readonly property real maxValue: {
        if (points.length === 0) return 1
        let m = points[0].value
        for (let i = 1; i < points.length; i++) if (points[i].value > m) m = points[i].value
        return m
    }

    // Last index that isn't a forecast point — history is assumed
    // contiguous and first, so this doubles as the "now" boundary.
    readonly property int lastHistoryIndex: {
        let idx = 0
        for (let i = 0; i < points.length; i++) if (!points[i].forecast) idx = i
        return idx
    }

    readonly property real paddingX: 6
    readonly property real paddingTop: 24  // headroom for the forecast icon row
    readonly property real paddingBottom: 6
    readonly property real plotWidth: width - paddingX * 2
    readonly property real plotHeight: canvas.height - paddingTop - paddingBottom

    function xAt(i) {
        return paddingX + i * (plotWidth / Math.max(points.length - 1, 1))
    }
    function yAt(v) {
        const range = Math.max(1, maxValue - minValue)
        return paddingTop + plotHeight * (1 - (v - minValue) / range)
    }

    onPointsChanged: canvas.requestPaint()
    onWidthChanged: canvas.requestPaint()

    Canvas {
        id: canvas
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: parent.height - 22

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            if (chart.points.length < 2) return

            const h = height
            const lastHist = chart.lastHistoryIndex

            // area fill — history only, so it's visually clear the shaded
            // region is measured data, not projection
            ctx.beginPath()
            ctx.moveTo(chart.xAt(0), h)
            for (let i = 0; i <= lastHist; i++) ctx.lineTo(chart.xAt(i), chart.yAt(chart.points[i].value))
            ctx.lineTo(chart.xAt(lastHist), h)
            ctx.closePath()
            const grad = ctx.createLinearGradient(0, 0, 0, h)
            grad.addColorStop(0, Qt.rgba(chart.lineColor.r, chart.lineColor.g, chart.lineColor.b, 0.30))
            grad.addColorStop(1, Qt.rgba(chart.lineColor.r, chart.lineColor.g, chart.lineColor.b, 0.0))
            ctx.fillStyle = grad
            ctx.fill()

            // history stroke (solid)
            ctx.beginPath()
            ctx.lineWidth = 2
            ctx.strokeStyle = chart.lineColor
            ctx.lineJoin = "round"
            ctx.lineCap = "round"
            for (let i = 0; i <= lastHist; i++) {
                const x = chart.xAt(i), y = chart.yAt(chart.points[i].value)
                if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y)
            }
            ctx.stroke()

            // forecast stroke (dashed), continuing on from the last history point
            if (lastHist < chart.points.length - 1) {
                ctx.beginPath()
                ctx.strokeStyle = chart.forecastColor
                ctx.setLineDash([5, 4])
                for (let i = lastHist; i < chart.points.length; i++) {
                    const x = chart.xAt(i), y = chart.yAt(chart.points[i].value)
                    if (i === lastHist) ctx.moveTo(x, y); else ctx.lineTo(x, y)
                }
                ctx.stroke()
                ctx.setLineDash([])
            }

            // dots — filled for history, hollow ring for forecast
            for (let i = 0; i < chart.points.length; i++) {
                const x = chart.xAt(i), y = chart.yAt(chart.points[i].value)
                ctx.beginPath()
                ctx.arc(x, y, 2.5, 0, Math.PI * 2)
                if (chart.points[i].forecast) {
                    ctx.lineWidth = 1.5
                    ctx.strokeStyle = chart.forecastColor
                    ctx.stroke()
                } else {
                    ctx.fillStyle = chart.lineColor
                    ctx.fill()
                }
            }
        }
    }

    // "now" boundary rule
    Rectangle {
        visible: chart.lastHistoryIndex < chart.points.length - 1
        x: chart.xAt(chart.lastHistoryIndex) - 0.5
        y: 0
        width: 1
        height: canvas.height
        color: "#2c3f5c"
    }

    // forecast condition icons, one above each projected point
    Repeater {
        model: chart.points
        delegate: Text {
            visible: !!modelData.icon
            text: modelData.icon || ""
            font.pixelSize: 13
            x: chart.xAt(index) - width / 2
            y: chart.yAt(modelData.value) - height - 6
        }
    }

    Row {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        Repeater {
            model: chart.points
            delegate: Text {
                width: chart.width / chart.points.length
                horizontalAlignment: Text.AlignHCenter
                text: index % chart.labelEvery === 0 ? modelData.label : ""
                color: "#8296b8"
                font.pixelSize: 11
            }
        }
    }
}
