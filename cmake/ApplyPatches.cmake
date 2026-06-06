# cmake/ApplyPatches.cmake
# Reads [build] extra_cxx_flags and link_libraries from game.toml,
# applies them to the game target.

function(apply_game_patches TARGET TOML_PATH)
    if(NOT EXISTS "${TOML_PATH}")
        return()
    endif()

    file(READ "${TOML_PATH}" _content)
    string(REPLACE "\r\n" "\n" _content "${_content}")

    # Extract extra_cxx_flags array values
    string(REGEX MATCHALL "\"(-[^\"]+)\"" _flag_matches "${_content}")
    foreach(_m IN LISTS _flag_matches)
        string(REGEX REPLACE "\"(.+)\"" "\\1" _flag "${_m}")
        # Only flags starting with - (skip library names)
        if(_flag MATCHES "^-")
            target_compile_options(${TARGET} PRIVATE "${_flag}")
        endif()
    endforeach()

    # Extract link_libraries array: values that don't start with -
    string(REGEX MATCH
        "link_libraries[ \t]*=[ \t]*\\[([^\\]]+)\\]"
        _libs_block "${_content}")
    if(CMAKE_MATCH_1)
        string(REGEX MATCHALL "\"([^\"]+)\"" _lib_matches "${CMAKE_MATCH_1}")
        foreach(_lm IN LISTS _lib_matches)
            string(REGEX REPLACE "\"(.+)\"" "\\1" _lib "${_lm}")
            if(NOT _lib MATCHES "^-")
                target_link_libraries(${TARGET} PRIVATE "${_lib}")
                message(STATUS "[ApplyPatches] link: ${_lib}")
            endif()
        endforeach()
    endif()
endfunction()
