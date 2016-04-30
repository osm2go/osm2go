find_package(Perl REQUIRED)

add_custom_command(OUTPUT elemstyles-mapnik.xml
		COMMAND sh -c "\"${PERL_EXECUTABLE}\" \"${CMAKE_CURRENT_SOURCE_DIR}/compose-elemstyles.pl\" 0.4 '#f2eee8' \"${CMAKE_CURRENT_SOURCE_DIR}/elemstyles-mapnik.xml.in\" > \"${CMAKE_CURRENT_BINARY_DIR}/elemstyles-mapnik.xml\""
		DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/compose-elemstyles.pl
			${CMAKE_CURRENT_SOURCE_DIR}/elemstyles-mapnik.xml.in
		VERBATIM
)

add_custom_target(elemstyles
		DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/elemstyles-mapnik.xml)

add_dependencies(osm2go elemstyles)

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
	if (NOT MAEMO_VERSION VERSION_LESS 5)
		message(STATUS "Installing backup configuration for maemo >= 5")
		install(FILES ${PROJECT_NAME}.conf
			DESTINATION /etc/osso-backup/applications)
	endif ()
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
install(FILES COPYING ${INSTALL_DATA_FILES}
	DESTINATION ${DATA_DIR})

install(DIRECTORY icons
	DESTINATION ${DATA_DIR}
	FILES_MATCHING
	MESSAGE_NEVER
	PATTERN *.png)

install(DIRECTORY demo
	DESTINATION ${DATA_DIR})
