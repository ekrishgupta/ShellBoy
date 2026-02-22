cmake_minimum_required(VERSION 3.20)
project(ShellBoy VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Fetch FTXUI
FetchContent_Declare(
    ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI
    GIT_TAG v5.0.0
)
FetchContent_MakeAvailable(ftxui)

# Fetch GoogleTest
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG release-1.12.1
)
FetchContent_MakeAvailable(googletest)

# Add subdirectories (we will create CMakeLists.txt in them later)
# add_subdirectory(core)
# add_subdirectory(mmu)
# add_subdirectory(frontend)

# Enable testing
enable_testing()
# add_subdirectory(tests)
