add_library(osm-gps-map STATIC
	converter.c
	converter.h
	osm-gps-map.c
	osm-gps-map.h
	osm-gps-map-point.h
	osm-gps-map-source.h
	osm-gps-map-types.h
	osm-gps-map-widget.h
	osm-gps-map-osd-select.c
	osm-gps-map-osd-select.h
)

pkg_check_modules(SOUP REQUIRED IMPORTED_TARGET libsoup-2.4)

target_link_libraries(osm-gps-map
	PRIVATE
		PkgConfig::SOUP
		${MATH_LIBRARY}
)

if (MAEMO_FOUND)
	target_link_libraries(osm-gps-map
		PUBLIC PkgConfig::Hildon
	)
	target_compile_definitions(osm-gps-map PRIVATE FREMANTLE)
else ()
	target_link_libraries(osm-gps-map
		PUBLIC ${GTK2_TARGETS}
	)
endif ()

set_property(SOURCE osm-gps-map.c APPEND PROPERTY COMPILE_DEFINITIONS VERSION="${PROJECT_VERSION}")

target_include_directories(osm-gps-map INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
