# Shared helper functions for the NexKey build.

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
