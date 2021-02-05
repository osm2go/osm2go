set(CMAKE_AUTOMOC TRUE)
set(CMAKE_AUTOUIC TRUE)

function(o2g_qt_test NAME)
	add_executable(test_${NAME} test_${NAME}.cpp)
	target_link_libraries(test_${NAME}
		PRIVATE
			osm2go_lib
			Qt5::Test
	)

	add_test(NAME ${NAME} COMMAND test_${NAME})
endfunction()

o2g_qt_test(RelationMemberModel)
o2g_qt_test(RelationMembershipModel)
o2g_qt_test(RelationModel)
o2g_qt_test(TagModel)
o2g_qt_test(WmsModel)