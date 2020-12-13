set(CTEST_CUSTOM_COVERAGE_EXCLUDE
	"/test/"
	"/CMakeFiles/"
	"/curl/"
	"/[^/]*_autogen/"
)

set(CTEST_EXTRA_COVERAGE_GLOB
	"*.[ch]"
	"*.cpp"
)

set(CTEST_CUSTOM_MEMCHECK_IGNORE
	"parsechangelog"
)
