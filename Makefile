PLUGIN = liquid-battery

CFLAGS += `pkg-config --cflags gtk+-3.0 libxfce4panel-2.0 gio-2.0`
LIBS   += `pkg-config --libs gtk+-3.0 libxfce4panel-2.0 gio-2.0`

all:
	mkdir -p build
	$(CC) -fPIC -shared -o ./build/lib$(PLUGIN).so ./battery-plugin.c $(CFLAGS) $(LIBS)

test: all
	mkdir -p ~/.local/lib/xfce4/panel/plugins
	mkdir -p ~/.local/share/xfce4/panel/plugins

	mv ./build/lib$(PLUGIN).so ~/.local/lib/xfce4/panel/plugins/
	cp $(PLUGIN).desktop ~/.local/share/xfce4/panel/plugins/

	xfce4-panel -r

install: all
	mkdir -p ~/.local/lib/xfce4/panel/plugins
	mkdir -p ~/.local/share/xfce4/panel/plugins

	cp ./build/lib$(PLUGIN).so ~/.local/lib/xfce4/panel/plugins/
	cp $(PLUGIN).desktop ~/.local/share/xfce4/panel/plugins/

nuke:
	rm  -rfv ./build
	rm  -fv ~/.local/lib/xfce4/panel/plugins/lib$(PLUGIN).so
	rm  -fv ~/.local/share/xfce4/panel/plugins/$(PLUGIN).desktop
