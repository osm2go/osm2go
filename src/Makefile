#
# Copyright (C) 2008 Till Harbaum <till@harbaum.org>.
#
# This file is part of OSM2Go.
#
# OSM2Go is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# OSM2Go is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with OSM2Go.  If not, see <http://www.gnu.org/licenses/>.
#

APP = osm2go
PREFIX = /usr/local

PKGS = gtk+-2.0 libxml-2.0 goocanvas libcurl gthread-2.0 \
	gnome-vfs-module-2.0 libgnome-2.0

OBJS = main.o map.o osm.o osm_api.o wms.o \
	statusbar.o project.o settings.o pos.o diff.o map_hl.o \
	iconbar.o info.o icon.o track.o gps.o canvas_goocanvas.o \
	area_edit.o josm_presets.o relation_edit.o misc.o map_edit.o \
	josm_elemstyles.o style.o net_io.o banner.o

DEFS = -DPACKAGE=\"$(APP)\" -DPREFIX=\"$(PREFIX)\" -DOSM_STREAM_PARSER -DUSE_GOOCANVAS
CFLAGS = -Wall -O2 $(DEFS) `pkg-config --cflags $(PKGS)`
LDFLAGS = `pkg-config --libs $(PKGS)`

all: $(APP)

version.h: ../debian/changelog
	awk '!/^$$/ { print "#define VERSION \"" $$2 "\"";exit }' $< | tr -d \(\) > $@

%.o: %.c
	gcc $(CFLAGS) -o $@ -c $<

$(APP): $(OBJS)
	gcc $(LDFLAGS) -o $@ $(OBJS)

