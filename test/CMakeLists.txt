add_executable(osm_way_length osm_way_length.c)
target_link_libraries(osm_way_length
	osm2go_lib
	${CURL_LIBRARIES}
	${LIBXML2_LIBRARIES}
	PkgConfig::GooCanvas
	${MORE_LIBS}
)

add_test(NAME osm_way_length COMMAND osm_way_length)
