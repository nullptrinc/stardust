# Stardust

Stardust is the application framework of [Starry](https://github.com/nullptrinc/starrysky),
in the spirit of BlackBerry 10's Cascades. An application describes its UI in
QML; the QtQml engine is used only as the language. Every property change,
bindings included, becomes a mutation for a render core that runs on its own
thread and owns the window, layout (StackLayout, DockLayout), implicit
animations, touch handling and GLES2 drawing. A busy application thread
therefore never stalls an animation: an `onClicked` handler that blocks for a
second leaves a running slide at 60 frames per second.

```qml
import Stardust 1.0

Window {
    layout: "dock"
    background: "#101425"
    Container {
        id: card
        padding: 18
        radius: 16
        background: card.pressed ? "#5a9cff" : "#2f80ed"
        scale: card.pressed ? 0.96 : 1
        onClicked: console.log("tapped")
        Label { text: "Tap me"; fontSize: 18; bold: true }
    }
}
```

Run it with `stardust app.qml`. Elements: `Window`, `Container`, `Label`,
`Timer`, `Plot`. `STARDUST_TRACE=1` prints frame statistics for every
animation; `STARDUST_CAPTURE=file.ppm` saves one frame of the window.

## Materials

One shader draws every container as a rounded box with optional effects, all
animated like other properties:

| Property | |
|---|---|
| `background`, `backgroundEnd` | a vertical gradient |
| `gloss` | glass: a brighter upper half with a horizon |
| `material` | `plain`, `brushed` metal, or `chrome` that reflects `Window.lightAngle` |
| `borderWidth`, `borderColor` | |
| `bevel` | lit top edge, shaded bottom; negative sinks it |
| `innerShadow`, `shadow`, `shadowColor` | a recess, a drop shadow |
| `innerGlow`, `glow`, `glowColor` | light inside and around the edge |

Labels add `textShadow`, `textShadowOffset`, `textGlow` (a real Gaussian
blur) and `textGlowColor`.

## Focus and remote controls

Keys come through the ordinary Wayland keyboard. A `focusable` container
takes part in directional navigation: the arrow keys move the focus to the
nearest focusable container in that direction, preferring those in line,
and holding an arrow repeats. The render core animates the move on its own
thread: the focused container lifts by `focusScale` on a spring, gains
`focusShadow` and a `focusHighlight` sheen, and leans `focusTilt` px the way
the focus came from; a lifted container draws over its neighbours.

```qml
Container {
    scroll: "horizontal"        // keeps the focused card in view
    orientation: "horizontal"
    padding: 40                 // room for the lift at the ends
    spacing: 40
    Container {
        focusable: true
        preferredWidth: 300; preferredHeight: 180; radius: 20
        background: focused ? "#3a4a6a" : "#262b38"
        onActivated: console.log("OK")
    }
}
```

| | |
|---|---|
| `focused` | follows the focus; `forceFocus()` moves it here |
| `onActivated` | OK (or Enter) on the focused container; a tap and OK stand in for each other when only `onClicked` or `onActivated` is handled |
| `onBack` | Back or Escape, on the focused container or its nearest ancestor that handles it; with no handler the app quits and the shell comes back |
| `onKeyPressed(key)`, `onKeyReleased(key)` | any other key by name (`"playpause"`, `"volumeup"`, `"info"`, `"0"`…), and an arrow with nowhere to go |
| `scroll` | `"horizontal"` or `"vertical"`: moves the content to show the focused descendant within the padding |

The first focusable container takes the focus when there is none; when the
focused one goes away or is hidden, the nearest one takes it.

## System state

- `Theme` (in `Stardust 1.0`): the system accent and its shades (`accent`,
  `accentLit`, `accentGlow`, `accentShade`, `accentDeep`), shared by all apps
  through `/blinks/system/appearance`; `Theme.setAccent("#4fc3ff")`.
  `STARRY_MODE=tv` selects the TV variant (`Theme.tv`, `Theme.formFactor`):
  sizes for 1920x1080 at sofa distance (`safeMarginX/Y`, `cardWidth`,
  `cardHeight`, `cardRadius`, `cardSpacing`, `fontTitle`…`fontCaption`), the
  focus lift (`focusScale`, `focusShadow`, `focusHighlight`, `focusTilt`) and
  solid colours (`background`, `surface`, `surfaceRaised`, `textPrimary`,
  `textSecondary`, `shadowColor`). Mali-400/450 GPUs have no highp in
  fragment shaders, so the TV look keeps to solid fills, soft shadows and a
  faint sheen; long `backgroundEnd` gradients and the `brushed` and `chrome`
  materials band or break there.
- `import Constellation 1.0`: `Battery` for the battery, `Blink { path: ... }`
  for any object of the [Constellation](https://github.com/nullptrinc/constellation)
  namespace (`values`, `exists`, `publish({...})`).

Apps are packaged as `.apf` with [cosmic](https://github.com/nullptrinc/cosmic);
the system apps live in [sysapps](https://github.com/nullptrinc/sysapps).

## Building

Needs Qt 6 (Core, Qml), wayland, EGL, GLES2, freetype and the Constellation
client library.

```sh
meson setup build --prefix=/opt/starry --libdir=lib
ninja -C build
```

## License

MIT, see [LICENSE](LICENSE).
