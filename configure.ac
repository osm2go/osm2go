#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

# OSM2Go autoconf script
#
# Copyright (C) 2008 Till Harbaum <till@harbaum.org>
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

AC_PREREQ(2.61)
AC_INIT([OSM2Go],[0.8.7],[osm2go-users@garage.maemo.org])
AC_CONFIG_SRCDIR([src/main.c])
# AC_CONFIG_HEADER([src/config.h])

PACKAGE="osm2go"
AC_SUBST(PACKAGE)

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET

# Checks for libraries.

# Checks for header files.
#AC_HEADER_STDC
#AC_CHECK_HEADERS([libintl.h locale.h stdlib.h string.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
#AC_C_CONST
#AC_C_INLINE
#AC_TYPE_SIZE_T
#AC_HEADER_TIME
#AC_STRUCT_TM
#AC_HEADER_STDBOOL

# Checks for library functions.
#AC_FUNC_MALLOC
#AC_FUNC_MKTIME
#AC_FUNC_STRFTIME
#AC_FUNC_STRTOD
#AC_CHECK_FUNCS([gettimeofday localtime_r memset pow setlocale sqrt strcasecmp strchr strcspn strdup strerror strrchr strstr strtoul tzset])

# add libm, used for cos()
AC_CHECK_LIB([m], [cos])

LIBS_SAVE=$LIBS
LIBS="$LIBS $LIBM"
AC_CHECK_FUNCS([log2], [ CFLAGS="$CFLAGS -DLOG2=log2" ])
LIBS=$LIBS_SAVE

# check for libcurl
PKG_CHECK_MODULES([libcurl], [libcurl], [], [AC_MSG_ERROR([libcurl missing])])
CFLAGS="$CFLAGS $libcurl_CFLAGS"
LIBS="$LIBS $libcurl_LIBS"

# check for libsoup
PKG_CHECK_MODULES([libsoup24], [libsoup-2.4], [
  EXTRA_OBJS="$EXTRA_OBJS converter.o osm-gps-map.o osm-gps-map-osd-select.o"
  CFLAGS="$CFLAGS $libsoup24_CFLAGS -DENABLE_OSM_GPS_MAP"
  LIBS="$LIBS $libsoup24_LIBS"
], [
  PKG_CHECK_MODULES([libsoup22], [libsoup-2.2], [
    EXTRA_OBJS="$EXTRA_OBJS converter.o osm-gps-map.o osm-gps-map-osd-select.o"
    CFLAGS="$CFLAGS $libsoup22_CFLAGS -DLIBSOUP22 -DENABLE_OSM_GPS_MAP"
    LIBS="$LIBS $libsoup22_LIBS"
  ], [
    AC_MSG_NOTICE([WARNING: libsoup-2.4/2.2 missing, disabling internal map])
  ])
])

# check for goocanvas
PKG_CHECK_MODULES([goocanvas], [goocanvas], [], [AC_MSG_ERROR([goocanvas missing])])
CFLAGS="$CFLAGS -DUSE_GOOCANVAS $goocanvas_CFLAGS"
LIBS="$LIBS $goocanvas_LIBS"

# check for gnome-vfs-module-2.0
PKG_CHECK_MODULES([gnome_vfs_module], [gnome-vfs-module-2.0], [], [AC_MSG_ERROR([gnome-vfs-module-2.0 missing])])
CFLAGS="$CFLAGS $gnome_vfs_module_CFLAGS"
LIBS="$LIBS $gnome_vfs_module_LIBS"

# check for libxml-2.0
PKG_CHECK_MODULES([libxml2], [libxml-2.0], [], [AC_MSG_ERROR([libxml-2.0 missing])])
CFLAGS="$CFLAGS $libxml2_CFLAGS"
LIBS="$LIBS $libxml2_LIBS"

