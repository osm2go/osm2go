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
	${CMAKE_CURRENT_SOURCE_DIR}/map_gtk.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/map_gtk.h
	${CMAKE_CURRENT_SOURCE_DIR}/net_io_curl.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/notifications.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/osm2go_i18n.h
	${CMAKE_CURRENT_SOURCE_DIR}/osm2go_platform.h
	${CMAKE_CURRENT_SOURCE_DIR}/osm2go_platform_gtk.h
	${CMAKE_CURRENT_SOURCE_DIR}/osm_upload_dialog.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/platform.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/project_widgets.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/relation_edit.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/settings.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/statusbar.h
	${CMAKE_CURRENT_SOURCE_DIR}/style_widgets.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/style_widgets.h
	${CMAKE_CURRENT_SOURCE_DIR}/uicontrol.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/wms_dialog.cpp
)

set(TR1_DIR_ADDED FALSE)
function(add_compat_header hdr)
	# yes, NOT: it will add this only on the first call, afterwards the
	# directory exists so we know that the include has been added
	if (NOT TR1_DIR_ADDED)
		target_include_directories(osm2go_lib PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/tr1wrappers)
		set(TR1_DIR_ADDED TRUE PARENT_SCOPE)
	endif ()
	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/tr1wrappers/${hdr}
			${CMAKE_CURRENT_BINARY_DIR}/tr1wrappers/${hdr}
			COPYONLY)
endfunction()

include(CheckIncludeFileCXX)
include(CheckCXXSourceCompiles)

check_include_file_cxx(cstdint CSTDINT_HEADER)
if (NOT CSTDINT_HEADER)
	add_compat_header(cstdint)
	add_compat_header(cinttypes)
endif ()

check_cxx_source_compiles("#include <array>
int main(int argc, char **) {
  std::array<int, 4> a;
  for (int i = 0; i < (argc > a.size() ? a.size() : argc); i++)
    a[i] = i;
  return a[2];
}
" HAS_STL_ARRAY)
if (NOT HAS_STL_ARRAY)
	add_compat_header(array)
endif ()

check_cxx_source_compiles("#include <mutex>
int main() {
  std::mutex m;
  return !!m.try_lock();
}
" HAS_STL_MUTEX)
if (NOT HAS_STL_MUTEX)
	add_compat_header(mutex)
endif ()

check_cxx_source_compiles("#include <type_traits>
int main() {
  const int r = 5;
  const int &s = r;
  return !!std::is_integral<std::remove_const<typeof(s)>::type>::value;
}
" HAS_STL_TYPE_TRAITS)
if (NOT HAS_STL_TYPE_TRAITS)
	add_compat_header(type_traits)
endif ()

check_cxx_source_compiles("#include <unordered_map>
int main(int argc, char **argv) {
  std::unordered_map<int, char *> m;
  for (int i = 0; i < argc; i++)
    m[i] = argv[i];
  return !!m.empty();
}
" HAS_STL_UNORDERED_MAP)
if (NOT HAS_STL_UNORDERED_MAP)
	add_compat_header(unordered_map)
endif ()

check_cxx_source_compiles("#include <unordered_set>
int main(int argc, char **argv) {
  std::unordered_set<char *> m;
  for (int i = 0; i < argc; i++)
    m.insert(argv[i]);
  return !!m.empty();
}
" HAS_STL_UNORDERED_SET)
if (NOT HAS_STL_UNORDERED_SET)
	add_compat_header(unordered_set)
endif ()

pkg_check_modules(GooCanvas REQUIRED IMPORTED_TARGET goocanvas)
target_sources(osm2go_lib PRIVATE
	src/platforms/gtk/canvas_goocanvas.cpp
)
target_link_libraries(osm2go_platform_lib_private INTERFACE PkgConfig::GooCanvas)

pkg_check_modules(GConf2 REQUIRED IMPORTED_TARGET gconf-2.0)
target_link_libraries(osm2go_platform_lib_private INTERFACE PkgConfig::GConf2)

if (MAEMO_FOUND)
	pkg_check_modules(Hildon REQUIRED IMPORTED_TARGET hildon-1)
	pkg_check_modules(Osso REQUIRED IMPORTED_TARGET libosso)
	pkg_check_modules(libLocation REQUIRED IMPORTED_TARGET liblocation)

	target_link_libraries(osm2go_platform_lib INTERFACE
			PkgConfig::Hildon
			PkgConfig::libLocation
			PkgConfig::Osso
	)

	pkg_check_modules(TabletBrowserInterface REQUIRED tablet-browser-interface)
	add_subdirectory(fremantle)

	# the data root dir is /opt on fremantle
	set(DATA_DIR "/opt/${PROJECT_NAME}" PARENT_SCOPE)
	set(PICKER_MENU On PARENT_SCOPE)

	# SVG icons do not work on my N900
	set(USE_SVG_ICONS_DEFAULT Off PARENT_SCOPE)
else ()
	find_package(GTK2 2.24 REQUIRED)

	pkg_check_modules(LibGPS REQUIRED IMPORTED_TARGET libgps)
	target_link_libraries(osm2go_platform_lib_private INTERFACE PkgConfig::LibGPS)

	target_link_libraries(osm2go_platform_lib INTERFACE
			${GTK2_TARGETS}
	)

	add_subdirectory(desktop)

	set(USE_SVG_ICONS_DEFAULT On PARENT_SCOPE)
endif ()
target_compile_definitions(osm2go_platform_lib INTERFACE GTK_DISABLE_DEPRECATED)

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
else ()
	install(CODE
		"message(STATUS \"Not Optifying binary\")")
	install(TARGETS osm2go DESTINATION ${CMAKE_INSTALL_BINDIR})
endif ()
