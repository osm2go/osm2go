find_package(Perl REQUIRED)

add_custom_command(OUTPUT elemstyles-mapnik.xml
		COMMAND "${PERL_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/compose-elemstyles.pl" "0.4" "#f2eee8" "${CMAKE_CURRENT_BINARY_DIR}/elemstyles-mapnik.xml" "${CMAKE_CURRENT_SOURCE_DIR}/elemstyles-mapnik.xml.in"
		DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/compose-elemstyles.pl
			${CMAKE_CURRENT_SOURCE_DIR}/elemstyles-mapnik.xml.in
		VERBATIM
)

add_custom_target(elemstyles
		DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/elemstyles-mapnik.xml)

if (TARGET osm2go) # TODO: remove once Qt has a main executable
	add_dependencies(osm2go elemstyles)
endif ()
if (BUILD_TESTING)
	add_dependencies(style_load elemstyles)
endif ()

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/elemstyles-mapnik.xml
	DESTINATION ${DATA_DIR})

if (MAEMO_FOUND)
	# on maemo datadir may not point to the place where the system is
	# expecting the icons. thus we hardcode the /share path
	install(FILES ${PROJECT_NAME}.desktop
		DESTINATION share/applications/hildon)
	foreach(s 40 26)
		install(FILES ${PROJECT_NAME}.${s}.png
			DESTINATION share/icons/hicolor/${s}x${s}/hildon
			RENAME ${PROJECT_NAME}.png)
	endforeach()
	# .png file in scalable? Maemo is strange...
	install(FILES ${PROJECT_NAME}.64.png
		DESTINATION share/icons/hicolor/scalable/hildon
		RENAME ${PROJECT_NAME}.png)
	install(FILES ${PROJECT_NAME}.conf
		DESTINATION /etc/osso-backup/applications)
else ()
	install(FILES ${PROJECT_NAME}-ubuntu.desktop
		DESTINATION ${CMAKE_INSTALL_DATADIR}/applications/${PROJECT_NAME}.desktop)
	foreach(s 64 32 22 16)
		install(FILES ${PROJECT_NAME}.${s}.png
			DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${s}x${s}/apps
			RENAME ${PROJECT_NAME}.png)
	endforeach()
	install(FILES ${PROJECT_NAME}.svg
		DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
endif ()

file(GLOB INSTALL_DATA_FILES LIST_DIRECTORIES FALSE RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
	*.txt *.xml *.style)
list(REMOVE_ITEM INSTALL_DATA_FILES CMakeLists.txt)
if (NOT USE_SVG_ICONS)
	# must be modified, will be installed below
	list(REMOVE_ITEM INSTALL_DATA_FILES defaultpresets.xml)
endif ()
install(FILES ${INSTALL_DATA_FILES}
	DESTINATION ${DATA_DIR})

install(DIRECTORY icons
	DESTINATION ${DATA_DIR}
	FILES_MATCHING
	MESSAGE_NEVER
	PATTERN *.png)

if (USE_SVG_ICONS)
	install(DIRECTORY icons
		DESTINATION ${DATA_DIR}
		FILES_MATCHING
		MESSAGE_NEVER
		PATTERN *.svg)
else ()
	find_package(ImageMagick COMPONENTS convert REQUIRED)
	file(GLOB_RECURSE SVG_ICONS
		LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
		icons/*.svg)
	foreach (icon IN LISTS SVG_ICONS)
		string(REPLACE .svg .png png_icon "${icon}")
		if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${png_icon})
			continue()
		endif ()
		get_filename_component(icon_path "${icon}" DIRECTORY)
		file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${icon_path}")
		add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${png_icon}
				DEPENDS ${icon}
				COMMAND ${ImageMagick_convert_EXECUTABLE}
					"${CMAKE_CURRENT_SOURCE_DIR}/${icon}"
					"-resize" "'24x24>'"
					"${CMAKE_CURRENT_BINARY_DIR}/${png_icon}")
		install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${png_icon}"
			DESTINATION ${DATA_DIR}/${icon_path})
		list(APPEND all_pngs "${CMAKE_CURRENT_BINARY_DIR}/${png_icon}")
		set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${CMAKE_CURRENT_BINARY_DIR}/${png_icon}")
	endforeach ()
	add_custom_target(png_icons DEPENDS ${all_pngs})
	add_dependencies(osm2go png_icons)

	# replace the icon references in the presets file
	file(READ defaultpresets.xml PRESETS_XML)
	string(REPLACE ".svg\"" ".png\"" PRESETS_PNG "${PRESETS_XML}")
	file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/defaultpresets.xml" "${PRESETS_PNG}")
	install(FILES "${CMAKE_CURRENT_BINARY_DIR}/defaultpresets.xml"
		DESTINATION ${DATA_DIR})
endif ()

install(DIRECTORY demo
	DESTINATION ${DATA_DIR})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../LICENSES/GPL-3.0-or-later ${CMAKE_CURRENT_BINARY_DIR}/GPL-3.0-or-later COPYONLY)
