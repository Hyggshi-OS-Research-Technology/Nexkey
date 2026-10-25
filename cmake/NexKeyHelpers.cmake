# Shared helper functions for the NexKey build.

# Configure a target that talks to a GObject / GLib C API.
#
# The C libraries rely on C style casts and implicit conversions, so the cast,
# conversion and shadow warnings used for the core library are not enabled.
function(nexkey_configure_gobject_target target)
    target_compile_features(${target} PUBLIC cxx_std_${NEXKEY_CXX_STANDARD})
    set_target_properties(${target} PROPERTIES
        CXX_EXTENSIONS OFF
        CXX_STANDARD_REQUIRED ON
    )

    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wno-deprecated-declarations)
    endif()

    if(NEXKEY_WARNINGS_AS_ERRORS)
        if(MSVC)
            target_compile_options(${target} PRIVATE /WX)
        else()
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

# Configure a target with the project language level and warning set.
function(nexkey_configure_target target)
    target_compile_features(${target} PUBLIC cxx_std_${NEXKEY_CXX_STANDARD})

    set_target_properties(${target} PROPERTIES
        CXX_EXTENSIONS OFF
        CXX_STANDARD_REQUIRED ON
    )

    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wshadow
            -Wnon-virtual-dtor
            -Wold-style-cast
            -Wcast-qual
            -Wconversion
            -Wsign-conversion
            -Wunused
        )
    endif()

    if(NEXKEY_WARNINGS_AS_ERRORS)
        if(MSVC)
            target_compile_options(${target} PRIVATE /WX)
        else()
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()
