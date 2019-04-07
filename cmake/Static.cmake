# Set static linking if the platform requires it.
#
# set_static()
#
function(set_static_if_needed)
     if (MINGW)
        # Let exectutables run outside MinGW environment
        # Force searching static libs
        set(CMAKE_FIND_LIBRARY_SUFFIXES ".a" PARENT_SCOPE)

        # Force static linking
        link_libraries(-static -static-libgcc -static-libstdc++)
    endif()
endfunction()
