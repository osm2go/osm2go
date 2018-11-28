# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2018 Rolf Eike Beer <eike@sf-mail.de>.

# find out if std::optional can be used

set(OPT_TESTCODE
"
#ifdef CXX17_OPTIONAL
#include <optional>
#elif defined(CXX14_EXP_OPTIONAL)
#include <experimental/optional>
namespace std {
	using experimental::optional;
}
#endif

int main(void)
{
	std::optional<int> x = 1;
	return x ? *x - 1 : 1;
}
"
)

if (CMAKE_CXX_STANDARD LESS 17)
	set(OLD_STANDARD "${CMAKE_CXX_STANDARD}")
	set(CMAKE_CXX_STANDARD 17)
endif ()

check_cxx_source_compiles("${OPT_TESTCODE}" CXX17_OPTIONAL)

if (OLD_STANDARD)
	if (NOT CXX17_OPTIONAL)
		set(CMAKE_CXX_STANDARD ${OLD_STANDARD})
	endif ()
	unset(OLD_STANDARD)
endif ()

if (NOT CXX17_OPTIONAL)
	if (CMAKE_CXX_STANDARD LESS 14)
		set(OLD_STANDARD "${CMAKE_CXX_STANDARD}")
		set(CMAKE_CXX_STANDARD 14)
	endif ()

	check_cxx_source_compiles("${OPT_TESTCODE}" CXX14_EXP_OPTIONAL)
	if (NOT CXX14_EXP_OPTIONAL)
		set(CMAKE_CXX_STANDARD ${OLD_STANDARD})
	endif ()
	unset(OLD_STANDARD)
	configure_file("${CMAKE_CURRENT_SOURCE_DIR}/optional.hxx.in"
			"${CMAKE_CURRENT_BINARY_DIR}/optional"
			@ONLY)
endif ()

unset(OPT_TESTCODE)
