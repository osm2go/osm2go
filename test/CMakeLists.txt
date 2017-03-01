function(osm_test BASENAME)
	if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${BASENAME}.cpp")
		add_executable(${BASENAME} ${BASENAME}.cpp)
	else ()
		add_executable(${BASENAME} ${BASENAME}.c)
	endif ()
	target_link_libraries(${BASENAME}
		osm2go_lib
	)

	add_test(NAME ${BASENAME}
			COMMAND ${BASENAME} ${ARGN})
endfunction()

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/test1.trk.in" "${CMAKE_CURRENT_BINARY_DIR}/test1.trk" @ONLY)

osm_test(track_load_save "${CMAKE_CURRENT_BINARY_DIR}/" "test1" "${CMAKE_CURRENT_BINARY_DIR}/export.gpx")
osm_test(presets_load "${CMAKE_CURRENT_BINARY_DIR}/../data" "${CMAKE_CURRENT_SOURCE_DIR}/../data")
set_property(TEST presets_load PROPERTY WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../data)
osm_test(track_load_multi "${CMAKE_CURRENT_SOURCE_DIR}/multi.gpx")
osm_test(diff_restore "${CMAKE_CURRENT_SOURCE_DIR}/" "diff_restore")

osm_test(style_load "elemstyles.xml" 318 318)
set_property(TEST style_load PROPERTY WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME style_load_mapnik
		COMMAND style_load "elemstyles-mapnik.xml" 128 128
		WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME style_load_potlatch
		COMMAND style_load "elemstyles-potlatch.xml" 27 27
		WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
