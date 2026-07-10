import QtQuick

// A horizontal rule that fades in and out at each end instead of a hard
// edge-to-edge line — sections trail off into one another rather than
// being sealed shut by a boundary.
Item {
    id: div
    property color color: "#3987e5"
    property real peakOpacity: 0.4
    implicitHeight: 2

    Canvas {
        id: canvas
        anchors.fill: parent
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            const g = ctx.createLinearGradient(0, 0, width, 0)
            g.addColorStop(0, Qt.rgba(div.color.r, div.color.g, div.color.b, 0))
            g.addColorStop(0.5, Qt.rgba(div.color.r, div.color.g, div.color.b, div.peakOpacity))
            g.addColorStop(1, Qt.rgba(div.color.r, div.color.g, div.color.b, 0))
            ctx.fillStyle = g
            ctx.fillRect(0, 0, width, height)
        }
        Component.onCompleted: requestPaint()
        onWidthChanged: requestPaint()
    }
}
