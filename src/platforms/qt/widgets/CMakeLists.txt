# must match the version of the libraries found in CMAKE_SOURCE_DIR
find_package(Qt5 ${Qt5_VERSION} COMPONENTS Widgets REQUIRED)

target_sources(osm2go_lib PRIVATE
	info.cpp
	info_p.h
	josm_presets_button.cpp
	notifications.cpp
	osm_upload_dialog.cpp
	relation_edit.cpp
	uicontrol.cpp
	wms_dialog.cpp
)

target_include_directories(osm2go_lib
		PRIVATE
			${CMAKE_CURRENT_SOURCE_DIR}
			${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(osm2go_platform_lib INTERFACE
	Qt5::Widgets
)
