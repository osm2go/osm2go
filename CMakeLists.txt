cmake_minimum_required(VERSION 3.8 FATAL_ERROR)

project(osm2go VERSION 0.9.6 LANGUAGES C CXX)

include(CheckFunctionExists)
include(CMakePushCheckState)
include(GNUInstallDirs)
include(WriteCompilerDetectionHeader)

find_package(CURL REQUIRED)
find_package(LibXml2 REQUIRED)
find_package(PkgConfig REQUIRED)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_STANDARD 11)

set(CMAKE_INCLUDE_CURRENT_DIR TRUE)

# The epilog defines O2G_NULLPTR as NULL instead of 0 on unsupported compilers
# (i.e. N900 gcc 4.2) to make the function sentinel detection working again and
# avoid all the misleading compiler warnings generated otherwise.
write_compiler_detection_header(
		FILE "${CMAKE_CURRENT_BINARY_DIR}/osm2go_cpp.h"
		PREFIX "O2G"
		COMPILERS GNU Clang
		FEATURES cxx_nullptr cxx_override
		ALLOW_UNKNOWN_COMPILER_VERSIONS ALLOW_UNKNOWN_COMPILERS
		EPILOG
"#if !(defined(O2G_COMPILER_CXX_NULLPTR) && O2G_COMPILER_CXX_NULLPTR)
#  undef O2G_NULLPTR
#  define O2G_NULLPTR NULL
#endif
"
)

set(osm2go_SOURCE_FILES
	src/about.cpp
	src/about.h
	src/appdata.h
	src/area_edit.cpp
	src/area_edit.h
	src/banner.c
	src/banner.h
	src/canvas.c
	src/canvas.h
	src/canvas_goocanvas.cpp
	src/diff.cpp
	src/diff.h
	src/gps.h
	src/icon.cpp
	src/icon.h
	src/iconbar.c
	src/iconbar.h
	src/info.cpp
	src/info.h
	src/josm_elemstyles.cpp
	src/josm_elemstyles.h
	src/josm_elemstyles_p.h
	src/josm_presets.cpp
	src/josm_presets.h
	src/josm_presets_p.h
	src/josm_presets_parser.cpp
	src/list.c
	src/list.h
	src/map.cpp
	src/map.h
	src/map_edit.cpp
	src/map_edit.h
	src/map_hl.cpp
	src/map_hl.h
	src/misc.cpp
	src/misc.h
	src/net_io.c
	src/net_io.h
	src/osm.cpp
	src/osm.h
	src/osm_api.cpp
	src/osm_api.h
	src/pos.cpp
	src/pos.h
	src/project.cpp
	src/project.h
	src/relation_edit.cpp
	src/relation_edit.h
	src/settings.c
	src/settings.h
	src/statusbar.cpp
	src/statusbar.h
	src/style.cpp
	src/style.h
	src/track.cpp
	src/track.h
	src/undo.h
	src/wms.cpp
	src/wms.h
	${CMAKE_CURRENT_BINARY_DIR}/osm2go_cpp.h
)

pkg_search_module(SOUP IMPORTED_TARGET libsoup-2.4 libsoup-2.2)

if (SOUP_FOUND)
	add_definitions(-DENABLE_OSM_GPS_MAP)
	list(APPEND osm2go_SOURCE_FILES
		src/converter.c
		src/converter.h
		src/osm-gps-map.c
		src/osm-gps-map.h
		src/osm-gps-map-point.h
		src/osm-gps-map-source.h
		src/osm-gps-map-types.h
		src/osm-gps-map-widget.h
		src/osm-gps-map-osd-select.c
		src/osm-gps-map-osd-select.h
	)
	list(APPEND MORE_LIBS PRIVATE PkgConfig::SOUP)
	if (SOUP_VERSION VERSION_LESS 2.4)
		set_property(SOURCE src/osm-gps-map.c APPEND PROPERTY COMPILE_DEFINITIONS LIBSOUP22)
	endif ()
