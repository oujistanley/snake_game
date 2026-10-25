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

# On Windows, snake.rc embeds snake.ico into snake.exe. That gives the file
# its icon in File Explorer, and the game window and taskbar use it too.
set(SNAKE_SOURCES main.cpp)
if (WIN32)
    enable_language(RC)
    list(APPEND SNAKE_SOURCES snake.rc)
endif()

add_executable(snake ${SNAKE_SOURCES})
target_link_libraries(snake PRIVATE raylib)

# In non-Debug builds (Release, RelWithDebInfo), hide the black console window.
# (Debug builds keep it, so you can still see Raylib's log messages.)
if (MSVC)
    target_link_options(snake PRIVATE
        "$<$<NOT:$<CONFIG:Debug>>:/SUBSYSTEM:WINDOWS>"
        "$<$<NOT:$<CONFIG:Debug>>:/ENTRY:mainCRTStartup>")
elseif (MINGW)
    target_link_options(snake PRIVATE "$<$<NOT:$<CONFIG:Debug>>:-mwindows>")
endif()
