cmake_minimum_required(VERSION 3.20)
project(J2MERecompUniversal CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Options ───────────────────────────────────────────────────────────────────
option(J2ME_BUILD_RECOMP_TOOL  "Build the j2me-recomp tool"          ON)
option(J2ME_BUILD_RUNTIME      "Build j2me-runtime library"          ON)
option(J2ME_BUILD_GAME         "Build the recompiled game executable" ON)
option(J2ME_RUN_RECOMP         "Run recomp step automatically"       ON)

set(J2ME_GAME_TOML "${CMAKE_SOURCE_DIR}/game.toml" CACHE FILEPATH
    "Path to game TOML config")

# ── Parse game.toml ───────────────────────────────────────────────────────────
include(cmake/ParseGameToml.cmake)
parse_game_toml("${J2ME_GAME_TOML}")
# Sets: GAME_NAME  GAME_JAR  GAME_MAIN_CLASS  GAME_OUTPUT_DIR

set(RECOMP_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/recomp_out")

# ── 1. j2me-recomp tool ───────────────────────────────────────────────────────
if(J2ME_BUILD_RECOMP_TOOL)
    add_subdirectory(j2me-recomp)
endif()

# ── 2. j2me-runtime ───────────────────────────────────────────────────────────
if(J2ME_BUILD_RUNTIME)
    add_subdirectory(j2me-runtime)
endif()

# ── 3. Recompilation step: JAR → C++ sources ──────────────────────────────────
if(J2ME_RUN_RECOMP AND J2ME_BUILD_RECOMP_TOOL)
    file(MAKE_DIRECTORY "${RECOMP_OUTPUT_DIR}")
    add_custom_command(
        OUTPUT  "${RECOMP_OUTPUT_DIR}/.recomp_stamp"
        COMMAND "$<TARGET_FILE:j2me_recomp_tool>"
                --jar    "${GAME_JAR}"
                --main   "${GAME_MAIN_CLASS}"
                --toml   "${J2ME_GAME_TOML}"
                --output "${RECOMP_OUTPUT_DIR}"
        COMMAND ${CMAKE_COMMAND} -E touch "${RECOMP_OUTPUT_DIR}/.recomp_stamp"
        DEPENDS j2me_recomp_tool "${J2ME_GAME_TOML}"
        COMMENT "Recompiling ${GAME_JAR} → C++ sources"
        VERBATIM
    )
    add_custom_target(run_recomp ALL
        DEPENDS "${RECOMP_OUTPUT_DIR}/.recomp_stamp")
endif()

# ── 4. Game executable ────────────────────────────────────────────────────────
if(J2ME_BUILD_GAME)
    # Collect generated sources
    file(GLOB RECOMP_SOURCES
        "${RECOMP_OUTPUT_DIR}/*.cpp"
    )
    # Hand-written entry point
    set(GAME_ENTRY "${CMAKE_SOURCE_DIR}/game_src/game_main.cpp")
    if(NOT EXISTS "${GAME_ENTRY}")
        set(GAME_ENTRY "")
    endif()

    set(ALL_GAME_SOURCES ${RECOMP_SOURCES} ${GAME_ENTRY})

    if(ALL_GAME_SOURCES)
        add_executable(${GAME_NAME} ${ALL_GAME_SOURCES})
        target_include_directories(${GAME_NAME} PRIVATE
            "${CMAKE_SOURCE_DIR}/j2me-runtime/include"
            "${RECOMP_OUTPUT_DIR}"
        )
        target_link_libraries(${GAME_NAME} PRIVATE j2me_runtime)
        target_compile_features(${GAME_NAME} PRIVATE cxx_std_17)

        if(J2ME_RUN_RECOMP)
            add_dependencies(${GAME_NAME} run_recomp)
        endif()

        include(cmake/ApplyPatches.cmake)
        apply_game_patches(${GAME_NAME} "${J2ME_GAME_TOML}")

        if(WIN32)
            target_compile_definitions(${GAME_NAME} PRIVATE
                _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
        endif()

        message(STATUS "[J2MERecomp] Game sources: ${ALL_GAME_SOURCES}")
    else()
        message(STATUS
            "[J2MERecomp] No recomp sources found in ${RECOMP_OUTPUT_DIR} — "
            "run 'make recomp' then re-run cmake.")
    endif()
endif()
