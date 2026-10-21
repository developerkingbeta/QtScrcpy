function(qsc_add_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE
        qsc_batch
        Qt${QT_DESIRED_VERSION}::Test
        Qt${QT_DESIRED_VERSION}::Core
    )
    add_test(NAME ${name} COMMAND ${name})
endfunction()

qsc_add_test(tst_hostcheck)
qsc_add_test(tst_import)
qsc_add_test(tst_adbrunner)
qsc_add_test(tst_deviceprobe)
qsc_add_test(tst_store)
qsc_add_test(tst_parsers)
set_tests_properties(tst_import PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/fixtures)
set_tests_properties(tst_adbrunner PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/fixtures)
set_tests_properties(tst_deviceprobe PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/fixtures)
set_tests_properties(tst_parsers PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/fixtures)
