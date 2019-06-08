##
## OSM2go CTest script
##
## This will run a Nightly build on OSM2go
##

##
## What you need:
##
## All platforms:
## -cmake >= 3.8
## -git command line client
## -all the other stuff needed to build OSM2go like CURL, libXML2, compiler, ...
##

##
## How to setup:
##
## Write to a file my_osm2go.cmake:
##
## ######### begin file
## # the binary directory does not need to exist (but it's parent)
## # it will be deleted before use
## set(OSM2GO_BUILD_DIR "my/path/to/the/build/dir")
##
## # if you don't want to run a Nightly, but e.g. an Experimental build
## # set(dashboard_model "Experimental")
##
## # if your "git" executable can not be found by FindGit.cmake
## # set(GIT_EXECUTABLE "path/to/my/git")
##
## # if you only want to run the test, but not submit the results
## set(NO_SUBMIT TRUE)
##
## # if you are not on a openSUSE system the script currently doesn't
## # set a proper build name
## set(CTEST_BUILD_NAME "Fedora Core 14 x86_64")
##
## # add extra configure options
## # set(CONF_OPTIONS "-DPICKER_MENU=On")
##
## # This _*MUST*_ be the last command in this file!
## include(/path/to/osm2go/ctest_osm2go.cmake)
## ######### end file
##
## Then run this script with
## ctest -S my_osm2go_nightly.cmake -V
##

# Check for required variables.
foreach (req
		OSM2GO_BUILD_DIR
	)
	if (NOT DEFINED ${req})
		message(FATAL_ERROR "The containing script must set ${req}")
	endif ()
endforeach ()

cmake_minimum_required(VERSION 3.13)

if (NOT GIT_EXECUTABLE)
	find_package(Git REQUIRED)
endif()
set(UpdateCommand ${GIT_EXECUTABLE})

set(CTEST_SOURCE_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})
set(CTEST_BINARY_DIRECTORY ${OSM2GO_BUILD_DIR})

# Select the model (Nightly, Experimental, Continuous).
if (NOT DEFINED dashboard_model)
	set(dashboard_model Nightly)
endif()
if (NOT "${dashboard_model}" MATCHES "^(Nightly|Experimental|Continuous)$")
	message(FATAL_ERROR "dashboard_model must be Nightly, Experimental, or Continuous")
endif()

if (NOT CTEST_CMAKE_GENERATOR)
	set(CTEST_CMAKE_GENERATOR "Unix Makefiles")
endif ()

# set the site name
if (NOT CTEST_SITE)
	execute_process(COMMAND hostname --fqdn
			OUTPUT_VARIABLE CTEST_SITE
			OUTPUT_STRIP_TRAILING_WHITESPACE)
endif ()

# set the build name
if (NOT CTEST_BUILD_NAME)
	if (EXISTS /etc/SuSE-release)
		file(STRINGS /etc/SuSE-release _SUSEVERSION LIMIT_COUNT 1)
		string(REGEX REPLACE "[\\(\\)]" "" CTEST_BUILD_NAME "${_SUSEVERSION}")
		unset(_SUSEVERSION)
	elseif (EXISTS /etc/os-release)
		file(STRINGS /etc/os-release _OSVERSION)
		foreach(_OSVERSION_STRING ${_OSVERSION} REGEX "^(NAME|VERSION_ID)=")
			if (_OSVERSION_STRING MATCHES "^NAME=\"?([^\"]*)\"?$")
				set(_OSVER_NAME "${CMAKE_MATCH_1}")
			elseif (_OSVERSION_STRING MATCHES "^VERSION_ID=\"?([^\"]*)\"?$")
				set(_OSVER_VERSION "${CMAKE_MATCH_1}")
			endif ()
		endforeach()
		unset(_OSVERSION)
		if (_OSVER_NAME AND _OSVER_VERSION)
			set(CTEST_BUILD_NAME "${_OSVER_NAME} ${_OSVER_VERSION}")
		endif ()
	endif ()
endif ()

if (NOT CTEST_BUILD_NAME)
	message(FATAL_ERROR "CTEST_BUILD_NAME not set.\nPlease set this to a sensible value, preferably in the form \"distribution version architecture\", something like \"openSUSE 11.3 i586\"")
endif ()

find_program(CTEST_MEMORYCHECK_COMMAND valgrind)
if ($ENV{CC} MATCHES clang)
	find_program(CTEST_COVERAGE_COMMAND llvm-cov)
	set(CTEST_COVERAGE_EXTRA_FLAGS gcov)
else ()
	find_program(CTEST_COVERAGE_COMMAND gcov)
endif ()

ctest_read_custom_files(${CMAKE_CURRENT_LIST_DIR})

if (NOT IS_DIRECTORY ${CTEST_BINARY_DIRECTORY})
	make_directory(${CTEST_BINARY_DIRECTORY})
endif ()

ctest_empty_binary_directory(${CTEST_BINARY_DIRECTORY})

ctest_start(${dashboard_model})

ctest_update()

# get coverage: debug build
list(APPEND CONF_OPTIONS "-DCMAKE_BUILD_TYPE=Debug")

ctest_configure(
		OPTIONS "${CONF_OPTIONS}"
)
ctest_build()

ctest_test()

if (CTEST_COVERAGE_COMMAND)
	ctest_coverage()
endif ()

if (CTEST_MEMORYCHECK_COMMAND)
#	set(CTEST_MEMORYCHECK_SUPPRESSIONS_FILE "${CMAKE_CURRENT_LIST_DIR}/valgrind.supp")
	ctest_memcheck()
endif ()

if (NOT NO_SUBMIT)
	ctest_submit()
endif ()
