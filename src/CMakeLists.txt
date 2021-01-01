add_library(osm2go_lib STATIC
	appdata.h
	area_edit.h
	canvas.cpp
	canvas.h
	color.h
	diff.cpp
	diff.h
	fdguard.cpp
	fdguard.h
	gps_state.h
	icon.h
	iconbar.h
	josm_elemstyles.cpp
	josm_elemstyles.h
	josm_elemstyles_p.h
	josm_presets.cpp
	josm_presets.h
	josm_presets_p.h
	josm_presets_parser.cpp
	map.cpp
	map.h
	map_edit.cpp
	map_hl.cpp
	map_hl.h
	map_state.h
	misc.cpp
	misc.h
	notifications.h
	net_io.cpp
	net_io.h
	object_dialogs.h
	osm.cpp
	osm_names.cpp
	osm.h
	osm_p.h
	osm_api.cpp
	osm_api.h
	osm_api_p.h
	osm_objects.cpp
	osm_objects.h
	osm_parser.cpp
	osm2go_annotations.cpp
	osm2go_annotations.h
	osm2go_stl.h
	pos.cpp
	pos.h
	project.cpp
	project.h
	project_p.h
	settings.h
	style.cpp
	style.h
	style_p.h
	track.cpp
	track.h
	uicontrol.h
	wms.cpp
	wms.h
	wms_p.h
	${CMAKE_CURRENT_BINARY_DIR}/../osm2go_cpp.h
)

if (NOT BUILD_WITH_QT)
	add_subdirectory(platforms/gtk)
else ()
	add_subdirectory(platforms/qt)
endif ()

target_compile_definitions(osm2go_lib
		PUBLIC PACKAGE="${PROJECT_NAME}"
		PUBLIC VERSION="${PROJECT_VERSION}"
)

set_property(SOURCE platforms/gtk/net_io_curl.cpp
		platforms/gtk/osm_upload_dialog.cpp
		osm_api.cpp
		APPEND PROPERTY COMPILE_DEFINITIONS "CURL_NO_OLDIES")
set_property(SOURCE
		platforms/gtk/platform.cpp
		platforms/qt/platform.cpp
		APPEND PROPERTY COMPILE_DEFINITIONS "DATADIR=\"${DATA_DIR}\"")

if (SERVER_EDITABLE)
	set_property(SOURCE
			platforms/gtk/project_widgets.cpp
			APPEND PROPERTY COMPILE_DEFINITIONS "SERVER_EDITABLE")
endif ()

if (PICKER_MENU)
	set_property(SOURCE
			platforms/gtk/josm_presets_button.cpp
			APPEND PROPERTY COMPILE_DEFINITIONS "PICKER_MENU")
endif ()

if (USE_SVG_ICONS)
	set_property(SOURCE
			platforms/gtk/icon.cpp
			APPEND PROPERTY COMPILE_DEFINITIONS "USE_SVG_ICONS")
endif ()

target_link_libraries(osm2go_lib
	PRIVATE
		${MATH_LIBRARY}
		${CXX_FILESYSTEM_LIBS}
	PUBLIC
		${CURL_LIBRARIES}
		OpenSSL::SSL
		${LIBXML2_LIBRARIES}
)

# curl can't be added here as that would end up in the search list
# _after_ the default curl in scratchbox
target_include_directories(osm2go_lib
	SYSTEM PUBLIC
		${LIBXML2_INCLUDE_DIR}
)

target_include_directories(osm2go_lib
	PUBLIC
		${CURL_INCLUDE_DIRS}
		${CMAKE_CURRENT_SOURCE_DIR}
		${CMAKE_CURRENT_BINARY_DIR}/..
)

set(USE_SVG_ICONS_DEFAULT "${USE_SVG_ICONS_DEFAULT}" PARENT_SCOPE)
