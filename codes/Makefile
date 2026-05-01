PLUGIN = liquid-battery
SRCS   = ./starting_point/main.c \
         ./plugin_features/plugin_behavior.c \
         ./ui_part/draw.c \
         ./utility.c

CFLAGS_XFCE += -DPLUGIN_IS_XFCE
CFLAGS_XFCE += `pkg-config --cflags gtk+-3.0 libxfce4panel-2.0 gio-2.0`
LIBS_XFCE   += `pkg-config --libs gtk+-3.0 libxfce4panel-2.0 gio-2.0`

CFLAGS_WAYBAR += -DPLUGIN_IS_WAYBAR
CFLAGS_WAYBAR += `pkg-config --cflags gtk+-3.0 gio-2.0`
LIBS_WAYBAR   += `pkg-config --libs gtk+-3.0 gio-2.0`

all: waybar

xfce:
	mkdir -p build
	$(CC) -fPIC -shared -o ./build/lib$(PLUGIN)-xfce.so $(SRCS) $(CFLAGS_XFCE) $(LIBS_XFCE)

waybar:
	mkdir -p build
	$(CC) -fPIC -shared -o ./build/lib$(PLUGIN)-waybar.so $(SRCS) $(CFLAGS_WAYBAR) $(LIBS_WAYBAR)

test-xfce: xfce
	mkdir -p ~/.local/lib/xfce4/panel/plugins
	mkdir -p ~/.local/share/xfce4/panel/plugins
	mv ./build/lib$(PLUGIN)-xfce.so ~/.local/lib/xfce4/panel/plugins/lib$(PLUGIN).so
	cp $(PLUGIN).desktop ~/.local/share/xfce4/panel/plugins/
	xfce4-panel -r

test-waybar: waybar
	mkdir -p ~/.local/lib/waybar/modules
	mv ./build/lib$(PLUGIN)-waybar.so ~/.local/lib/waybar/modules/lib$(PLUGIN).so

install-xfce: xfce
	mkdir -p ~/.local/lib/xfce4/panel/plugins
	mkdir -p ~/.local/share/xfce4/panel/plugins
	cp ./build/lib$(PLUGIN)-xfce.so ~/.local/lib/xfce4/panel/plugins/lib$(PLUGIN).so
	cp $(PLUGIN).desktop ~/.local/share/xfce4/panel/plugins/

install-waybar: waybar
	mkdir -p ~/.local/lib/waybar/modules
	cp ./build/lib$(PLUGIN)-waybar.so ~/.local/lib/waybar/modules/lib$(PLUGIN).so

nuke-waybar:
	rm -rfv ./build
	rm -fv ~/.local/lib/waybar/modules/lib$(PLUGIN).so

nuke-xfce:
	rm -rfv ./build
	rm -fv ~/.local/lib/xfce4/panel/plugins/lib$(PLUGIN).so
	rm -fv ~/.local/share/xfce4/panel/plugins/$(PLUGIN).desktop
