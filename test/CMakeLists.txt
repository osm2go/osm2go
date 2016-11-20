add_executable(osm_way_length osm_way_length.c)
target_link_libraries(osm_way_length
	osm2go_lib
	${CURL_LIBRARIES}
	${LIBXML2_LIBRARIES}
	PkgConfig::GooCanvas
	${MORE_LIBS}
)

add_test(NAME osm_way_length COMMAND osm_way_length)

add_executable(track_load_save track_load_save.c)
target_link_libraries(track_load_save
	osm2go_lib
	${CURL_LIBRARIES}
	${LIBXML2_LIBRARIES}
	PkgConfig::GooCanvas
	${MORE_LIBS}
)

add_test(NAME track_load_save COMMAND track_load_save "${CMAKE_CURRENT_SOURCE_DIR}/" "test1" "${CMAKE_CURRENT_BINARY_DIR}/export.gpx")
