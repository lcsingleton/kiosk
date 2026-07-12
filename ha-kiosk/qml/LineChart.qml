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
    readonly property real paddingLeft: 32  // gutter for y-axis grad labels
    readonly property real paddingTop: 24  // headroom for the forecast icon row
    readonly property real paddingBottom: 6
    readonly property real plotWidth: width - paddingLeft - paddingX
    readonly property real plotHeight: canvas.height - paddingTop - paddingBottom

    readonly property int minorPerMajor: 4  // minor gridlines drawn between each pair of majors

    // "Nice" round number close to range, per the standard Heckbert/Sparks
    // tick algorithm, so grad labels read as 5/10/25/50 rather than
    // whatever the raw data span happens to be.
    function niceNum(range, round) {
        const exponent = Math.floor(Math.log10(range))
        const fraction = range / Math.pow(10, exponent)
        let niceFraction
        if (round) {
            if (fraction < 1.5) niceFraction = 1
            else if (fraction < 3) niceFraction = 2
            else if (fraction < 7) niceFraction = 5
            else niceFraction = 10
        } else {
            if (fraction <= 1) niceFraction = 1
            else if (fraction <= 2) niceFraction = 2
            else if (fraction <= 5) niceFraction = 5
            else niceFraction = 10
        }
        return niceFraction * Math.pow(10, exponent)
    }

    // Axis domain snapped to nice round ticks (rather than the raw
    // min/maxValue) so major gridlines land on legible values and the
    // line gets a little headroom instead of touching the plot edges.
    readonly property var axisTicks: {
        const span = maxValue - minValue
        if (span <= 0) {
            const step = niceNum(Math.max(Math.abs(maxValue), 1), true) / minorPerMajor || 1
            return { min: minValue - step, max: maxValue + step, step: step }
        }
        const targetMajors = 4
        const step = niceNum(span / targetMajors, true)
        return { min: Math.floor(minValue / step) * step, max: Math.ceil(maxValue / step) * step, step: step }
    }
    readonly property real axisMin: axisTicks.min
    readonly property real axisMax: axisTicks.max
    readonly property real axisStep: axisTicks.step

    function formatTick(v) {
        return String(Math.round(v * 100) / 100)
    }

    function xAt(i) {
        return paddingLeft + i * (plotWidth / Math.max(points.length - 1, 1))
    }
    function yAt(v) {
        const range = Math.max(axisMax - axisMin, 0.0001)
        return paddingTop + plotHeight * (1 - (v - axisMin) / range)
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

            // y-axis gridlines — minor first so majors draw over them, then
            // major grad labels along the left gutter
            const gridLeft = chart.paddingLeft
            const gridRight = width - chart.paddingX
            const majorCount = Math.round((chart.axisMax - chart.axisMin) / chart.axisStep)
            const minorStep = chart.axisStep / chart.minorPerMajor
            const minorCount = majorCount * chart.minorPerMajor

            ctx.lineWidth = 1
            ctx.strokeStyle = "rgba(130, 150, 184, 0.12)"
            ctx.beginPath()
            for (let i = 0; i <= minorCount; i++) {
                if (i % chart.minorPerMajor === 0) continue
                const y = chart.yAt(chart.axisMin + i * minorStep)
                ctx.moveTo(gridLeft, y)
                ctx.lineTo(gridRight, y)
            }
            ctx.stroke()

            ctx.font = "11px sans-serif"
            ctx.fillStyle = "#8296b8"
            ctx.textAlign = "right"
            ctx.textBaseline = "middle"
            ctx.strokeStyle = "rgba(130, 150, 184, 0.3)"
            for (let i = 0; i <= majorCount; i++) {
                const v = chart.axisMin + i * chart.axisStep
                const y = chart.yAt(v)
                ctx.beginPath()
                ctx.moveTo(gridLeft, y)
                ctx.lineTo(gridRight, y)
                ctx.stroke()
                ctx.fillText(chart.formatTick(v), gridLeft - 6, y)
            }

            // x-axis gridlines — one major per point (each point is exactly
            // one BOM hourly sample), minor lines quartering each hour into
            // 15-minute steps. Hour labels already live in the Row below the
            // canvas, so these are lines only, no text.
            const gridTop = chart.paddingTop
            const gridBottom = h - chart.paddingBottom
            const xMinorPerMajor = 4

            ctx.lineWidth = 1
            ctx.strokeStyle = "rgba(130, 150, 184, 0.12)"
            ctx.beginPath()
            for (let i = 0; i < chart.points.length - 1; i++) {
                for (let m = 1; m < xMinorPerMajor; m++) {
                    const x = chart.xAt(i + m / xMinorPerMajor)
                    ctx.moveTo(x, gridTop)
                    ctx.lineTo(x, gridBottom)
                }
            }
            ctx.stroke()

            ctx.strokeStyle = "rgba(130, 150, 184, 0.3)"
            ctx.beginPath()
            for (let i = 0; i < chart.points.length; i++) {
                const x = chart.xAt(i)
                ctx.moveTo(x, gridTop)
                ctx.lineTo(x, gridBottom)
            }
            ctx.stroke()

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
        x: chart.paddingLeft
        width: chart.plotWidth
        anchors.bottom: parent.bottom
        Repeater {
            model: chart.points
            delegate: Text {
                width: chart.plotWidth / chart.points.length
                horizontalAlignment: Text.AlignHCenter
                text: index % chart.labelEvery === 0 ? modelData.label : ""
                color: "#8296b8"
                font.pixelSize: 11
            }
        }
    }
}
