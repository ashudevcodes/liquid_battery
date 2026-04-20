# liquid-battery

Animated battery indicator for the XFCE4 panel. Draws a liquid wave that fills and sloshes based on charge level and state.

## Features

- Smooth animated wave fill
- Color shifts by state: green (charging), red (≤10%), orange (≤20%), gray (normal)
- Wave amplitude reacts to: charging progress, hover, battery drop, state change
- Reads live data from UPower via D-Bus

## Dependencies

- GTK+ 3
- GLib / GIO
- libxfce4panel
- UPower (running on D-Bus system bus)

## Build & Install

```sh
make          # build → ./build/libliquid-battery.so
make test     # build + install to ~/.local/…  + restart xfce4-panel
make install  # same but no panel restart (cp instead of mv)
make nuke     # remove build dir and all installed files
```

Files land in:
- `~/.local/lib/xfce4/panel/plugins/libliquid-battery.so`
- `~/.local/share/xfce4/panel/plugins/liquid-battery.desktop`

After install, add via **Panel Preferences → Items → +**.

## Notes

- Targets `/org/freedesktop/UPower/devices/DisplayDevice` — works on most single-battery laptops.
