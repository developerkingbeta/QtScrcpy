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