clean:
	rm -f osm2go *.o *~ */*~ \#*\# version.h *.bak

install: $(APP)
	install $(APP) $(DESTDIR)$(PREFIX)/bin

dep: version.h
	makedepend -Y *.c

# DO NOT DELETE

area_edit.o: appdata.h version.h pos.h osm.h canvas.h map.h map_hl.h
area_edit.o: osm_api.h project.h statusbar.h area_edit.h settings.h diff.h
area_edit.o: iconbar.h icon.h info.h track.h gps.h wms.h josm_presets.h
area_edit.o: relation_edit.h misc.h map_edit.h josm_elemstyles.h style.h
area_edit.o: net_io.h
banner.o: banner.h appdata.h version.h pos.h osm.h canvas.h map.h map_hl.h
banner.o: osm_api.h project.h statusbar.h area_edit.h settings.h diff.h
banner.o: iconbar.h icon.h info.h track.h gps.h wms.h josm_presets.h
banner.o: relation_edit.h misc.h map_edit.h josm_elemstyles.h style.h
banner.o: net_io.h
canvas_gnomecanvas.o: appdata.h version.h pos.h osm.h canvas.h map.h map_hl.h
canvas_gnomecanvas.o: osm_api.h project.h statusbar.h area_edit.h settings.h
canvas_gnomecanvas.o: diff.h iconbar.h icon.h info.h track.h gps.h wms.h
canvas_gnomecanvas.o: josm_presets.h relation_edit.h misc.h map_edit.h
canvas_gnomecanvas.o: josm_elemstyles.h style.h net_io.h
canvas_goocanvas.o: appdata.h version.h pos.h osm.h canvas.h map.h map_hl.h
canvas_goocanvas.o: osm_api.h project.h statusbar.h area_edit.h settings.h
canvas_goocanvas.o: diff.h iconbar.h icon.h info.h track.h gps.h wms.h
canvas_goocanvas.o: josm_presets.h relation_edit.h misc.h map_edit.h
canvas_goocanvas.o: josm_elemstyles.h style.h net_io.h
dbus.o: appdata.h version.h pos.h osm.h canvas.h map.h map_hl.h osm_api.h
dbus.o: project.h statusbar.h area_edit.h settings.h diff.h iconbar.h icon.h
dbus.o: info.h track.h gps.h wms.h josm_presets.h relation_edit.h misc.h
dbus.o: map_edit.h josm_elemstyles.h style.h net_io.h
diff.o: appdata.h version.h pos.h osm.h canvas.h map.h map_hl.h osm_api.h
diff.o: project.h statusbar.h area_edit.h settings.h diff.h iconbar.h icon.h
diff.o: info.h track.h gps.h wms.h josm_presets.h relation_edit.h misc.h
diff.o: map_edit.h josm_elemstyles.h style.h net_io.h
gps.o: appdata.h version.h pos.h osm.h canvas.h map.h map_hl.h osm_api.h
gps.o: project.h statusbar.h area_edit.h settings.h diff.h iconbar.h icon.h
gps.o: info.h track.h gps.h wms.h josm_presets.h relation_edit.h misc.h
gps.o: map_edit.h josm_elemstyles.h style.h net_io.h
icon.o: appdata.h version.h pos.h osm.h canvas.h map.h map_hl.h osm_api.h
icon.o: project.h statusbar.h area_edit.h settings.h diff.h iconbar.h icon.h
icon.o: info.h track.h gps.h wms.h josm_presets.h relation_edit.h misc.h
icon.o: map_edit.h josm_elemstyles.h style.h net_io.h
iconbar.o: appdata.h version.h pos.h osm.h canvas.h map.h map_hl.h osm_api.h
iconbar.o: project.h statusbar.h area_edit.h settings.h diff.h iconbar.h
iconbar.o: icon.h info.h track.h gps.h wms.h josm_presets.h relation_edit.h
iconbar.o: misc.h map_edit.h josm_elemstyles.h style.h net_io.h
info.o: appdata.h version.h pos.h osm.h canvas.h map.h map_hl.h osm_api.h
info.o: project.h statusbar.h area_edit.h settings.h diff.h iconbar.h icon.h
info.o: info.h track.h gps.h wms.h josm_presets.h relation_edit.h misc.h
info.o: map_edit.h josm_elemstyles.h style.h net_io.h
josm_elemstyles.o: appdata.h version.h pos.h osm.h canvas.h map.h map_hl.h
josm_elemstyles.o: osm_api.h project.h statusbar.h area_edit.h settings.h
josm_elemstyles.o: diff.h iconbar.h icon.h info.h track.h gps.h wms.h
josm_elemstyles.o: josm_presets.h relation_edit.h misc.h map_edit.h
josm_elemstyles.o: josm_elemstyles.h style.h net_io.h
josm_presets.o: appdata.h version.h pos.h osm.h canvas.h map.h map_hl.h
josm_presets.o: osm_api.h project.h statusbar.h area_edit.h settings.h diff.h
josm_presets.o: iconbar.h icon.h info.h track.h gps.h wms.h josm_presets.h
josm_presets.o: relation_edit.h misc.h map_edit.h josm_elemstyles.h style.h
josm_presets.o: net_io.h
main.o: appdata.h version.h pos.h osm.h canvas.h map.h map_hl.h osm_api.h
main.o: project.h statusbar.h area_edit.h settings.h diff.h iconbar.h icon.h
main.o: info.h track.h gps.h wms.h josm_presets.h relation_edit.h misc.h
main.o: map_edit.h josm_elemstyles.h style.h net_io.h banner.h
map.o: appdata.h version.h pos.h osm.h canvas.h map.h map_hl.h osm_api.h
map.o: project.h statusbar.h area_edit.h settings.h diff.h iconbar.h icon.h
map.o: info.h track.h gps.h wms.h josm_presets.h relation_edit.h misc.h
map.o: map_edit.h josm_elemstyles.h style.h net_io.h
map_edit.o: appdata.h version.h pos.h osm.h canvas.h map.h map_hl.h osm_api.h
map_edit.o: project.h statusbar.h area_edit.h settings.h diff.h iconbar.h
map_edit.o: icon.h info.h track.h gps.h wms.h josm_presets.h relation_edit.h
map_edit.o: misc.h map_edit.h josm_elemstyles.h style.h net_io.h
map_hl.o: appdata.h version.h pos.h osm.h canvas.h map.h map_hl.h osm_api.h
map_hl.o: project.h statusbar.h area_edit.h settings.h diff.h iconbar.h
map_hl.o: icon.h info.h track.h gps.h wms.h josm_presets.h relation_edit.h
map_hl.o: misc.h map_edit.h josm_elemstyles.h style.h net_io.h
misc.o: appdata.h version.h pos.h osm.h canvas.h map.h map_hl.h osm_api.h
misc.o: project.h statusbar.h area_edit.h settings.h diff.h iconbar.h icon.h
misc.o: info.h track.h gps.h wms.h josm_presets.h relation_edit.h misc.h
misc.o: map_edit.h josm_elemstyles.h style.h net_io.h
net_io.o: appdata.h version.h pos.h osm.h canvas.h map.h map_hl.h osm_api.h
net_io.o: project.h statusbar.h area_edit.h settings.h diff.h iconbar.h
net_io.o: icon.h info.h track.h gps.h wms.h josm_presets.h relation_edit.h
net_io.o: misc.h map_edit.h josm_elemstyles.h style.h net_io.h
osm.o: appdata.h version.h pos.h osm.h canvas.h map.h map_hl.h osm_api.h
osm.o: project.h statusbar.h area_edit.h settings.h diff.h iconbar.h icon.h
osm.o: info.h track.h gps.h wms.h josm_presets.h relation_edit.h misc.h
osm.o: map_edit.h josm_elemstyles.h style.h net_io.h banner.h
osm_api.o: appdata.h version.h pos.h osm.h canvas.h map.h map_hl.h osm_api.h
osm_api.o: project.h statusbar.h area_edit.h settings.h diff.h iconbar.h
osm_api.o: icon.h info.h track.h gps.h wms.h josm_presets.h relation_edit.h
osm_api.o: misc.h map_edit.h josm_elemstyles.h style.h net_io.h
pos.o: appdata.h version.h pos.h osm.h canvas.h map.h map_hl.h osm_api.h
pos.o: project.h statusbar.h area_edit.h settings.h diff.h iconbar.h icon.h
pos.o: info.h track.h gps.h wms.h josm_presets.h relation_edit.h misc.h
pos.o: map_edit.h josm_elemstyles.h style.h net_io.h
project.o: appdata.h version.h pos.h osm.h canvas.h map.h map_hl.h osm_api.h
project.o: project.h statusbar.h area_edit.h settings.h diff.h iconbar.h
project.o: icon.h info.h track.h gps.h wms.h josm_presets.h relation_edit.h
project.o: misc.h map_edit.h josm_elemstyles.h style.h net_io.h banner.h
relation_edit.o: appdata.h version.h pos.h osm.h canvas.h map.h map_hl.h
relation_edit.o: osm_api.h project.h statusbar.h area_edit.h settings.h
relation_edit.o: diff.h iconbar.h icon.h info.h track.h gps.h wms.h
relation_edit.o: josm_presets.h relation_edit.h misc.h map_edit.h
relation_edit.o: josm_elemstyles.h style.h net_io.h
settings.o: appdata.h version.h pos.h osm.h canvas.h map.h map_hl.h osm_api.h
settings.o: project.h statusbar.h area_edit.h settings.h diff.h iconbar.h
settings.o: icon.h info.h track.h gps.h wms.h josm_presets.h relation_edit.h
settings.o: misc.h map_edit.h josm_elemstyles.h style.h net_io.h
statusbar.o: appdata.h version.h pos.h osm.h canvas.h map.h map_hl.h
statusbar.o: osm_api.h project.h statusbar.h area_edit.h settings.h diff.h
statusbar.o: iconbar.h icon.h info.h track.h gps.h wms.h josm_presets.h
statusbar.o: relation_edit.h misc.h map_edit.h josm_elemstyles.h style.h
statusbar.o: net_io.h
style.o: appdata.h version.h pos.h osm.h canvas.h map.h map_hl.h osm_api.h
style.o: project.h statusbar.h area_edit.h settings.h diff.h iconbar.h icon.h
style.o: info.h track.h gps.h wms.h josm_presets.h relation_edit.h misc.h
style.o: map_edit.h josm_elemstyles.h style.h net_io.h
track.o: appdata.h version.h pos.h osm.h canvas.h map.h map_hl.h osm_api.h
track.o: project.h statusbar.h area_edit.h settings.h diff.h iconbar.h icon.h
track.o: info.h track.h gps.h wms.h josm_presets.h relation_edit.h misc.h
track.o: map_edit.h josm_elemstyles.h style.h net_io.h
wms.o: appdata.h version.h pos.h osm.h canvas.h map.h map_hl.h osm_api.h
wms.o: project.h statusbar.h area_edit.h settings.h diff.h iconbar.h icon.h
wms.o: info.h track.h gps.h wms.h josm_presets.h relation_edit.h misc.h
wms.o: map_edit.h josm_elemstyles.h style.h net_io.h
