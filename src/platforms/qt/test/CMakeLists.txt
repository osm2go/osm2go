find_package(Qt5 ${Qt5_VERSION} COMPONENTS Quick QuickTest REQUIRED)

function(o2g_qt_test NAME)
	add_executable(test_${NAME} test_${NAME}.cpp)
	target_link_libraries(test_${NAME}
		PRIVATE
			osm2go_lib
			Qt5::Test
	)

	add_test(NAME ${NAME} COMMAND test_${NAME})
endfunction()

function(o2g_quick_test NAME)
	add_executable(test_qml_${NAME}
		${CMAKE_CURRENT_BINARY_DIR}/../qml.qrc
	)

	if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_qml_${NAME}.cpp")
		target_sources(test_qml_${NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/test_qml_${NAME}.cpp")
	else ()
		file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/test_qml_${NAME}.cpp" "#include <QtQuickTest>
QUICK_TEST_MAIN(${NAME})")
		target_sources(test_qml_${NAME} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/test_qml_${NAME}.cpp")
	endif ()
	target_link_libraries(test_qml_${NAME}
		PRIVATE
			osm2go_lib
			Qt5::Quick
			Qt5::QuickTest
	)

	add_test(NAME "Qml ${NAME}" COMMAND test_qml_${NAME} -input "${CMAKE_CURRENT_SOURCE_DIR}/test_${NAME}.qml")
endfunction()

o2g_qt_test(ProjectSelectModel)
o2g_qt_test(RelationMemberModel)
o2g_qt_test(RelationMembershipModel)
o2g_qt_test(RelationModel)
o2g_qt_test(TagModel)
o2g_qt_test(WmsModel)

o2g_qt_test(OldOrNotEmptyValidator)
o2g_qt_test(ProjectNameValidator)
o2g_qt_test(UrlValidator)
o2g_qt_test(WmsNameValidator)

# QtQuick on sparc segfaults
if (Qt5_VERSION VERSION_GREATER_EQUAL 5.11 AND NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^sparc")
	o2g_quick_test(DegreeInput)
endif ()
