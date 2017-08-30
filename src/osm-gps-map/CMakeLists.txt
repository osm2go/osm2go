add_library(osm-gps-map STATIC
	config.h
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

target_link_libraries(osm-gps-map
	PRIVATE
		PkgConfig::SOUP
		${MATH_LIBRARY}
)

if (MAEMO_FOUND)
	target_link_libraries(osm-gps-map
		PUBLIC PkgConfig::Hildon
	)
	target_compile_definitions(osm-gps-map PRIVATE USE_HILDON)
	if (MAEMO_VERSION VERSION_GREATER_EQUAL 5)
		target_compile_definitions(osm-gps-map PRIVATE FREMANTLE)
	endif ()
else ()
	target_link_libraries(osm-gps-map
		PUBLIC ${GTK2_TARGETS}
	)
endif ()

if (SOUP_VERSION VERSION_LESS 2.4)
	set_property(SOURCE osm-gps-map.c APPEND PROPERTY COMPILE_DEFINITIONS LIBSOUP22)
endif ()

set_property(SOURCE osm-gps-map.c APPEND PROPERTY COMPILE_DEFINITIONS VERSION="${PROJECT_VERSION}")

target_compile_definitions(osm-gps-map INTERFACE ENABLE_OSM_GPS_MAP)

target_include_directories(osm-gps-map INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
