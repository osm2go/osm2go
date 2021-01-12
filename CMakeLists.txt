cmake_minimum_required(VERSION 3.13 FATAL_ERROR)

project(osm2go VERSION 0.9.17 LANGUAGES C CXX)

include(CMakePushCheckState)
include(CheckCXXSourceCompiles)
include(CheckFunctionExists)
include(CMakePushCheckState)
include(CTest)
include(FeatureSummary)
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

set(CMAKE_OPTIMIZE_DEPENDENCIES On)

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
#if __cplusplus >= 201103L
#  define O2G_OPERATOR_EXPLICIT explicit
#else
#  define O2G_OPERATOR_EXPLICIT
#endif
")

set(DATA_DIR "${CMAKE_INSTALL_FULL_DATADIR}/${PROJECT_NAME}")

option(PICKER_MENU "use a picker table for tagging preset selection" OFF)
add_feature_info(PickerMenu PICKER_MENU "a picker menu like the one used on the N900")
option(BUILD_WITH_QT "build the experimental Qt version" OFF)
add_feature_info(Qt BUILD_WITH_QT "Qt instead of Gtk+ libraries are used for GUI")

if (BUILD_WITH_QT)
	# must be here so that AUTOMOC and AUTOUIC properties can be already set for osm2go_lib
	find_package(Qt5 REQUIRED Core Widgets)
endif ()

find_library(MATH_LIBRARY m)

# there shouldn't be a reason to change the server url
option(SERVER_EDITABLE "add widget to make API URL editable" OFF)
add_feature_info(ServerEditable SERVER_EDITABLE "an extra widget is shown where the URL of the OSM data server can be changed")

if (NOT PICKER_MENU)
	set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
endif ()

add_subdirectory(src)

option(USE_SVG_ICONS "Use SVG icons without converting them to PNG at build time" ${USE_SVG_ICONS_DEFAULT})
add_feature_info(Svg USE_SVG_ICONS "Scalable Vector Graphics are used to render icons, resulting in possibly better image quality")

if (BUILD_TESTING)
	add_subdirectory(test)
endif ()

add_subdirectory(data)

install(FILES LICENSES/GPL-3.0-or-later
	DESTINATION ${DATA_DIR})

feature_summary(WHAT ENABLED_FEATURES DISABLED_FEATURES)
