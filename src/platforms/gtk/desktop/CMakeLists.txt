target_sources(osm2go_lib PRIVATE
	gps.cpp
	platform.cpp
	statusbar.cpp
)

target_include_directories(osm2go_lib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
