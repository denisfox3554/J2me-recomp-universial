cmake_minimum_required(VERSION 3.20)

add_executable(j2me_recomp_tool
    src/main.cpp
    src/jar_loader.cpp
    src/class_parser.cpp
    src/bytecode_lifter.cpp
    src/ssa_builder.cpp
    src/cfg_builder.cpp
    src/cpp_emitter.cpp
    src/toml_config.cpp
    src/patch_applier.cpp
)

target_include_directories(j2me_recomp_tool PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(j2me_recomp_tool PRIVATE cxx_std_17)

# ── Platform specifics ────────────────────────────────────────────────────────
if(WIN32)
    target_compile_definitions(j2me_recomp_tool PRIVATE
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
    # MinGW: link minizip/zlib statically so the tool is self-contained
    target_link_options(j2me_recomp_tool PRIVATE -static-libgcc -static-libstdc++)
endif()

# ── zlib for JAR (ZIP) reading ────────────────────────────────────────────────
find_package(ZLIB REQUIRED)
target_link_libraries(j2me_recomp_tool PRIVATE ZLIB::ZLIB)

install(TARGETS j2me_recomp_tool
        RUNTIME DESTINATION bin)
