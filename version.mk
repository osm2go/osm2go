
# Set the version number pertaining to the trunk here.

TRUNK_VERSION=0.6.14

# Packagers can append or prepend whatever magic strings through 
# environment vars.

VERSION=$${VERSION_PREFIX}$(TRUNK_VERSION)$${VERSION_SUFFIX}

version.h: FORCE
	grep -q "\"$(VERSION)\"" $@ || echo "#define VERSION \"$(VERSION)\"" >$@

FORCE:

