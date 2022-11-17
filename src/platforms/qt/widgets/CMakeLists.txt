# must match the version of the libraries found in CMAKE_SOURCE_DIR
find_package(Qt5 ${Qt5_VERSION} COMPONENTS Widgets QuickWidgets REQUIRED)

target_sources(osm2go_lib PRIVATE
	canvas_graphicsscene.cpp
	canvas_graphicsscene.h
	iconbar_toolbar.cpp
	info.cpp
	info_p.h
	josm_presets_button.cpp
	map_graphicsview.cpp
	map_graphicsview.h
	ListEditDialog.cpp
	ListEditDialog.h
	MainUiQt.h
	notifications.cpp
	osm_upload_dialog.cpp
	project_widgets.cpp
	project_widgets_p.h
	ProjectPropertiesDialog.cpp
	ProjectPropertiesDialog.h
	ProjectPropertiesDialog.ui
	relation_edit.cpp
	RelationMemberRoleDelegate.cpp
	RelationMemberRoleDelegate.h
	style_widgets.cpp
	style_widgets.h
	uicontrol.cpp
	wms_dialog.cpp
)

target_include_directories(osm2go_lib
		PRIVATE
			${CMAKE_CURRENT_SOURCE_DIR}
			${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(osm2go_platform_lib INTERFACE
	Qt5::QuickWidgets
	Qt5::Widgets
)

add_executable(osm2go
	${CMAKE_CURRENT_BINARY_DIR}/../qml.qrc
	main.cpp
)

target_link_libraries(osm2go
	osm2go_lib
)

install(TARGETS osm2go DESTINATION ${CMAKE_INSTALL_BINDIR})
