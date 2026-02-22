add_library(core Bus.cpp CPU.cpp PPU.cpp Timer.cpp)
target_include_directories(core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} PRIVATE ${CMAKE_SOURCE_DIR})
