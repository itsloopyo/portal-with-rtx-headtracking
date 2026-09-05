cmake_minimum_required(VERSION 3.20)
project(PortalWithRTXHeadTracking VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "Portal with RTX runs the 32-bit Source engine. Configure with -A Win32.")
endif()

if(WIN32)
    add_definitions(-DWIN32_LEAN_AND_MEAN -DNOMINMAX -D_CRT_SECURE_NO_WARNINGS)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# MinHook, vendored in the core submodule. No network fetch: a mutable git tag
# would change what is statically linked into the shipped .asi with nothing in
# the diff to show it.
set(MINHOOK_DIR "${CMAKE_SOURCE_DIR}/cameraunlock-core/vendor/minhook")
if(NOT EXISTS "${MINHOOK_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "cameraunlock-core/vendor/minhook is missing - run `git submodule update --init`.")
endif()
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
add_subdirectory(${MINHOOK_DIR} minhook)
unset(CMAKE_POLICY_VERSION_MINIMUM)

set(CAMERAUNLOCK_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CAMERAUNLOCK_BUILD_HOOKS ON CACHE BOOL "" FORCE)
add_subdirectory(cameraunlock-core/cpp ${CMAKE_BINARY_DIR}/cameraunlock-core)

add_library(PortalWithRTXHeadTracking SHARED
    src/pch.cpp
    src/dllmain.cpp
    src/debug_log.cpp
    src/config.cpp
    src/plugin.cpp
    src/tracker_feed.cpp
    src/hotkey_handler.cpp
    src/detour.cpp
    src/camera_hook.cpp
    src/fov_override.cpp
    src/aim_state.cpp
    src/aim_point.cpp
    src/crosshair_hook.cpp
    src/game_state.cpp
    src/source_math.cpp
    src/window_centering.cpp
    src/builds/build_registry.cpp
    src/builds/steam_offsets.cpp
)

target_precompile_headers(PortalWithRTXHeadTracking PRIVATE src/pch.h)

# The same bar the test suite is held to. Everything in src/ writes into a
# running game's memory through offsets no header declares, so a truncation or
# an unreferenced local is worth stopping the build for.
if(MSVC)
    target_compile_options(PortalWithRTXHeadTracking PRIVATE /W4 /WX)
else()
    target_compile_options(PortalWithRTXHeadTracking PRIVATE -Wall -Wextra -Werror)
endif()

set_target_properties(PortalWithRTXHeadTracking PROPERTIES
    OUTPUT_NAME "PortalWithRTXHeadTracking"
    SUFFIX ".asi"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)

target_include_directories(PortalWithRTXHeadTracking PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(PortalWithRTXHeadTracking PRIVATE
    cameraunlock
    cameraunlock_hooks
    minhook
    ws2_32
)

enable_testing()
add_subdirectory(tests)
