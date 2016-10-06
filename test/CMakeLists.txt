function(osm_test BASENAME)
	if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${BASENAME}.cpp")
		add_executable(${BASENAME} ${BASENAME}.cpp)
	else ()
		add_executable(${BASENAME} ${BASENAME}.c)
	endif ()
	target_link_libraries(${BASENAME}
		osm2go_lib
		${CURL_LIBRARIES}
		${LIBXML2_LIBRARIES}
		PkgConfig::GooCanvas
		${MORE_LIBS}
	)

	add_test(NAME ${BASENAME}
			COMMAND ${BASENAME} ${ARGN})
endfunction()

osm_test(osm_way_length)
osm_test(track_load_save "${CMAKE_CURRENT_SOURCE_DIR}/" "test1" "${CMAKE_CURRENT_BINARY_DIR}/export.gpx")
osm_test(presets_load)
set_property(TEST presets_load PROPERTY WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../data)
osm_test(track_load_multi "${CMAKE_CURRENT_SOURCE_DIR}/multi.gpx")