# check for maemo
AC_MSG_CHECKING([for Maemo])
PKG_CHECK_EXISTS(maemo-version, [
  MAEMO="yes"
  AC_MSG_RESULT(yes)

  VERSION=`pkg-config --modversion maemo-version`
  AC_MSG_NOTICE([Configuring for Maemo $VERSION])
  MAJOR=`echo $VERSION | cut -b1 -`
  CFLAGS="$CFLAGS -DMAEMO_VERSION_MAJOR=$MAJOR"
  MAEMO_VERSION_MAJOR=$MAJOR

  PKG_CHECK_MODULES([hildon], [hildon-1], [], [AC_MSG_ERROR([hildon-1 missing])])
  CFLAGS="$CFLAGS  -DUSE_HILDON $hildon_CFLAGS"
  LIBS="$LIBS $hildon_LIBS"

  PKG_CHECK_MODULES([osso], [libosso], [], [AC_MSG_ERROR([libosso missing])])
  CFLAGS="$CFLAGS $osso_CFLAGS"
  LIBS="$LIBS $osso_LIBS"

  PKG_CHECK_MODULES([hildon_fm_2], [hildon-fm-2], [], [AC_MSG_ERROR([hildon-fm-2 missing])])
  CFLAGS="$CFLAGS $hildon_fm_2_CFLAGS"
  LIBS="$LIBS $hildon_fm_2_LIBS"

  # prefer gpsbt over liblocation as we got complaints regarding liblocation
  # stability
  PKG_CHECK_EXISTS(gpsbt, [
    PKG_CHECK_MODULES([gpsbt], [gpsbt], [], [])
    CFLAGS="$CFLAGS -DENABLE_GPSBT $gpsbt_CFLAGS"
    LIBS="$LIBS $gpsbt_LIBS"
    EXTRA_OBJS="$EXTRA_OBJS gps.o"
  ], [
    AC_MSG_NOTICE([gpsbt not found, trying liblocation])

    PKG_CHECK_EXISTS(liblocation, [ 
      PKG_CHECK_MODULES([liblocation], [liblocation], [], [])
      CFLAGS="$CFLAGS $liblocation_CFLAGS"
      LIBS="$LIBS $liblocation_LIBS"
      EXTRA_OBJS="$EXTRA_OBJS gps_liblocation.o"
    ], [
      AC_MSG_NOTICE([WARNING: No liblocation nor gpsbt found, disabling gps auto start!])
      EXTRA_OBJS="$EXTRA_OBJS gps.o"
    ])
  ]) 

  PKG_CHECK_EXISTS(tablet-browser-interface, [
    CFLAGS="$CFLAGS -DENABLE_BROWSER_INTERFACE"
  ], [AC_MSG_NOTICE([WARNING: No tablet-browser-interface found, disabling microb interface!])])

  # maemo uses dbus for ipc with maemo mapper
  EXTRA_OBJS="$EXTRA_OBJS dbus.o"

  INSTALL_PLATTFORM="install-maemo"
  
  # fremantle uses icons of 48x48 pixels for the app installer,
  # older versions used 26x26
  if [[ "$MAJOR" == "5" ]]; then
     ICONNAME=osm2go.48.png
     CFLAGS="$CFLAGS -DFREMANTLE -DFREMANTLE_PANNABLE_AREA"
  else
     ICONNAME=osm2go.26.png
  fi

  AC_MSG_NOTICE([Using application installer icon "$ICONNAME"])
  
  # add icon entry to debian/control
  ICON=" Maemo icon $ICONNAME
XB-Maemo-Icon-26:
"

  # generate base64 icon, remove first and last line 
  # and add a space before each line
  ICON="$ICON`uuencode -m $srcdir/data/$ICONNAME $ICONNAME | sed '1d;$d;s/^/ /' -`"

  # the data root dir is /opt on fremantle
  if [[ "$MAJOR" == "5" ]]; then
    datarootdir=/opt
  fi
], [
  AC_MSG_RESULT(no)

  # check for gtk+-2.0
  PKG_CHECK_MODULES([gtk], [gtk+-2.0], [], [AC_MSG_ERROR([gtk+-2.0 missing!])])
  CFLAGS="$CFLAGS $gtk_CFLAGS"
  LIBS="$LIBS $gtk_LIBS"	   

  CFLAGS="$CFLAGS -DENABLE_BROWSER_INTERFACE"

  EXTRA_OBJS="$EXTRA_OBJS gps.o"

  INSTALL_PLATTFORM="install-debian"

  ICON=" No icon"
  MAEMO_VERSION_MAJOR=""
])

AC_SUBST(EXTRA_OBJS)
AC_SUBST(INSTALL_PLATTFORM)
AC_SUBST(MAEMO_VERSION_MAJOR)
AC_SUBST(ICON)
AC_CONFIG_FILES([Makefile src/Makefile data/Makefile debian/control])

AC_OUTPUT
