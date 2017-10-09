cmake_minimum_required(VERSION 3.8 FATAL_ERROR)

project(osm2go VERSION 0.9.10 LANGUAGES C CXX)

include(CheckFunctionExists)
include(CMakePushCheckState)
include(GNUInstallDirs)
include(WriteCompilerDetectionHeader)

find_package(CURL 7.32 REQUIRED)
find_package(OpenSSL 1 REQUIRED)
find_package(LibXml2 REQUIRED)
find_package(PkgConfig REQUIRED)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_STANDARD 11)

set(CMAKE_INCLUDE_CURRENT_DIR TRUE)

# The epilog defines O2G_NULLPTR as NULL instead of 0 on unsupported compilers
# (i.e. N900 gcc 4.2) to make the function sentinel detection working again and
# avoid all the misleading compiler warnings generated otherwise.
# The static_assert code of CMake does not work with the old gcc
# https://gitlab.kitware.com/cmake/cmake/issues/17019
write_compiler_detection_header(
		FILE "${CMAKE_CURRENT_BINARY_DIR}/osm2go_cpp.h"
		PREFIX "O2G"
		COMPILERS GNU Clang
		FEATURES cxx_nullptr cxx_override cxx_static_assert cxx_deleted_functions
		ALLOW_UNKNOWN_COMPILER_VERSIONS ALLOW_UNKNOWN_COMPILERS
		EPILOG
"#if !(defined(O2G_COMPILER_CXX_NULLPTR) && O2G_COMPILER_CXX_NULLPTR)
#  undef O2G_NULLPTR
#  define O2G_NULLPTR NULL
#endif
#if !(defined(O2G_COMPILER_CXX_STATIC_ASSERT) && O2G_COMPILER_CXX_STATIC_ASSERT)
#  define static_assert(X, MSG) G_STATIC_ASSERT(X)
#endif
"
)

add_library(osm2go_lib STATIC
	src/about.cpp
	src/about.h
	src/appdata.h
	src/area_edit.cpp
	src/area_edit.h
	src/banner.cpp
	src/banner.h
	src/canvas.cpp
	src/canvas.h
	src/diff.cpp
	src/diff.h
	src/fdguard.cpp
	src/fdguard.h
	src/gps.h
	src/icon.cpp
	src/icon.h
	src/iconbar.cpp
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
	src/list.cpp
	src/list.h
	src/map.cpp
	src/map.h
	src/map_edit.cpp
	src/map_edit.h
	src/map_hl.cpp
	src/map_hl.h
	src/misc.cpp
	src/misc.h
	src/net_io.cpp
	src/net_io.h
	src/osm.cpp
	src/osm.h
	src/osm_api.cpp
	src/osm_api.h
	src/osm2go_platform.h
	src/osm2go_stl.h
	src/pos.cpp
	src/pos.h
	src/project.cpp
	src/project.h
	src/relation_edit.cpp
	src/relation_edit.h
	src/settings.cpp
	src/settings.h
	src/statusbar.cpp
	src/statusbar.h
	src/style.cpp
	src/style.h
	src/track.cpp
	src/track.h
	src/wms.cpp
	src/wms.h
	${CMAKE_CURRENT_BINARY_DIR}/osm2go_cpp.h
)

set(DATA_DIR "${CMAKE_INSTALL_FULL_DATADIR}/${PROJECT_NAME}")

option(PICKER_MENU "use a picker table for tagging preset selection" OFF)

# required at the moment, but add proper checks to allow easier porting later
pkg_search_module(GooCanvas REQUIRED IMPORTED_TARGET goocanvas)
if (GooCanvas_FOUND)
	target_sources(osm2go_lib PRIVATE
		src/canvas_goocanvas.cpp
	)
	target_link_libraries(osm2go_lib PRIVATE PkgConfig::GooCanvas)
endif ()

