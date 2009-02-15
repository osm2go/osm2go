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

all:
	cd src && make
	cd data && make

install:
	cd src && make install
	cd data && make install

clean:
	rm -f *~ \#*\# *.bak *-stamp
	cd src && make clean
	cd data && make clean

distclean: clean

ChangeLog: FORCE
	svn2cl --group-by-day --include-rev --separate-daylogs --include-actions


-include version.mk

tarball: distclean
	sh mktarball.sh "$(VERSION_PREFIX)$(TRUNK_VERSION)$(VERSION_SUFFIX)"


FORCE:

