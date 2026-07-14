import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.VirtualKeyboard

Window {
    id: root
    visible: true
    width: 960
    height: 540
    color: Theme.background
    title: "ha-tab kiosk (dev preview — resize me)"

    DashboardData { id: data }

    // Normalizes a tapped item from whichever list it came from
    // (todaySchedule uses "event" for its title field; weekend/upcoming use
    // "title") into the one shape EventEditPopup expects.
    function openEditPopup(item, titleKey) {
        eventEditPopup.isNew = false
        eventEditPopup.event = {
            calendarId: item.calendarId,
            eventId: item.eventId,
            etag: item.etag,
            title: item[titleKey],
            startIso: item.startIso,
            endIso: item.endIso,
            attendees: item.attendees
        }
        eventEditPopup.open()
    }

    // personName decides the popup's initial hour and prefills them as an
    // attendee (the column the user tapped in) — but not which calendar it's
    // created on: there's no per-person calendar, so every new event lands
    // on the household's default calendar regardless of which person's
    // column was tapped — see main.cpp's "defaultCalendarId" comment.
    function openCreatePopup(personName, approxHour) {
        eventEditPopup.isNew = true
        eventEditPopup.newCalendarId = calendarBridge.defaultCalendarId
        eventEditPopup.newAttendee = personName
        eventEditPopup.event = { title: "" }
        eventEditPopup.newStartHour = approxHour
        eventEditPopup.open()
    }

    // Re-finds the same event by id across all three lists — used to keep
    // the open popup's etag current as the snapshot refreshes, so a second
    // action in the same popup session (e.g. Rename then Move) isn't
    // rejected as a conflict against an etag the first action already
    // advanced past.
    function findLiveEvent(eventId) {
        let found = data.todaySchedule.find(e => e.eventId === eventId)
        if (found) return { calendarId: found.calendarId, eventId: found.eventId, etag: found.etag, title: found.event, startIso: found.startIso, endIso: found.endIso, attendees: found.attendees }
        found = data.weekend.find(e => e.eventId === eventId)
        if (found) return { calendarId: found.calendarId, eventId: found.eventId, etag: found.etag, title: found.title, startIso: found.startIso, endIso: found.endIso, attendees: found.attendees }
        found = data.upcoming.find(e => e.eventId === eventId)
        if (found) return { calendarId: found.calendarId, eventId: found.eventId, etag: found.etag, title: found.title, startIso: found.startIso, endIso: found.endIso, attendees: found.attendees }
        return null
    }

    Connections {
        target: calendarBridge
        function onSnapshotChanged() {
            if (eventEditPopup.visible && !eventEditPopup.isNew && eventEditPopup.event) {
                const fresh = findLiveEvent(eventEditPopup.event.eventId)
                if (fresh)
                    eventEditPopup.event = fresh
            }
        }
        function onCommandFailed(commandId, what, errorCode, errorMessage) {
            errorBanner.show(what + ": " + errorMessage)
        }
        function onAuthorizationRequired(verificationUrl, userCode, expiresInSecs) {
            authPromptBanner.show(verificationUrl, userCode, expiresInSecs)
        }
    }

    // Fixed virtual resolution matching the tablet's portrait mount.
    // Uniformly scaled (never stretched) to fit however this dev window is
    // resized, and letterboxed/centered on whatever's left over.
    Item {
        id: canvas
        width: 1920
        height: 1080
        transformOrigin: Item.TopLeft
        scale: Math.min(root.width / width, root.height / height)
        x: (root.width - width * scale) / 2
        y: (root.height - height * scale) / 2

        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: Theme.canvasGradientTop }
                GradientStop { position: 1.0; color: Theme.canvasGradientBottom }
            }
        }

        // ---- header: clock + date + settings --------------------------
        Item {
            id: header
            width: parent.width
            height: 72

            Text {
                id: clock
                anchors.left: parent.left
                anchors.leftMargin: 24
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.textPrimary
                font.pixelSize: 28
                font.bold: true
            }
            Text {
                id: dateLabel
                anchors.left: clock.right
                anchors.leftMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.textSecondary
                font.pixelSize: 16
            }
            Text {
                anchors.right: parent.right
                anchors.rightMargin: 24
                anchors.verticalCenter: parent.verticalCenter
                text: "⚙"
                color: Theme.textSecondary
                font.pixelSize: 22
            }

            Timer {
                interval: 1000
                running: true
                repeat: true
                triggeredOnStart: true
                onTriggered: {
                    const now = new Date()
                    clock.text = Qt.formatDateTime(now, "h:mm AP")
                    dateLabel.text = Qt.formatDateTime(now, "dddd, MMMM d")
                }
            }
        }

        // ---- dashboard body ----------------------------------------------
        // Canvas is a fixed 1920x1080, so the grid never scrolls as a
        // whole; individual cards implement their own internal scrolling
        // where their content can exceed the space available to them.
        GridLayout {
            id: dashboardGrid
            anchors.top: header.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.topMargin: 8
            anchors.leftMargin: 18
            anchors.rightMargin: 18
            columns: 2
            columnSpacing: 14
            rowSpacing: 14

            // paired with ShoppingListCard below (both compact,
            // controls-first) so the grid actually earns its two
            // columns instead of every section spanning full width
            // like a single stacked feed
            ClimateCard {
                Layout.fillWidth: true
                Layout.fillHeight: false
                Layout.alignment: Qt.AlignTop
                dashboardData: data
            }

            ShoppingListCard {
                Layout.fillWidth: true
                Layout.fillHeight: false
                Layout.alignment: Qt.AlignTop
                dashboardData: data
            }

            WeatherCard {
                Layout.columnSpan: 2
                Layout.fillWidth: true
                dashboardData: data
            }

            CalendarCard {
                Layout.columnSpan: 2
                Layout.fillWidth: true
                dashboardData: data
                onEditRequested: (item, titleKey) => openEditPopup(item, titleKey)
                onCreateRequested: (person, hour) => openCreatePopup(person, hour)
            }
        }

        InputPanel {
            id: inputPanel
            z: 99
            y: Qt.inputMethod.visible ? canvas.height - height : canvas.height
            anchors.left: parent.left
            anchors.right: parent.right
        }

        Column {
            id: bannerColumn
            z: 100
            anchors.top: canvas.top
            anchors.left: canvas.left
            anchors.right: canvas.right
            anchors.topMargin: 84
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: 8

            ErrorBanner {
                id: errorBanner
                anchors.left: parent.left
                anchors.right: parent.right
            }

            AuthPromptBanner {
                id: authPromptBanner
                anchors.left: parent.left
                anchors.right: parent.right
            }
        }

        EventEditPopup {
            id: eventEditPopup
            parent: canvas
            dashboardData: data
        }

        // Software-only dim for now — see IdleController; real backlight
        // sysfs control is a follow-up once the actual device path is
        // confirmed on hardware. Above everything else (including popups),
        // since "asleep" should cover the whole screen regardless of what
        // was open when the idle timeout hit.
        Rectangle {
            id: sleepOverlay
            anchors.fill: parent
            z: 1000
            color: "black"
            opacity: data.screenAsleep ? 1 : 0
            visible: opacity > 0
            Behavior on opacity {
                NumberAnimation { duration: 400 }
            }
        }
    }
}
