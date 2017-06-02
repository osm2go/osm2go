function(osm_test BASENAME)
	add_executable(${BASENAME} ${BASENAME}.cpp)
	target_link_libraries(${BASENAME}
		osm2go_lib
	)

	add_test(NAME ${BASENAME}
			COMMAND ${BASENAME} ${ARGN})
endfunction()

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/test1.trk.in" "${CMAKE_CURRENT_BINARY_DIR}/test1.trk" @ONLY)

osm_test(track_load_save "${CMAKE_CURRENT_BINARY_DIR}/" "test1" "${CMAKE_CURRENT_BINARY_DIR}/export.gpx")
osm_test(osm_edit)
osm_test(presets_classes)
osm_test(presets_load "${CMAKE_CURRENT_BINARY_DIR}/../data" "${CMAKE_CURRENT_SOURCE_DIR}/../data")
set_property(TEST presets_load PROPERTY WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../data)
osm_test(track_load_multi "${CMAKE_CURRENT_SOURCE_DIR}/multi.gpx")
osm_test(diff_restore "${CMAKE_CURRENT_SOURCE_DIR}/" "diff_restore")

osm_test(style_load "elemstyles.xml" 355 370 "standard")
set_property(TEST style_load PROPERTY WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME style_load_mapnik
		COMMAND style_load "elemstyles-mapnik.xml" 140 146 "mapnik"
		WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME style_load_potlatch
		COMMAND style_load "${CMAKE_CURRENT_SOURCE_DIR}/../data/elemstyles-potlatch.xml" 27 27)
add_test(NAME style_load_test1
		COMMAND style_load "${CMAKE_CURRENT_SOURCE_DIR}/test1.xml" 5 5 "standard"
		WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(osm_load osm_load.cpp)
target_link_libraries(osm_load osm2go_lib)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/test1.style.in" "${CMAKE_CURRENT_BINARY_DIR}/test1.style" @ONLY)
osm_test(style_apply "${CMAKE_CURRENT_BINARY_DIR}/test1")
set_property(TEST style_apply PROPERTY WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
