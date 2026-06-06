cmake_minimum_required(VERSION 3.20)

# ── SDL2 ──────────────────────────────────────────────────────────────────────
find_package(SDL2 REQUIRED)

# SDL2 imported target name differs between cmake versions and MinGW packages
if(NOT TARGET SDL2::SDL2)
    add_library(SDL2::SDL2 INTERFACE IMPORTED)
    set_target_properties(SDL2::SDL2 PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${SDL2_INCLUDE_DIRS}"
        INTERFACE_LINK_LIBRARIES      "${SDL2_LIBRARIES}")
endif()

find_package(SDL2_mixer QUIET)
if(NOT SDL2_mixer_FOUND)
    # Try pkg-config fallback (common on MinGW)
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(SDL2_MIXER QUIET SDL2_mixer)
        if(SDL2_MIXER_FOUND)
            set(SDL2_mixer_FOUND TRUE)
            add_library(SDL2_mixer::SDL2_mixer INTERFACE IMPORTED)
            set_target_properties(SDL2_mixer::SDL2_mixer PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${SDL2_MIXER_INCLUDE_DIRS}"
                INTERFACE_LINK_LIBRARIES      "${SDL2_MIXER_LIBRARIES}")
        endif()
    endif()
endif()

add_library(j2me_runtime STATIC
    src/j2me_runtime.cpp
    src/display.cpp
    src/audio.cpp
    src/timer.cpp
    src/rms.cpp
    src/midlet.cpp
    src/gc.cpp
    src/j2me_gfx.cpp
)

target_include_directories(j2me_runtime PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${SDL2_INCLUDE_DIRS}
)

target_link_libraries(j2me_runtime PUBLIC SDL2::SDL2)

if(SDL2_mixer_FOUND)
    target_link_libraries(j2me_runtime PUBLIC SDL2_mixer::SDL2_mixer)
    target_compile_definitions(j2me_runtime PUBLIC HAVE_SDL2_MIXER)
else()
    message(STATUS "[j2me-runtime] SDL2_mixer not found — audio disabled")
endif()

target_compile_features(j2me_runtime PUBLIC cxx_std_17)

if(WIN32)
    target_compile_definitions(j2me_runtime PRIVATE
        _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_options(j2me_runtime INTERFACE -static-libgcc -static-libstdc++)
    # On MinGW SDL2main must come before SDL2
    if(TARGET SDL2::SDL2main)
        target_link_libraries(j2me_runtime PUBLIC -lmingw32 SDL2::SDL2main)
    else()
        target_link_libraries(j2me_runtime PUBLIC -lmingw32 -lSDL2main)
    endif()
endif()
