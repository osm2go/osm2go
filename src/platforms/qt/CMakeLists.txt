# must match the version of the libraries found in CMAKE_SOURCE_DIR
find_package(Qt5 ${Qt5_VERSION} COMPONENTS Gui Network Positioning Svg Widgets REQUIRED)

set(THREADS_PREFER_PTHREAD_FLAG On)
find_package(Threads REQUIRED)

set_property(TARGET osm2go_lib PROPERTY CXX_STANDARD 17)
set_property(TARGET osm2go_lib PROPERTY CXX_STANDARD_REQUIRED TRUE)

add_library(osm2go_platform_lib INTERFACE)

target_include_directories(osm2go_lib
		PUBLIC
			${CMAKE_CURRENT_SOURCE_DIR})

set_property(TARGET osm2go_lib PROPERTY AUTOMOC TRUE)
set_property(TARGET osm2go_lib PROPERTY AUTOUIC TRUE)

target_sources(osm2go_lib PRIVATE
	canvas_graphicsscene.cpp
	canvas_graphicsscene.h
	gps.cpp
	gps_p.h
	icon.cpp
	iconbar.cpp
	info.cpp
	info_p.h
	josm_presets_button.cpp
	map_graphicsview.cpp
	map_graphicsview.h
	net_io.cpp
	notifications.cpp
	osm2go_i18n.h
	osm2go_platform.h
	osm2go_platform_qt.h
	osm_upload_dialog.cpp
	platform.cpp
	project_widgets.cpp
	relation_edit.cpp
	RelationMemberModel.cpp
	RelationMemberModel.h
	RelationMembershipModel.cpp
	RelationMembershipModel.h
	settings.cpp
	TagModel.cpp
	TagModel.h
	uicontrol.cpp
	wms_dialog.cpp
)

target_link_libraries(osm2go_platform_lib INTERFACE
	Qt5::Core
	Qt5::Gui
	Qt5::Network
	Qt5::Positioning
	Qt5::Svg
	Qt5::Widgets
	Threads::Threads
)

target_link_libraries(osm2go_lib PUBLIC osm2go_platform_lib)

set(USE_SVG_ICONS_DEFAULT On PARENT_SCOPE)

if (BUILD_TESTING)
	find_package(Qt5 ${Qt5_VERSION} COMPONENTS Test REQUIRED)
	add_subdirectory(test)
endif ()
