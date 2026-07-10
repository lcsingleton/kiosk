import QtQuick

// A "section" of the dashboard, not a boxed card — no fill, no border, no
// rounded silhouette. Identity comes from the glow behind its icon and the
// accent color threaded through its own content; sections are separated by
// a FlowDivider that fades at both ends rather than a hard rule, so the
// page reads as one continuous surface instead of a grid of tiles.
Item {
    id: card
    property string title: ""
    property string icon: "●"
    property color accent: "#3987e5"
    property int contentSpacing: 10
    property bool showDivider: true
    default property alias content: body.children

    implicitHeight: layout.height + (showDivider ? 26 : 6)

    Column {
        id: layout
        width: card.width
        spacing: 14

        Row {
            spacing: 12
            Item {
                width: 38; height: 38
                anchors.verticalCenter: parent.verticalCenter
                Glow { anchors.fill: parent; color: card.accent }
                Text { anchors.centerIn: parent; text: card.icon; font.pixelSize: 18 }
            }
            Text {
                text: card.title
                font.pixelSize: 18
                font.bold: true
                color: "#eef2f9"
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        Column {
            id: body
            width: layout.width
            spacing: card.contentSpacing
        }
    }

    FlowDivider {
        visible: card.showDivider
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        color: card.accent
    }
}
