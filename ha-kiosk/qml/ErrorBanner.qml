import QtQuick
import QtQuick.Layouts

// Dismissible feedback for a failed write command — named after what the
// user was doing ("Rescheduling 'Dentist checkup'"), not a generic error,
// so it's obvious which action to retry. Auto-dismisses after a while so a
// forgotten banner doesn't sit on the kiosk forever; a tap dismisses early.
Item {
    id: banner
    property string message: ""
    visible: false
    height: visible ? content.implicitHeight + 24 : 0
    clip: true
    Behavior on height { NumberAnimation { duration: 150; easing.type: Easing.OutQuad } }

    function show(text) {
        message = text
        visible = true
        autoHide.restart()
    }
    function hide() {
        visible = false
        autoHide.stop()
    }

    Timer { id: autoHide; interval: 10000; onTriggered: banner.hide() }

    Rectangle {
        anchors.fill: parent
        color: "#3d1414"
        border.color: "#e5484d"
        border.width: 1
        radius: 10
        opacity: 0.95
    }

    RowLayout {
        id: content
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        spacing: 10

        Text { text: "⚠"; color: "#ff6b6f"; font.pixelSize: 16 }
        Text {
            Layout.fillWidth: true
            text: banner.message
            color: "#f5d0d1"
            font.pixelSize: 13
            wrapMode: Text.WordWrap
        }
        Text {
            text: "✕"
            color: "#f5d0d1"
            font.pixelSize: 16
            font.bold: true
            MouseArea { anchors.fill: parent; anchors.margins: -10; onClicked: banner.hide() }
        }
    }
}
