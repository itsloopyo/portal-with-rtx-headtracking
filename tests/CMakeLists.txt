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

# The differential test for the HeadTracking.ini reader. The oracle is the reader of the newest
# published build (v0.1.0), compiled from its own sources in config_differential/oracle with the
# mod's and core's namespaces renamed, so it links beside today's code and nothing in it can
# resolve to a symbol of the mod under test. Its include roots put the published build's files
# ahead of core's headers.
set(PORTAL_CORE_CPP ${CMAKE_SOURCE_DIR}/cameraunlock-core/cpp)
add_library(portal_rtx_config_oracle STATIC
    config_differential/oracle_api.cpp
    config_differential/oracle/src/config.cpp
    ${PORTAL_CORE_CPP}/src/config/ini_reader.cpp
    ${PORTAL_CORE_CPP}/src/logging/file_log.cpp
)
target_include_directories(portal_rtx_config_oracle PRIVATE
    config_differential/oracle/src
    ${PORTAL_CORE_CPP}/include
)
target_compile_definitions(portal_rtx_config_oracle PRIVATE
    headtracking=portal_oracle
    cameraunlock=portal_oracle_core
)

add_executable(portal_rtx_config_differential_tests
    config_differential/differential_tests.cpp
    ${CMAKE_SOURCE_DIR}/src/legacy_config/legacy_config.cpp
)
target_include_directories(portal_rtx_config_differential_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    config_differential
)
target_link_libraries(portal_rtx_config_differential_tests PRIVATE portal_rtx_config_oracle cameraunlock bcrypt)
target_compile_definitions(portal_rtx_config_differential_tests PRIVATE PORTAL_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
if(MSVC)
    target_compile_options(portal_rtx_config_differential_tests PRIVATE /W4 /WX)
else()
    target_compile_options(portal_rtx_config_differential_tests PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME portal_rtx_config_differential_tests COMMAND portal_rtx_config_differential_tests)
