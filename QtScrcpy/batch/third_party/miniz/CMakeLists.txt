add_library(qsc_miniz STATIC miniz.c)
set_target_properties(qsc_miniz PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
target_compile_options(qsc_miniz PRIVATE -w)
target_include_directories(qsc_miniz PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
