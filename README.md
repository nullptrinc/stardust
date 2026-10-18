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

## System state

- `Theme` (in `Stardust 1.0`): the system accent and its shades (`accent`,
  `accentLit`, `accentGlow`, `accentShade`, `accentDeep`), shared by all apps
  through `/blinks/system/appearance`; `Theme.setAccent("#4fc3ff")`.
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
