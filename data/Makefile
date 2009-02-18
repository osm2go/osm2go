TEMPLATE_ELEMSTYLES=elemstyles-mapnik.xml
APP = osm2go
PREFIX = /usr/local

all: $(TEMPLATE_ELEMSTYLES)

elemstyles-mapnik.xml: elemstyles-mapnik.xml.in
	perl ./compose-elemstyles.pl 0.4 '#f2eee8' $+ > $@.tmp
	mv $@.tmp $@

clean:
	rm -f $(TEMPLATE_ELEMSTYLES)

install: $(TEMPLATE_ELEMSTYLES)
	install -d $(DESTDIR)$(PREFIX)/share/applications
	install $(APP)-ubuntu.desktop $(DESTDIR)$(PREFIX)/share/applications/$(APP).desktop
	install -d $(DESTDIR)$(PREFIX)/share/pixmaps
	install $(APP).64.png $(DESTDIR)$(PREFIX)/share/pixmaps/$(APP).png
	install -d $(DESTDIR)$(PREFIX)/share/$(APP)	
	install -m 644 *.txt $(DESTDIR)$(PREFIX)/share/$(APP)
	install -m 644 *.xml $(DESTDIR)$(PREFIX)/share/$(APP)
	install -m 644 *.style $(DESTDIR)$(PREFIX)/share/$(APP)
	for f in `find icons -name "*.png"`; do \
	   install -d $(DESTDIR)$(PREFIX)/share/$(APP)/`dirname $$f` ;\
	   install -m 644 $$f $(DESTDIR)$(PREFIX)/share/$(APP)/$$f ;\
	done;


distclean: clean
