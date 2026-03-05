add_library(core
    Bus.cpp
    CPU.cpp
    PPU.cpp
    Timer.cpp
    Joypad.cpp
    Serial.cpp
    APU.cpp
    AudioBackend.cpp
)

target_include_directories(core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/thirdparty
)

# macOS: miniaudio uses CoreAudio / AudioToolbox
if(APPLE)
    target_link_libraries(core PRIVATE
        "-framework CoreAudio"
        "-framework AudioToolbox"
    )
elseif(UNIX)
    target_link_libraries(core PRIVATE pthread dl m)
endif()
