cmake_minimum_required(VERSION 3.16)

project(ZetianExternal LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(ZetianExternal
    src/main.cpp
    src/json_parser.cpp
    src/offset_loader.cpp
    src/process.cpp
    src/roblox_state.cpp
    src/death_detector.cpp
    src/player_monitor.cpp
    src/crosshair_renderer.cpp
)

target_compile_definitions(ZetianExternal PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
)

target_compile_options(ZetianExternal PRIVATE /W4 /permissive-)

target_link_libraries(ZetianExternal PRIVATE user32 gdi32 gdiplus)

add_custom_command(TARGET ZetianExternal POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/offsets.json
        $<TARGET_FILE_DIR:ZetianExternal>/offsets.json
    COMMENT "Copying offsets.json next to the executable"
)

add_custom_command(TARGET ZetianExternal POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/crosshairs
        $<TARGET_FILE_DIR:ZetianExternal>/crosshairs
    COMMENT "Copying crosshairs folder next to the executable"
)
