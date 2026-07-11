import QtQuick
import QtQuick.Layouts

// Shown when the calendar-sync daemon falls back to the delegated-user
// OAuth "TV and limited input" device flow (see DelegatedAuth on the
// daemon side) to satisfy an invite/uninvite the service account alone
// can't do. Unlike ErrorBanner this can't auto-dismiss after a few
// seconds — a human has to go complete the grant on a phone or laptop,
// which realistically takes minutes, not seconds — so it stays up until
// either the grant's own deadline passes or someone dismisses it.
Item {
    id: banner
    property string verificationUrl: ""
    property string userCode: ""
    property int remainingSecs: 0
    visible: false
    height: visible ? content.implicitHeight + 24 : 0
    clip: true
    Behavior on height { NumberAnimation { duration: 150; easing.type: Easing.OutQuad } }

    function show(url, code, expiresInSecs) {
        verificationUrl = url
        userCode = code
        remainingSecs = expiresInSecs
        visible = true
        countdown.restart()
    }
    function hide() {
        visible = false
        countdown.stop()
    }

    Timer {
        id: countdown
        interval: 1000
        repeat: true
        onTriggered: {
            banner.remainingSecs -= 1
            if (banner.remainingSecs <= 0)
                banner.hide()
        }
    }

    function formattedRemaining() {
        const m = Math.floor(Math.max(0, remainingSecs) / 60)
        const s = Math.max(0, remainingSecs) % 60
        return m + ":" + (s < 10 ? "0" : "") + s
    }

    Rectangle {
        anchors.fill: parent
        color: "#122b3d"
        border.color: "#4dabf7"
        border.width: 1
        radius: 10
        opacity: 0.97
    }

    RowLayout {
        id: content
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        anchors.topMargin: 12
        anchors.bottomMargin: 12
        spacing: 14

        Text { text: "🔑"; color: "#8ecdf7"; font.pixelSize: 20 }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Text {
                text: "Calendar needs one-time authorization to send invites"
                color: "#d6ecfc"
                font.pixelSize: 13
                font.bold: true
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            Text {
                text: "Visit " + banner.verificationUrl + " and enter code:"
                color: "#a9d3f0"
                font.pixelSize: 12
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            Text {
                text: banner.userCode
                color: "#ffffff"
                font.pixelSize: 20
                font.bold: true
                font.family: "monospace"
                font.letterSpacing: 2
            }
            Text {
                text: "expires in " + banner.formattedRemaining()
                color: "#7fa8c9"
                font.pixelSize: 11
            }
        }

        Text {
            text: "✕"
            color: "#d6ecfc"
            font.pixelSize: 16
            font.bold: true
            MouseArea { anchors.fill: parent; anchors.margins: -10; onClicked: banner.hide() }
        }
    }
}
