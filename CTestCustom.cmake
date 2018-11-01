set(CTEST_CUSTOM_COVERAGE_EXCLUDE
	"/test/"
	"/CMakeFiles/"
	"/curl/"
	"/src/platforms/gtk/osm-gps-map/"
)

set(CTEST_EXTRA_COVERAGE_GLOB
	"*.[ch]"
	"*.cpp"
)

set(CTEST_CUSTOM_MEMCHECK_IGNORE
	"parsechangelog"
)
