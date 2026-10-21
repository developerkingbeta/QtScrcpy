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
qsc_add_test(tst_registrar)
qsc_add_test(tst_controller)
target_link_libraries(tst_controller PRIVATE Qt${QT_DESIRED_VERSION}::Widgets)

# Absolute fixture root so QFINDTESTDATA / wrong cwd cannot starve registrar flows.
target_compile_definitions(tst_registrar PRIVATE
    QSC_TEST_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
target_compile_definitions(tst_parsers PRIVATE
    QSC_TEST_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

add_executable(tst_batchdialog
    tst_batchdialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/batchdialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../util/config.cpp
)
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    target_sources(tst_batchdialog PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../util/path.mm)
endif()
target_include_directories(tst_batchdialog PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui
    ${CMAKE_CURRENT_SOURCE_DIR}/../util
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(tst_batchdialog PRIVATE
    qsc_batch
    Qt${QT_DESIRED_VERSION}::Test
    Qt${QT_DESIRED_VERSION}::Widgets
    Qt${QT_DESIRED_VERSION}::Core
)
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    target_link_libraries(tst_batchdialog PRIVATE "-framework Foundation")
endif()
add_test(NAME tst_batchdialog COMMAND tst_batchdialog)
set_tests_properties(tst_import PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/fixtures)
set_tests_properties(tst_adbrunner PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/fixtures)
set_tests_properties(tst_deviceprobe PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/fixtures)
set_tests_properties(tst_parsers PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/fixtures)
set_tests_properties(tst_registrar PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/fixtures)
set_tests_properties(tst_controller PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/fixtures)