else ()
	message(WARNING "libsoup-2.4/2.2 missing, disabling internal map")
endif ()

set(DATA_DIR "${CMAKE_INSTALL_FULL_DATADIR}/${PROJECT_NAME}")

option(PICKER_MENU "use a picker table for tagging preset selection" OFF)

pkg_search_module(GooCanvas REQUIRED IMPORTED_TARGET goocanvas)

set(FREMANTLE Off)
pkg_check_modules(MAEMO maemo-version)
if (MAEMO_FOUND)
	string(REGEX REPLACE "^([0-9]+)\\..*" "\\1" MAEMO_VERSION_MAJOR "${MAEMO_VERSION}")

	pkg_check_modules(Hildon REQUIRED IMPORTED_TARGET hildon-1)
	pkg_check_modules(Osso REQUIRED IMPORTED_TARGET libosso)
	pkg_check_modules(Hildon_FM2 REQUIRED IMPORTED_TARGET hildon-fm-2)

	list(APPEND MORE_LIBS PUBLIC
			PkgConfig::Hildon
			PkgConfig::Osso
			PkgConfig::Hildon_FM2
	)

	# prefer gpsbt over liblocation as we got complaints regarding liblocation stability
	pkg_check_modules(GPSBT IMPORTED_TARGET gpsbt)
	if (GPSBT_FOUND)
		list(APPEND MODE_LIBS PkgConfig::GPSBT)
		add_definitions(-DENABLE_GPSBT)
	else ()
		pkg_check_modules(libLocation IMPORTED_TARGET liblocation)
		if (libLocation_FOUND)
			list(APPEND MORE_LIBS PRIVATE PkgConfig::libLocation)
		else ()
			message(WARNING "No liblocation nor gpsbt found, disabling gps auto start!")
		endif ()
	endif ()

	pkg_check_modules(TabletBrowserInterface tablet-browser-interface)
	if (TabletBrowserInterface_FOUND)
		add_definitions(-DENABLE_BROWSER_INTERFACE)
	endif ()

	# maemo uses dbus for ipc with maemo mapper
	list(APPEND osm2go_SOURCE_FILES
		src/dbus.c
		src/dbus.h
	)

	add_definitions(-DMAEMO_VERSION_MAJOR=${MAEMO_VERSION_MAJOR} -DUSE_HILDON)

	# fremantle uses icons of 48x48 pixels for the app installer,
	# older versions used 26x26
	if (MAEMO_VERSION VERSION_LESS 5)
		set(ICONNAME osm2go.26.png)
	else ()
		set(ICONNAME osm2go.48.png)
		# the data root dir is /opt on fremantle
		set(DATA_DIR "/opt/${PROJECT_NAME}")
		add_definitions(-DFREMANTLE_PANNABLE_AREA)
		set(PICKER_MENU On)
		set(FREMANTLE On)
	endif ()

	message(STATUS "Using application installer icon '${ICONNAME}'")

	execute_process(COMMAND uuencode -m "${CMAKE_CURRENT_SOURCE_DIR}/data/${ICONNAME}" "${ICONNAME}"
			COMMAND sed "1d;$d;s/^/ /"
			OUTPUT_VARIABLE ICON
			OUTPUT_STRIP_TRAILING_WHITESPACE)
	set(ICON " Maemo icon ${ICONNAME}\nXB-Maemo-Icon-26:\n${ICON}")

	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/debian/control.in ${CMAKE_CURRENT_BINARY_DIR}/debian/control @ONLY)

	# SVG icons do not work on my N900
	set(USE_SVG_ICONS_DEFAULT Off)
else ()
	find_package(GTK2 REQUIRED)

	add_definitions(-DENABLE_BROWSER_INTERFACE -DMAEMO_VERSION_MAJOR)

	list(APPEND MORE_LIBS PUBLIC ${GTK2_TARGETS})

	set(USE_SVG_ICONS_DEFAULT On)
endif ()

if (FREMANTLE)
	add_definitions(-DFREMANTLE)
