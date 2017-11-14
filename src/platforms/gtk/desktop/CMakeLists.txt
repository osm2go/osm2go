target_sources(osm2go_lib PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/platform.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/statusbar.cpp
)

target_include_directories(osm2go_lib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
