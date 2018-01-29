target_sources(osm2go_lib PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/about.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/area_edit.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/icon.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/iconbar.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/josm_presets_button.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/info.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/list.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/osm2go_i18n.h
	${CMAKE_CURRENT_SOURCE_DIR}/osm2go_platform.h
	${CMAKE_CURRENT_SOURCE_DIR}/osm2go_platform_gtk.h
	${CMAKE_CURRENT_SOURCE_DIR}/osm_upload_dialog.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/platform.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/project_widgets.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/relation_edit.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/settings.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/style_widgets.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/style_widgets.h
	${CMAKE_CURRENT_SOURCE_DIR}/uicontrol.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/wms_dialog.cpp
)

if (MAEMO_FOUND)
	add_subdirectory(fremantle)
else ()
	add_subdirectory(desktop)
endif ()

include(CheckIncludeFileCXX)

check_include_file_cxx(cstdint CSTDINT_HEADER)
if (NOT CSTDINT_HEADER)
	target_include_directories(osm2go_lib
			PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/tr1wrappers)
endif ()