pkg_check_modules(MAEMO maemo-version)
if (MAEMO_FOUND)
	if (MAEMO_VERSION VERSION_LESS 5)
		message(FATAL_ERROR "Build for Maemo requires at least Fremantle")
	endif ()

	pkg_check_modules(Hildon REQUIRED IMPORTED_TARGET hildon-1)
	pkg_check_modules(Osso REQUIRED IMPORTED_TARGET libosso)
	pkg_check_modules(Hildon_FM2 REQUIRED IMPORTED_TARGET hildon-fm-2)

	target_link_libraries(osm2go_lib PUBLIC
			PkgConfig::Hildon
			PkgConfig::Osso
			PkgConfig::Hildon_FM2
	)

	# prefer gpsbt over liblocation as we got complaints regarding liblocation stability
	pkg_check_modules(GPSBT IMPORTED_TARGET gpsbt)
	if (GPSBT_FOUND)
		target_link_libraries(osm2go_lib PRIVATE PkgConfig::GPSBT)
		target_compile_definitions(osm2go_lib PRIVATE ENABLE_GPSBT)
	else ()
		pkg_check_modules(libLocation IMPORTED_TARGET liblocation)
		if (libLocation_FOUND)
			target_link_libraries(osm2go_lib PRIVATE PkgConfig::libLocation)
		else ()
			message(WARNING "No liblocation nor gpsbt found, disabling gps auto start!")
		endif ()
	endif ()

	pkg_check_modules(TabletBrowserInterface REQUIRED tablet-browser-interface)

	# maemo uses dbus for ipc with maemo mapper
	target_sources(osm2go_lib PRIVATE
		src/dbus.c
		src/dbus.h
	)

	set(ICONNAME osm2go.48.png)
	# the data root dir is /opt on fremantle
	set(DATA_DIR "/opt/${PROJECT_NAME}")
	set(PICKER_MENU On)
	set(FREMANTLE On)
	target_compile_definitions(osm2go_lib PUBLIC FREMANTLE)

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

	target_link_libraries(osm2go_lib PUBLIC ${GTK2_TARGETS})

	set(USE_SVG_ICONS_DEFAULT On)
endif ()

find_library(MATH_LIBRARY m)

pkg_search_module(SOUP IMPORTED_TARGET libsoup-2.4)

if (SOUP_FOUND)
	add_subdirectory(src/osm-gps-map)
	target_link_libraries(osm2go_lib PRIVATE osm-gps-map)
	set_property(SOURCE src/area_edit.cpp APPEND PROPERTY COMPILE_DEFINITIONS "ENABLE_OSM_GPS_MAP")
else ()
	message(WARNING "libsoup-2.4/2.2 missing, disabling internal map")
endif ()

target_compile_definitions(osm2go_lib
		PUBLIC PACKAGE="${PROJECT_NAME}"
		PUBLIC VERSION="${PROJECT_VERSION}"
)

if (libLocation_FOUND)
	target_sources(osm2go_lib PRIVATE src/gps_liblocation.cpp)
else ()
	pkg_check_modules(GVFS REQUIRED IMPORTED_TARGET gnome-vfs-module-2.0)
	target_sources(osm2go_lib PRIVATE src/gps.cpp)
	target_link_libraries(osm2go_lib PRIVATE PkgConfig::GVFS)
endif ()

pkg_check_modules(GConf2 REQUIRED IMPORTED_TARGET gconf-2.0)
target_link_libraries(osm2go_lib PRIVATE PkgConfig::GConf2)

set_property(SOURCE src/misc.cpp APPEND PROPERTY COMPILE_DEFINITIONS "DATADIR=\"${DATA_DIR}\"")
set_property(SOURCE src/net_io.cpp src/osm_api.cpp APPEND PROPERTY COMPILE_DEFINITIONS "CURL_NO_OLDIES")

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

target_link_libraries(osm2go_lib
	PRIVATE
		${MATH_LIBRARY}
	PUBLIC
		${CURL_LIBRARIES}
		OpenSSL::SSL
		${LIBXML2_LIBRARIES}
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

target_compile_definitions(osm2go
		PRIVATE PREFIX="${CMAKE_INSTALL_PREFIX}"
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
