add_executable(ShellBoy BrailleRenderer.cpp main.cpp)

# Link FTXUI and our core library
target_link_libraries(ShellBoy
    PRIVATE 
    core
    ftxui::screen
    ftxui::dom
    ftxui::component
)
