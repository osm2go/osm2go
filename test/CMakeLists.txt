function(osm_test BASENAME)
	add_executable(${BASENAME} ${BASENAME}.cpp)
	target_link_libraries(${BASENAME}
		osm2go_lib
	)

	add_test(NAME ${BASENAME}
			COMMAND ${BASENAME} ${ARGN})
endfunction()

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/test1.trk.in" "${CMAKE_CURRENT_BINARY_DIR}/test1.trk" @ONLY)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/diff_restore/diff_restore.osmchange.in" "${CMAKE_CURRENT_BINARY_DIR}/diff_restore.osmchange" @ONLY)

osm_test(track_load_save "${CMAKE_CURRENT_BINARY_DIR}/" "test1" "${CMAKE_CURRENT_BINARY_DIR}/export.gpx")
osm_test(osm_edit)
osm_test(presets_classes)
osm_test(presets_load "${CMAKE_CURRENT_BINARY_DIR}/../data" "${CMAKE_CURRENT_SOURCE_DIR}/../data")
set_property(TEST presets_load PROPERTY WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../data)
osm_test(track_load_multi "${CMAKE_CURRENT_SOURCE_DIR}/multi.gpx")
osm_test(diff_restore "${CMAKE_CURRENT_SOURCE_DIR}/" "diff_restore" "${CMAKE_CURRENT_BINARY_DIR}/diff_restore.osmchange")

osm_test(style_load "elemstyles.xml" 347 357 "standard")
set_property(TEST style_load PROPERTY WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME style_load_mapnik
		COMMAND style_load "elemstyles-mapnik.xml" 134 137 "mapnik"
		WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME style_load_potlatch
		COMMAND style_load "${CMAKE_CURRENT_SOURCE_DIR}/../data/elemstyles-potlatch.xml" 27 27)
add_test(NAME style_load_test1
		COMMAND style_load "${CMAKE_CURRENT_SOURCE_DIR}/test1.xml" 7 8 "standard"
		WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(osm_load osm_load.cpp)
target_link_libraries(osm_load osm2go_lib)

add_test(NAME osm_load
		COMMAND osm_load ${CMAKE_CURRENT_SOURCE_DIR}/diff_restore/diff_restore.osm)
set_property(TEST osm_load PROPERTY PASS_REGULAR_EXPRESSION "Nodes: 10, 2 with 5 tags\nWays: 3, 3 with 9 tags\nRelations: 4, 4 with 21 tags\n$")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/test1.style.in" "${CMAKE_CURRENT_BINARY_DIR}/test1.style" @ONLY)
osm_test(style_apply "${CMAKE_CURRENT_BINARY_DIR}/test1")
set_property(TEST style_apply PROPERTY WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

find_program(DPKG_PARSECHANGELOG NAMES dpkg-parsechangelog)
if (DPKG_PARSECHANGELOG)
	add_test(NAME parsechangelog COMMAND ${DPKG_PARSECHANGELOG}
			WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
	set_property(TEST parsechangelog PROPERTY FAIL_REGULAR_EXPRESSION ": warning:")
endif ()

osm_test(fdguard)

add_executable(suppression-dummy suppression-dummy.cpp)
target_link_libraries(suppression-dummy PRIVATE ${LIBXML2_LIBRARIES} ${CURL_LIBRARIES})
target_include_directories(suppression-dummy PRIVATE ${LIBXML2_INCLUDE_DIR} ${CURL_INCLUDE_DIRS})
if (MAEMO_FOUND)
	target_link_libraries(suppression-dummy PRIVATE
			PkgConfig::Hildon
			PkgConfig::Osso
			PkgConfig::Hildon_FM2
	)
else ()
	target_link_libraries(suppression-dummy PRIVATE ${GTK2_TARGETS})
endif ()
