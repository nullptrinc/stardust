// SPDX-License-Identifier: MIT
// A TV-style screen for trying focus with a remote or a keyboard: two rows
// of cards that scroll, a hero line for the focused card, and a detail page.
// The arrow keys move the focus, OK opens a card, Back closes the page or,
// on the first page, quits. Run it with STARRY_MODE=tv for the TV theme:
//
//     STARRY_MODE=tv stardust tv.qml
import Stardust 1.0

Window {
    id: window
    title: "Stardust TV"
    appId: "stardust-tv-demo"
    layout: "dock"
    background: Theme.background

    property string current: ""
    property string opened: ""
    property string lastKey: ""
    property var last: null
    // Rows keep the lift inside the safe area; the page itself pads by safeMarginY.
    readonly property real rowPadding: Theme.safeMarginX - Theme.safeMarginY

    function open(card) {
        last = card
        opened = card.title
        backButton.forceFocus()
    }
    function close() {
        opened = ""
        if (last) last.forceFocus()
    }

    onKeyPressed: (key) => lastKey = key

    // A card: solid fill, the theme's lift; the hero follows the focus.
    component Card: Container {
        id: card
        property string title
        property string tint: Theme.surface
        readonly property var self: card
        focusable: true
        preferredWidth: Theme.cardWidth
        preferredHeight: Theme.cardHeight
        radius: Theme.cardRadius
        background: tint
        shadowColor: Theme.shadowColor
        focusScale: Theme.focusScale
        focusShadow: Theme.focusShadow
        focusHighlight: Theme.focusHighlight
        focusTilt: Theme.focusTilt
        scale: card.pressed ? 0.96 : 1
        layout: "dock"
        Label {
            text: card.title
            fontSize: Theme.fontBody
            bold: true
            textColor: Theme.textPrimary
            horizontalAlignment: "center"
            verticalAlignment: "center"
        }
    }

    component Heading: Container {
        property alias text: label.text
        padding: window.rowPadding
        Label { id: label; fontSize: Theme.fontHeading; bold: true; textColor: Theme.textSecondary }
    }

    function timeNow() {
        var now = new Date()
        return ("0" + now.getHours()).slice(-2) + ":" + ("0" + now.getMinutes()).slice(-2)
    }

    property string time: timeNow()
    property Timer clock: Timer {
        interval: 1000
        repeat: true
        running: true
        onTriggered: window.time = window.timeNow()
    }

    // The home page.
    Container {
        padding: Theme.safeMarginY
        verticalAlignment: "fill"
        opacity: window.opened ? 0 : 1

        Container {
            layout: "dock"
            preferredHeight: 250
            Container {
                padding: window.rowPadding
                spacing: 8
                Label {
                    text: window.current
                    fontSize: Theme.fontTitle * 1.3
                    bold: true
                    textColor: Theme.textPrimary
                }
                Label {
                    text: window.lastKey ? "Key: " + window.lastKey : "OK opens, Back quits"
                    fontSize: Theme.fontCaption
                    textColor: Theme.textSecondary
                }
            }
            Label {
                text: window.time
                fontSize: Theme.fontHeading
                bold: true
                textColor: Theme.textPrimary
                horizontalAlignment: "end"
                verticalAlignment: "start"
            }
        }

        Heading { text: "Featured"; preferredHeight: 60 }
        Container {
            scroll: "horizontal"
            orientation: "horizontal"
            spacing: Theme.cardSpacing
            padding: window.rowPadding
            Card { title: "Films"; tint: "#7a3b3b"; onFocusedChanged: if (focused) window.current = title; onActivated: window.open(self) }
            Card { title: "Series"; tint: "#5b3f7a"; onFocusedChanged: if (focused) window.current = title; onActivated: window.open(self) }
            Card { title: "Music"; tint: "#2f5d7a"; onFocusedChanged: if (focused) window.current = title; onActivated: window.open(self) }
            Card { title: "Radio"; tint: "#7a6a2f"; onFocusedChanged: if (focused) window.current = title; onActivated: window.open(self) }
            Card { title: "Photos"; tint: "#2f7a5b"; onFocusedChanged: if (focused) window.current = title; onActivated: window.open(self) }
            Card { title: "Weather"; tint: "#2f4f7a"; onFocusedChanged: if (focused) window.current = title; onActivated: window.open(self) }
            Card { title: "News"; tint: "#7a4a2f"; onFocusedChanged: if (focused) window.current = title; onActivated: window.open(self) }
            Card { title: "Games"; tint: "#3f7a2f"; onFocusedChanged: if (focused) window.current = title; onActivated: window.open(self) }
        }

        Heading { text: "Library"; preferredHeight: 60 }
        Container {
            scroll: "horizontal"
            orientation: "horizontal"
            spacing: Theme.cardSpacing
            padding: window.rowPadding
            Card { title: "Recent"; onFocusedChanged: if (focused) window.current = title; onActivated: window.open(self) }
            Card { title: "Downloads"; onFocusedChanged: if (focused) window.current = title; onActivated: window.open(self) }
            Card { title: "Favourites"; onFocusedChanged: if (focused) window.current = title; onActivated: window.open(self) }
            Card { title: "Kids"; onFocusedChanged: if (focused) window.current = title; onActivated: window.open(self) }
            Card { title: "Documentaries"; onFocusedChanged: if (focused) window.current = title; onActivated: window.open(self) }
            Card { title: "Concerts"; onFocusedChanged: if (focused) window.current = title; onActivated: window.open(self) }
            Card { title: "Sports"; onFocusedChanged: if (focused) window.current = title; onActivated: window.open(self) }
            Card { title: "Settings"; onFocusedChanged: if (focused) window.current = title; onActivated: window.open(self) }
        }
    }

    // A detail page. Back here closes it instead of quitting.
    Container {
        layout: "dock"
        verticalAlignment: "fill"
        opacity: window.opened ? 1 : 0
        onBack: window.close()

        Container {
            horizontalAlignment: "center"
            verticalAlignment: "center"
            spacing: 32
            Label {
                text: window.opened
                fontSize: Theme.fontTitle * 1.3
                bold: true
                textColor: Theme.textPrimary
                horizontalAlignment: "center"
            }
            Label {
                text: "An app would open here."
                fontSize: Theme.fontBody
                textColor: Theme.textSecondary
                horizontalAlignment: "center"
            }
            Container {
                id: backButton
                focusable: true
                horizontalAlignment: "center"
                padding: 20
                radius: 16
                background: backButton.focused ? Theme.surfaceRaised : Theme.surface
                shadowColor: Theme.shadowColor
                focusScale: Theme.focusScale
                focusShadow: Theme.focusShadow
                focusHighlight: Theme.focusHighlight
                focusTilt: Theme.focusTilt
                onActivated: window.close()
                Label { text: "Back"; fontSize: Theme.fontBody; bold: true; textColor: Theme.textPrimary }
            }
        }
    }
}
