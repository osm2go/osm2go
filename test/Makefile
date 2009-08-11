#
#

all: statusbar_bg fremantle_submenu fremantle_popup

statusbar_bg: statusbar_bg.c
	gcc `pkg-config --libs --cflags gtk+-2.0` -o $@ $<

fremantle_submenu: fremantle_submenu.c
	gcc `pkg-config --libs --cflags hildon-1` -o $@ $<

fremantle_popup: fremantle_popup.c
	gcc `pkg-config --libs --cflags hildon-1` -o $@ $<

clean:
	rm -f statusbar_bg fremantle_submenu *.o
