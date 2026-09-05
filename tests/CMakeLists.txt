# SPDX-License-Identifier: MIT
# Copyright (c) 2026 itsloopyo / CameraUnlock
# Characterization tests for the mod's pure logic. Deliberately links nothing
# from the game or the hooks: anything that needs a live client.dll is verified
# in-game, and anything that does not belongs here.
add_executable(portal_rtx_headtracking_tests
    test_main.cpp
    source_math_tests.cpp
    position_mapping_tests.cpp
    log_throttle_tests.cpp
    build_profile_tests.cpp
    tracker_axes_tests.cpp
    ${CMAKE_SOURCE_DIR}/src/source_math.cpp
    ${CMAKE_SOURCE_DIR}/src/builds/steam_offsets.cpp
    ${CMAKE_SOURCE_DIR}/src/builds/build_registry.cpp
)

target_include_directories(portal_rtx_headtracking_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/cameraunlock-core/cpp/include
)

# The core library, for the fingerprint comparison and the log sink the build
# registry calls into. Still nothing from the game and nothing from the hooks:
# the registry is compiled in so the profile tests assert over the array the mod
# actually routes on rather than a copy of it.
target_link_libraries(portal_rtx_headtracking_tests PRIVATE cameraunlock)

if(MSVC)
    target_compile_options(portal_rtx_headtracking_tests PRIVATE /W4 /WX)
else()
    target_compile_options(portal_rtx_headtracking_tests PRIVATE -Wall -Wextra -Werror)
endif()

add_test(NAME portal_rtx_headtracking_tests COMMAND portal_rtx_headtracking_tests)
