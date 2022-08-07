option(NETWORK_TESTS "Enable tests that require network access" Off)
add_feature_info(NetworkTests NETWORK_TESTS "additional testcases are run that access public internet servers")

function(osm_test BASENAME)
	add_executable(${BASENAME} ${BASENAME}.cpp)
	target_link_libraries(${BASENAME}
		osm2go_lib
	)

	add_test(NAME ${BASENAME}
			COMMAND ${BASENAME} ${ARGN})
	set_property(TEST ${BASENAME} APPEND PROPERTY ENVIRONMENT G_MESSAGES_DEBUG=all)
endfunction()

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/test1.trk.in" "${CMAKE_CURRENT_BINARY_DIR}/test1.trk" @ONLY)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/diff_restore_data/diff_restore_data.osmchange.in" "${CMAKE_CURRENT_BINARY_DIR}/diff_restore_data.osmchange" @ONLY)

osm_test(track_load_save "${CMAKE_CURRENT_BINARY_DIR}/" "test1" "${CMAKE_CURRENT_BINARY_DIR}/export.gpx")
osm_test(map_items)
osm_test(osm_edit)
osm_test(osm_names)
osm_test(presets_classes)
osm_test(presets_load "${CMAKE_CURRENT_BINARY_DIR}/../data" "${CMAKE_CURRENT_SOURCE_DIR}/../data")
set_property(TEST presets_load PROPERTY WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
# a path that is known to contain no additional presets
set_property(TEST presets_load APPEND PROPERTY ENVIRONMENT "XDG_DATA_HOME=${CMAKE_CURRENT_SOURCE_DIR}/../src/platforms")
osm_test(presets_items)
set_property(TEST presets_items APPEND PROPERTY ENVIRONMENT "XDG_DATA_HOME=${CMAKE_CURRENT_SOURCE_DIR}/presets")
set_property(TEST presets_items APPEND PROPERTY ENVIRONMENT "LC_MESSAGES=xy_ZZ")
osm_test(settings)
osm_test(track_load_multi "${CMAKE_CURRENT_SOURCE_DIR}/multi.gpx")
osm_test(diff_restore "${CMAKE_CURRENT_SOURCE_DIR}/" "diff_restore_data" "${CMAKE_CURRENT_BINARY_DIR}/diff_restore_data.osmchange")

osm_test(style_load "elemstyles.xml" 347 357 "standard")
set_property(TEST style_load PROPERTY WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME style_load_mapnik
		COMMAND style_load "elemstyles-mapnik.xml" 137 143 "mapnik"
		WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME style_load_potlatch
		COMMAND style_load "${CMAKE_CURRENT_SOURCE_DIR}/../data/elemstyles-potlatch.xml" 27 27)
add_test(NAME style_load_test1
		COMMAND style_load "${CMAKE_CURRENT_SOURCE_DIR}/test1.xml" 10 14 "standard"
		WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(osm_load osm_load.cpp)
target_link_libraries(osm_load osm2go_lib)

add_test(NAME osm_load
		COMMAND osm_load ${CMAKE_CURRENT_SOURCE_DIR}/diff_restore_data/diff_restore_data.osm)
set_property(TEST osm_load PROPERTY PASS_REGULAR_EXPRESSION "Nodes: 18, 5 with 19 tags\nWays: 7, 7 with 24 tags\nRelations: 9, 9 with 85 tags\n$")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/test1.style.in" "${CMAKE_CURRENT_BINARY_DIR}/test1.style" @ONLY)
osm_test(style_apply "${CMAKE_CURRENT_BINARY_DIR}/test1")
set_property(TEST style_apply PROPERTY WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

osm_test(style_apply_mapnik)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/../data/mapnik.style" "${CMAKE_CURRENT_BINARY_DIR}/../data/mapnik.style" COPYONLY)
set_property(TEST style_apply_mapnik PROPERTY WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

find_program(DPKG_PARSECHANGELOG NAMES dpkg-parsechangelog)
if (DPKG_PARSECHANGELOG)
	add_test(NAME parsechangelog COMMAND ${DPKG_PARSECHANGELOG}
			WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
	set_property(TEST parsechangelog PROPERTY FAIL_REGULAR_EXPRESSION ": warning:")
endif ()

if (NETWORK_TESTS)
	osm_test(api_limits "--online" "${CMAKE_CURRENT_SOURCE_DIR}")
else ()
	osm_test(api_limits "${CMAKE_CURRENT_SOURCE_DIR}")
endif ()
osm_test(canvas_base)
osm_test(canvas_points)
osm_test(fdguard $<TARGET_FILE:fdguard>)

add_executable(suppression-dummy suppression-dummy.cpp)
target_link_libraries(suppression-dummy PRIVATE ${LIBXML2_LIBRARIES} ${CURL_LIBRARIES})
target_include_directories(suppression-dummy PRIVATE ${LIBXML2_INCLUDE_DIR} ${CURL_INCLUDE_DIRS})

osm_test(i18n)
osm_test(project "${CMAKE_CURRENT_SOURCE_DIR}/empty_proj.proj"
		"${CMAKE_CURRENT_SOURCE_DIR}/diff_restore_data/diff_restore_data.osm"
		"${CMAKE_CURRENT_SOURCE_DIR}/diff_restore_data/diff_restore_data.diff")

if (NETWORK_TESTS)
	osm_test(net_io "${CMAKE_SOURCE_DIR}/LICENSES/GPL-3.0-or-later")
	osm_test(osm_api_download)
endif ()

if (TARGET osm2go) # TODO: remove once Qt has a main executable
	add_test(NAME application_help COMMAND osm2go --help)
	set_property(TEST application_help PROPERTY PASS_REGULAR_EXPRESSION "--projects[ \t]")
endif ()
