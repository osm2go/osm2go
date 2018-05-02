add_library(osm2go_platform_lib INTERFACE)
add_library(osm2go_platform_lib_private INTERFACE)

target_include_directories(osm2go_platform_lib
		INTERFACE
			${CMAKE_CURRENT_SOURCE_DIR})

target_sources(osm2go_lib PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/about.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/area_edit.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/icon.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/iconbar.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/josm_presets_button.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/info.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/list.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/list.h
	${CMAKE_CURRENT_SOURCE_DIR}/MainUiGtk.h
	${CMAKE_CURRENT_SOURCE_DIR}/map_widget.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/notifications.cpp
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

include(CheckIncludeFileCXX)

check_include_file_cxx(cstdint CSTDINT_HEADER)
if (NOT CSTDINT_HEADER)
	target_include_directories(osm2go_platform_lib
			INTERFACE
				${CMAKE_CURRENT_SOURCE_DIR}/tr1wrappers)
endif ()

pkg_search_module(GooCanvas REQUIRED IMPORTED_TARGET goocanvas)
target_sources(osm2go_lib PRIVATE
	src/platforms/gtk/canvas_goocanvas.cpp
)
target_link_libraries(osm2go_platform_lib_private INTERFACE PkgConfig::GooCanvas)

pkg_check_modules(GConf2 REQUIRED IMPORTED_TARGET gconf-2.0)
target_link_libraries(osm2go_platform_lib_private INTERFACE PkgConfig::GConf2)

if (MAEMO_FOUND)
	pkg_check_modules(Hildon REQUIRED IMPORTED_TARGET hildon-1)
	pkg_check_modules(Osso REQUIRED IMPORTED_TARGET libosso)

	target_link_libraries(osm2go_platform_lib INTERFACE
			PkgConfig::Hildon
			PkgConfig::Osso
	)

	# prefer gpsbt over liblocation as we got complaints regarding liblocation stability
	pkg_check_modules(GPSBT IMPORTED_TARGET gpsbt)
	if (GPSBT_FOUND)
		target_link_libraries(osm2go_platform_lib_private INTERFACE PkgConfig::GPSBT)
		target_compile_definitions(osm2go_platform_lib_private INTERFACE ENABLE_GPSBT)
	else ()
		pkg_check_modules(libLocation IMPORTED_TARGET liblocation)
		if (libLocation_FOUND)
			target_link_libraries(osm2go_platform_lib_private INTERFACE PkgConfig::libLocation)
		else ()
			message(WARNING "No liblocation nor gpsbt found, disabling gps auto start!")
		endif ()
	endif ()

	pkg_check_modules(TabletBrowserInterface REQUIRED tablet-browser-interface)
	add_subdirectory(fremantle)

	# the data root dir is /opt on fremantle
	set(DATA_DIR "/opt/${PROJECT_NAME}" PARENT_SCOPE)
	set(PICKER_MENU On PARENT_SCOPE)

	# SVG icons do not work on my N900
	set(USE_SVG_ICONS_DEFAULT Off PARENT_SCOPE)
else ()
	find_package(GTK2 2.24 REQUIRED)

	target_link_libraries(osm2go_platform_lib INTERFACE
			${GTK2_TARGETS}
	)

	add_subdirectory(desktop)

	set(USE_SVG_ICONS_DEFAULT On PARENT_SCOPE)
endif ()

if (NOT libLocation_FOUND)
	pkg_check_modules(GVFS REQUIRED IMPORTED_TARGET gnome-vfs-module-2.0)
	target_sources(osm2go_lib PRIVATE src/platforms/gtk/gps.cpp)
	target_link_libraries(osm2go_platform_lib_private INTERFACE PkgConfig::GVFS)
endif ()

pkg_search_module(SOUP REQUIRED IMPORTED_TARGET libsoup-2.4)

add_subdirectory(osm-gps-map)
target_link_libraries(osm2go_platform_lib_private INTERFACE osm-gps-map)

add_executable(osm2go
	main.cpp
)

target_link_libraries(osm2go PRIVATE
	osm2go_lib
)

target_compile_definitions(osm2go
		PRIVATE PREFIX="${CMAKE_INSTALL_PREFIX}"
)

# move the executable to a place where it will find the data paths when launched
set_property(TARGET osm2go PROPERTY RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

if (MAEMO_FOUND)
	pkg_check_modules(Hildon_FM2 REQUIRED IMPORTED_TARGET hildon-fm-2)
	target_link_libraries(osm2go PRIVATE
			PkgConfig::Hildon_FM2
	)

	install(CODE
		"message(STATUS \"Optifying binary\")")
	install(PROGRAMS $<TARGET_FILE:osm2go>
		DESTINATION ${DATA_DIR}
		RENAME ${PROJECT_NAME}.bin)
	install(DIRECTORY
		DESTINATION ${CMAKE_INSTALL_BINDIR})
	install(CODE
		"execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink  ${DATA_DIR}/${PROJECT_NAME}.bin \$ENV{DESTDIR}${CMAKE_INSTALL_FULL_BINDIR}/${PROJECT_NAME})")
else ()
	install(CODE
		"message(STATUS \"Not Optifying binary\")")
	install(TARGETS osm2go DESTINATION ${CMAKE_INSTALL_BINDIR})
endif ()
