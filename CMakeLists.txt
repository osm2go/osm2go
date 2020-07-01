cmake_minimum_required(VERSION 3.13 FATAL_ERROR)

project(osm2go VERSION 0.9.16 LANGUAGES C CXX)

include(CMakePushCheckState)
include(CheckCXXSourceCompiles)
include(CheckFunctionExists)
include(CMakePushCheckState)
include(CTest)
include(GNUInstallDirs)
include(WriteCompilerDetectionHeader)

set(CMAKE_C_STANDARD 99)

find_package(OpenSSL 1 REQUIRED)
pkg_check_modules(MAEMO maemo-version)
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/curl/CMakeLists.txt")
	# initialize the curl flags, taken from CMake itself
	set(BUILD_CURL_EXE OFF CACHE INTERNAL "No curl exe")
	set(BUILD_DASHBOARD_REPORTS OFF CACHE INTERNAL "No curl dashboard reports")
	set(BUILD_RELEASE_DEBUG_DIRS OFF CACHE INTERNAL "No curl release/debug dirs")
	set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "Build shared libraries")
	set(CMAKE_USE_GSSAPI OFF CACHE INTERNAL "Disable curl gssapi")
	set(CMAKE_USE_LIBSSH2 OFF CACHE INTERNAL "Disable curl libssh2")
	set(CMAKE_USE_LIBSSH OFF)
	set(CMAKE_USE_OPENLDAP OFF CACHE INTERNAL "No curl OpenLDAP")
	set(CURL_DISABLE_COOKIES OFF CACHE INTERNAL "Do not disable curl cookie support")
	set(CURL_DISABLE_CRYPTO_AUTH ON CACHE INTERNAL "Do not disable curl crypto auth")
	set(CURL_DISABLE_DICT ON CACHE INTERNAL "Disable curl dict protocol?")
	set(CURL_DISABLE_FILE ON CACHE INTERNAL "Disable curl file protocol?")
	set(CURL_DISABLE_FTP ON CACHE INTERNAL "Disable curl ftp protocol?")
	set(CURL_DISABLE_GOPHER ON CACHE INTERNAL "Disable curl gopher protocol?")
	set(CURL_DISABLE_HTTP OFF CACHE INTERNAL "Disable curl http protocol?")
	set(CURL_DISABLE_IMAP ON CACHE INTERNAL "Disable curl imap protocol?")
	set(CURL_DISABLE_LDAP ON CACHE INTERNAL "Disable curl ldap protocol?")
	set(CURL_DISABLE_LDAPS ON CACHE INTERNAL "Disable curl ldaps protocol?")
	set(CURL_DISABLE_POP3 ON CACHE INTERNAL "Disable curl pop3 protocol?")
	set(CURL_DISABLE_PROXY OFF CACHE INTERNAL "Do not disable curl proxy")
	set(CURL_DISABLE_RTSP ON CACHE INTERNAL "Disable curl rtsp protocol?")
	set(CURL_DISABLE_SMTP ON CACHE INTERNAL "Disable curl smtp protocol?")
	set(CURL_DISABLE_TELNET ON CACHE INTERNAL "Disable curl telnet protocol?")
	set(CURL_DISABLE_TFTP ON CACHE INTERNAL "Disable curl tftp protocol?")
	set(CURL_DISABLE_VERBOSE_STRINGS OFF CACHE INTERNAL "Do not disable curl verbosity")
	set(CURL_ENABLE_MQTT OFF)
	set(CURL_HIDDEN_SYMBOLS OFF CACHE INTERNAL "No curl hidden symbols")
	set(CURL_WERROR OFF CACHE INTERNAL "Turn compiler warnings into errors")
	set(DISABLED_THREADSAFE OFF CACHE INTERNAL "Curl can use thread-safe functions")
	set(ENABLE_ARES OFF CACHE INTERNAL "No curl c-ares support")
	set(ENABLE_ALT_SVC OFF)
	set(ENABLE_CURLDEBUG OFF CACHE INTERNAL "No curl TrackMemory features")
	set(ENABLE_DEBUG OFF CACHE INTERNAL "No curl debug features")
	set(ENABLE_IPV6 ON CACHE INTERNAL "No curl IPv6 support")
	set(ENABLE_MANUAL OFF CACHE INTERNAL "No curl built-in manual")
	set(ENABLE_THREADED_RESOLVER OFF CACHE INTERNAL "No curl POSIX threaded DNS lookup")
	set(ENABLE_UNIX_SOCKETS OFF CACHE INTERNAL "No curl Unix domain sockets support")
	set(HTTP_ONLY OFF CACHE INTERNAL "Curl is not http-only")
	set(PICKY_COMPILER OFF CACHE INTERNAL "Enable picky compiler options")
	set(USE_NGHTTP2 OFF)
	set(USE_NGTCP2 OFF)
	set(USE_QUICHE OFF)
	set(USE_WIN32_LDAP OFF CACHE INTERNAL "No curl Windows LDAP")
	set(CMAKE_USE_WINSSL OFF CACHE INTERNAL "Enable Windows SSL")
	set(CMAKE_USE_DARWINSSL OFF CACHE INTERNAL "Enable Darwin SSL")
	set(CMAKE_USE_OPENSSL On CACHE INTERNAL "Enable OpenSSL")
	set(CMAKE_USE_MBEDTLS OFF CACHE INTERNAL "Enable mbedTLS for SSL/TLS")
	set(CMAKE_USE_WOLFSSL OFF)

	# tested on N900, so newer versions should be fine, too
	set(HAVE_POLL_FINE_EXITCODE 0 CACHE INTERNAL "exit code of poll test program")
	set(HAVE_POLL_FINE_EXITCODE__TRYRUN_OUTPUT "" CACHE INTERNAL "Output of poll test program")

	if (MAEMO_FOUND)
		set(CURL_CA_PATH "/etc/certs/common-ca/" CACHE INTERNAL "Location of default CA path.")
	endif ()
	set(ORIG_TESTING ${BUILD_TESTING})
	set(BUILD_TESTING OFF)
	add_subdirectory(curl)
	set(BUILD_TESTING ${ORIG_TESTING})
	unset(ORIG_TESTING)
	set(CURL_LIBRARIES libcurl)
	set(CURL_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/curl/include")
else ()
	find_package(CURL 7.32 REQUIRED)
endif ()
find_package(LibXml2 REQUIRED)
find_package(PkgConfig REQUIRED)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wshadow -Wpointer-arith")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wshadow -Wpointer-arith")

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 4.8)
	# the warning is already available in gcc 4.7, but there NULL is badly defined so it will
	# cause lot of warnings in the Gtk headers
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wzero-as-null-pointer-constant")
endif ()