else ()
	list(APPEND osm2go_SOURCE_FILES
		src/scale_popup.c
		src/scale_popup.h
	)
endif ()

find_library(MATH_LIBRARY m)

add_definitions(-DUSE_GOOCANVAS)
add_definitions("-DPACKAGE=\"${PROJECT_NAME}\"" "-DPREFIX=\"${CMAKE_INSTALL_PREFIX}\"" "-DVERSION=\"${PROJECT_VERSION}\"")

if (libLocation_FOUND)
	list(APPEND osm2go_SOURCE_FILES src/gps_liblocation.cpp)
else ()
	pkg_check_modules(GVFS REQUIRED IMPORTED_TARGET gnome-vfs-module-2.0)
	list(APPEND osm2go_SOURCE_FILES src/gps.cpp)
	list(APPEND MORE_LIBS PRIVATE PkgConfig::GVFS)
endif ()

cmake_push_check_state(RESET)
set(CMAKE_REQUIRED_LIBRARIES ${MATH_LIBRARY})
set(CMAKE_REQUIRED_INCLUDES math.h)
check_function_exists(log2 HAVE_LOG2)
cmake_pop_check_state()

set_property(SOURCE src/misc.cpp APPEND PROPERTY COMPILE_DEFINITIONS "DATADIR=\"${DATA_DIR}\"")
if (HAVE_LOG2)
	set_property(SOURCE src/area_edit.c APPEND PROPERTY COMPILE_DEFINITIONS "LOG2=log2")
endif ()

# there shouldn't be a reason to change the server url
option(SERVER_EDITABLE "add widget to make API URL editable" OFF)
if (SERVER_EDITABLE)
	set_property(SOURCE src/project.cpp APPEND PROPERTY COMPILE_DEFINITIONS "SERVER_EDITABLE")
endif ()

if (PICKER_MENU)
	set_property(SOURCE src/josm_presets.cpp APPEND PROPERTY COMPILE_DEFINITIONS "PICKER_MENU")
endif ()

option(USE_SVG_ICONS "Use SVG icons without converting them to PNG at build time" ${USE_SVG_ICONS_DEFAULT})
if (USE_SVG_ICONS)
	set_property(SOURCE src/icon.cpp APPEND PROPERTY COMPILE_DEFINITIONS USE_SVG_ICONS)
endif ()

add_library(osm2go_lib STATIC
	${osm2go_SOURCE_FILES}
)

target_link_libraries(osm2go_lib
	${MORE_LIBS}
	PRIVATE
		${MATH_LIBRARY}
	PUBLIC
		${CURL_LIBRARIES}
		${LIBXML2_LIBRARIES}
		PkgConfig::GooCanvas
)

target_include_directories(osm2go_lib
	PUBLIC
		${CMAKE_CURRENT_BINARY_DIR}
		${CURL_INCLUDE_DIRS}
		${LIBXML2_INCLUDE_DIR}
		src
)

add_executable(osm2go
	src/main.cpp
)

target_link_libraries(osm2go
	osm2go_lib
)

if (MAEMO_FOUND)
	install(CODE
		"message(STATUS \"Optifying binary\")")
	install(PROGRAMS $<TARGET_FILE:osm2go>
		DESTINATION ${DATA_DIR}
		RENAME ${PROJECT_NAME}.bin)
	install(DIRECTORY
		DESTINATION ${CMAKE_INSTALL_BINDIR})
	install(CODE
		"execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink  ${DATA_DIR}/${PROJECT_NAME}.bin \$ENV{DESTDIR}${CMAKE_INSTALL_FULL_BINDIR}/${PROJECT_NAME})")
else ()
	install(CODE
		"message(STATUS \"Not Optifying binary\")")
	install(TARGETS osm2go DESTINATION ${CMAKE_INSTALL_BINDIR})
endif ()

add_subdirectory(data)

option(ENABLE_TESTS "enable testcases" OFF)

if (ENABLE_TESTS)
	enable_testing()
	add_subdirectory(test)
endif ()
