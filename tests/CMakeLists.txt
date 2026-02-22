add_executable(ShellBoyTests test_cpu.cpp)

target_link_libraries(ShellBoyTests
    PRIVATE
    core
    gtest_main
)

include(GoogleTest)
gtest_discover_tests(ShellBoyTests)
