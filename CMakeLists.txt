cmake_minimum_required(VERSION 3.15)
project(SnakeGame CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Bake the Visual C++ runtime into snake.exe so other people don't need to
# install anything extra to run it. (Must be set before Raylib is added.)
if (MSVC)
    set(CMAKE_POLICY_DEFAULT_CMP0091 NEW)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Download and build Raylib automatically
include(FetchContent)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.zip
)
FetchContent_MakeAvailable(raylib)

add_executable(snake main.cpp)
target_link_libraries(snake PRIVATE raylib)

# In Release builds, hide the black console window behind the game.
# (Debug builds keep it, so you can still see Raylib's log messages.)
# In non-Debug builds (Release, RelWithDebInfo), hide the black console window.
# (Debug builds keep it, so you can still see Raylib's log messages.)
if (MSVC)
    target_link_options(snake PRIVATE
        "$<$<NOT:$<CONFIG:Debug>>:/SUBSYSTEM:WINDOWS>"
        "$<$<NOT:$<CONFIG:Debug>>:/ENTRY:mainCRTStartup>")
elseif (MINGW)
    target_link_options(snake PRIVATE "$<$<NOT:$<CONFIG:Debug>>:-mwindows>")
endif()