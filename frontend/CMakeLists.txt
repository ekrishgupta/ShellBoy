add_executable(ShellBoy BrailleRenderer.cpp main.cpp)

# Link FTXUI and our core library
target_link_libraries(ShellBoy
    PRIVATE 
    core
    mmu
    ftxui::screen
    ftxui::dom
    ftxui::component
)

target_include_directories(ShellBoy PRIVATE ${CMAKE_SOURCE_DIR})
