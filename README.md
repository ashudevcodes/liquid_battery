# liquid-battery

<kdb><img src="./assets/plugin_image.png"/></kdb>

Animated battery indicator for the XFCE4 panel and Waybar. Draws a liquid wave that fills and sloshes based on charge level and state.

## Features

- Smooth animated wave fill
- Color shifts by state: green (charging), red (≤10%), orange (≤20%), gray (normal)
- Wave amplitude reacts to: charging progress, hover, battery drop, state change
- Reads live data from UPower via D-Bus

## Dependencies

- GTK+ 3
- GLib / GIO
- libxfce4panel [if using xfce4-panel]
- UPower (running on D-Bus system bus)

## Build & Install

```sh
# build -> ./build/libliquid-battery-xfce.so OR ./build/libliquid-battery-waybar.so
make xfce
make waybar

# install -> ~/.local/lib/waybar/modules/ OR ~/.local/lib/xfce4/panel/plugins/
make install xfce
make install-waybar

# remove build dir and all installed files
make nuke-xfce
make nuke-waybar
```

Files land in:
- `~/.local/lib/xfce4/panel/plugins/libliquid-battery.so`
- `~/.local/share/xfce4/panel/plugins/liquid-battery.desktop`

After install,
if you are using xfce4-panel then go to xfce4-panel **Panel Preferences → Items → +** and choos liquid-battery.
else if you are in waybar go to you wayland config and add this
``
  "modules-right": [
	"cffi/battery",
  ],

  "cffi/battery": {
	"module_path": "/home/<YOUR-USERNAME>/.local/lib/waybar/modules/libliquid-battery.so"
  }
``

## Notes

- Targets `/org/freedesktop/UPower/devices/DisplayDevice` — works on most single-battery laptops.

## TODO

- add a way to change ui and behavior using config.
- runtime config changes
