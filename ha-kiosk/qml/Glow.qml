import QtQuick

// A soft radial wash in a given hue — used instead of a flat icon "chip" so
// accent colors bleed into the background rather than sitting inside a hard
// rounded-rect boundary. Canvas-drawn (no extra Qt module) radial gradient.
Item {
    id: glow
    property color color: Theme.accentBlue
    property real intensity: 0.75
    implicitWidth: 44
    implicitHeight: 44

    Canvas {
        id: canvas
        anchors.fill: parent
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            const r = Math.min(width, height) / 2
            const g = ctx.createRadialGradient(width / 2, height / 2, 0, width / 2, height / 2, r)
            g.addColorStop(0, Qt.rgba(glow.color.r, glow.color.g, glow.color.b, glow.intensity))
            g.addColorStop(0.7, Qt.rgba(glow.color.r, glow.color.g, glow.color.b, glow.intensity * 0.35))
            g.addColorStop(1, Qt.rgba(glow.color.r, glow.color.g, glow.color.b, 0))
            ctx.fillStyle = g
            ctx.beginPath()
            ctx.arc(width / 2, height / 2, r, 0, Math.PI * 2)
            ctx.fill()
        }
        Component.onCompleted: requestPaint()
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
    }
    Connections {
        target: glow
        function onColorChanged() { canvas.requestPaint() }
        function onIntensityChanged() { canvas.requestPaint() }
    }
}
