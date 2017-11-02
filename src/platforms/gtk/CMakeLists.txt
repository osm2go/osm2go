target_sources(osm2go_lib PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/about.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/list.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/osm2go_platform.h
	${CMAKE_CURRENT_SOURCE_DIR}/settings.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/style_widgets.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/style_widgets.h
)

if (MAEMO_FOUND)
	add_subdirectory(fremantle)
else ()
	add_subdirectory(desktop)
endif ()
