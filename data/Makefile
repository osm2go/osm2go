TEMPLATE_ELEMSTYLES=elemstyles-mapnik.xml

all: $(TEMPLATE_ELEMSTYLES)

elemstyles-mapnik.xml: elemstyles-mapnik.xml.in
	perl ./compose-elemstyles.pl 0.4 '#f2eee8' $+ > $@.tmp
	mv $@.tmp $@

clean:
	rm -f $(TEMPLATE_ELEMSTYLES)

distclean: clean
