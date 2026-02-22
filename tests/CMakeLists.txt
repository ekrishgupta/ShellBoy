add_executable(ShellBoyTests test_cpu.cpp)

target_link_libraries(ShellBoyTests
    PRIVATE
    core
    gtest_main
)

target_include_directories(ShellBoyTests PRIVATE ${CMAKE_SOURCE_DIR})

include(GoogleTest)
gtest_discover_tests(ShellBoyTests)
