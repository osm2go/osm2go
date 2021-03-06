if (NOT MAEMO_VERSION OR MAEMO_VERSION VERSION_LESS 5)
	message(FATAL_ERROR "Build for Maemo requires at least Fremantle")
endif ()

# maemo uses dbus for ipc with maemo mapper
target_sources(osm2go_lib PRIVATE
	dbus.c
	dbus.h
	platform.cpp
	statusbar.cpp
)

target_include_directories(osm2go_lib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(osm2go_lib PUBLIC FREMANTLE)

set(ICONNAME osm2go.48.png)

message(STATUS "Using application installer icon '${ICONNAME}'")

execute_process(COMMAND uuencode -m "${osm2go_SOURCE_DIR}/data/${ICONNAME}" "${ICONNAME}"
		COMMAND sed "1d;$d;s/^/ /"
		OUTPUT_VARIABLE ICON
		OUTPUT_STRIP_TRAILING_WHITESPACE)
set(ICON " Maemo icon ${ICONNAME}\nXB-Maemo-Icon-26:\n${ICON}")

configure_file(${osm2go_SOURCE_DIR}/debian/control.in ${osm2go_BINARY_DIR}/debian/control @ONLY)

if (libLocation_FOUND)
	target_sources(osm2go_lib PRIVATE gps_liblocation.cpp)
endif ()

install(DIRECTORY
	DESTINATION ${CMAKE_INSTALL_BINDIR})
install(CODE
	"execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink  ${DATA_DIR}/${PROJECT_NAME}.bin \$ENV{DESTDIR}${CMAKE_INSTALL_FULL_BINDIR}/${PROJECT_NAME})")
