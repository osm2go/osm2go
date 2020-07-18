if (MAEMO_FOUND)
	pkg_check_modules(Hildon REQUIRED IMPORTED_TARGET hildon-1)
else ()
	find_package(GTK2 2.24 REQUIRED)
endif ()

add_subdirectory(osm-gps-map)

target_include_directories(osm2go_lib
		PUBLIC
			${CMAKE_CURRENT_SOURCE_DIR})

target_sources(osm2go_lib PRIVATE
	about.cpp
	area_edit.cpp
	icon.cpp
	iconbar.cpp
	josm_presets_button.cpp
	info.cpp
	list.cpp
	list.h
	MainUiGtk.h
	map_gtk.cpp
	map_gtk.h
	net_io_curl.cpp
	notifications.cpp
	osm2go_i18n.h
	osm2go_platform.h
	osm2go_platform_gtk.h
	osm_upload_dialog.cpp
	platform.cpp
	project_widgets.cpp
	relation_edit.cpp
	settings.cpp
	statusbar.h
	style_widgets.cpp
	style_widgets.h
	uicontrol.cpp
	wms_dialog.cpp
)

set(TR1_DIR_ADDED FALSE)
function(add_compat_header hdr)
	# yes, NOT: it will add this only on the first call, afterwards the
	# directory exists so we know that the include has been added
	if (NOT TR1_DIR_ADDED)
		target_include_directories(osm2go_lib PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/tr1wrappers_bin)
		target_include_directories(osm-gps-map PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/tr1wrappers_bin)
		set(TR1_DIR_ADDED TRUE PARENT_SCOPE)
	endif ()
	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/tr1wrappers/${hdr}
			${CMAKE_CURRENT_BINARY_DIR}/tr1wrappers_bin/${hdr}
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

check_cxx_source_compiles("#include <variant>
int main(int argc, char **argv) {
  std::variant<bool, const char *> foo(bar);
  return std::holds_alternative<bool>(foo) ? std::get<bool>(foo) ? 1 : 0 : 0;
}
" HAS_STL_VARIANT)
if (NOT HAS_STL_VARIANT)
	add_compat_header(variant)
endif ()

pkg_check_modules(GooCanvas REQUIRED IMPORTED_TARGET goocanvas)
target_sources(osm2go_lib PRIVATE
	canvas_goocanvas.cpp
)
target_link_libraries(osm2go_lib PRIVATE PkgConfig::GooCanvas)

pkg_check_modules(GConf2 REQUIRED IMPORTED_TARGET gconf-2.0)
target_link_libraries(osm2go_lib PRIVATE PkgConfig::GConf2)

if (MAEMO_FOUND)
	pkg_check_modules(Osso REQUIRED IMPORTED_TARGET libosso)
	pkg_check_modules(libLocation REQUIRED IMPORTED_TARGET liblocation)

	target_link_libraries(osm2go_lib PUBLIC
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
	pkg_check_modules(LibGPS REQUIRED IMPORTED_TARGET libgps)
	target_link_libraries(osm2go_lib PRIVATE PkgConfig::LibGPS)

	target_link_libraries(osm2go_lib PUBLIC
			${GTK2_TARGETS}
	)

	add_subdirectory(desktop)

	set(USE_SVG_ICONS_DEFAULT On PARENT_SCOPE)
endif ()
target_compile_definitions(osm2go_lib PUBLIC GTK_DISABLE_DEPRECATED)

target_link_libraries(osm2go_lib PRIVATE osm-gps-map)

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