set(CMAKE_CXX_STANDARD 11)

include(${CMAKE_CURRENT_SOURCE_DIR}/find_filesystem.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/find_optional.cmake)

set(CMAKE_INCLUDE_CURRENT_DIR TRUE)

write_compiler_detection_header(
		FILE "${CMAKE_CURRENT_BINARY_DIR}/osm2go_cpp.h"
		PREFIX "O2G"
		COMPILERS GNU Clang
		FEATURES cxx_nullptr cxx_override cxx_static_assert cxx_deleted_functions
				cxx_noexcept
		ALLOW_UNKNOWN_COMPILER_VERSIONS ALLOW_UNKNOWN_COMPILERS
		${WCDH_COMPAT}
		BARE_FEATURES cxx_nullptr cxx_override cxx_noexcept
		EPILOG
"#if !(defined(O2G_COMPILER_CXX_STATIC_ASSERT) && O2G_COMPILER_CXX_STATIC_ASSERT)
#  define static_assert(X, MSG) O2G_STATIC_ASSERT_MSG(X, MSG)
#endif
")

add_library(osm2go_lib STATIC
	src/appdata.h
	src/area_edit.h
	src/canvas.cpp
	src/canvas.h
	src/color.h
	src/diff.cpp
	src/diff.h
	src/fdguard.cpp
	src/fdguard.h
	src/gps_state.h
	src/icon.h
	src/iconbar.h
	src/info.h
	src/josm_elemstyles.cpp
	src/josm_elemstyles.h
	src/josm_elemstyles_p.h
	src/josm_presets.cpp
	src/josm_presets.h
	src/josm_presets_p.h
	src/josm_presets_parser.cpp
	src/map.cpp
	src/map.h
	src/map_edit.cpp
	src/map_hl.cpp
	src/map_hl.h
	src/map_state.h
	src/misc.cpp
	src/misc.h
	src/notifications.h
	src/net_io.cpp
	src/net_io.h
	src/osm.cpp
	src/osm.h
	src/osm_p.h
	src/osm_api.cpp
	src/osm_api.h
	src/osm_api_p.h
	src/osm_objects.cpp
	src/osm_objects.h
	src/osm_parser.cpp
	src/osm2go_annotations.cpp
	src/osm2go_annotations.h
	src/osm2go_stl.h
	src/pos.cpp
	src/pos.h
	src/project.cpp
	src/project.h
	src/project_p.h
	src/relation_edit.h
	src/settings.h
	src/style.cpp
	src/style.h
	src/style_p.h
	src/track.cpp
	src/track.h
	src/uicontrol.h
	src/wms.cpp
	src/wms.h
	src/wms_p.h
	${CMAKE_CURRENT_BINARY_DIR}/osm2go_cpp.h
)

set(DATA_DIR "${CMAKE_INSTALL_FULL_DATADIR}/${PROJECT_NAME}")

option(PICKER_MENU "use a picker table for tagging preset selection" OFF)

add_subdirectory(src/platforms/gtk)

find_library(MATH_LIBRARY m)

target_compile_definitions(osm2go_lib
		PUBLIC PACKAGE="${PROJECT_NAME}"
		PUBLIC VERSION="${PROJECT_VERSION}"
)

set_property(SOURCE src/platforms/gtk/net_io_curl.cpp
		src/platforms/gtk/osm_upload_dialog.cpp
		src/osm_api.cpp
		APPEND PROPERTY COMPILE_DEFINITIONS "CURL_NO_OLDIES")
set_property(SOURCE src/platforms/gtk/platform.cpp APPEND PROPERTY COMPILE_DEFINITIONS "DATADIR=\"${DATA_DIR}\"")

# there shouldn't be a reason to change the server url
option(SERVER_EDITABLE "add widget to make API URL editable" OFF)
if (SERVER_EDITABLE)
	set_property(SOURCE src/platforms/gtk/project_widgets.cpp APPEND PROPERTY COMPILE_DEFINITIONS "SERVER_EDITABLE")
endif ()

if (PICKER_MENU)
	set_property(SOURCE src/platforms/gtk/josm_presets_button.cpp APPEND PROPERTY COMPILE_DEFINITIONS "PICKER_MENU")
endif ()

option(USE_SVG_ICONS "Use SVG icons without converting them to PNG at build time" ${USE_SVG_ICONS_DEFAULT})
if (USE_SVG_ICONS)
	set_property(SOURCE src/platforms/gtk/icon.cpp APPEND PROPERTY COMPILE_DEFINITIONS USE_SVG_ICONS)
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
		${CMAKE_CURRENT_BINARY_DIR}
		src
)

add_subdirectory(data)

if (BUILD_TESTING)
	add_subdirectory(test)
endif ()
