macro(FixVcpkgLibrary NAME)
    # With vcpkg, the library path should contain both 'debug' and 'optimized'
    # entries (see target_link_libraries() documentation for more information)
    #
    # NOTE: we only patch up when using vcpkg; the same issue might happen
    # when not using vcpkg, but this is non-trivial to fix, as we have no idea
    # what the paths are. With vcpkg we do. And we only official support vcpkg
    # with Windows.
    #
    # NOTE: this is based on the assumption that the debug file has the same
    # name as the optimized file. This is not always the case, but so far
    # experiences has shown that in those case vcpkg CMake files do the right
    # thing.
    if(VCPKG_TOOLCHAIN AND ${NAME}_LIBRARY AND ${NAME}_LIBRARY MATCHES "${VCPKG_INSTALLED_DIR}")
        if(${NAME}_LIBRARY MATCHES "/debug/")
            set(${NAME}_LIBRARY_DEBUG ${${NAME}_LIBRARY})
            string(REPLACE "/debug/lib/" "/lib/" ${NAME}_LIBRARY_RELEASE ${${NAME}_LIBRARY})
        else()
            set(${NAME}_LIBRARY_RELEASE ${${NAME}_LIBRARY})
            string(REPLACE "/lib/" "/debug/lib/" ${NAME}_LIBRARY_DEBUG ${${NAME}_LIBRARY})
        endif()
        include(SelectLibraryConfigurations)
        select_library_configurations(${NAME})
    endif()
endmacro()

function(FixVcpkgTarget NAME TARGET)
    if(EXISTS "${${NAME}_LIBRARY_RELEASE}")
        set_property(TARGET ${TARGET} APPEND PROPERTY
        IMPORTED_CONFIGURATIONS RELEASE)
        set_target_properties(${TARGET} PROPERTIES
        IMPORTED_LOCATION_RELEASE "${${NAME}_LIBRARY_RELEASE}")
    endif()
    if(EXISTS "${${NAME}_LIBRARY_DEBUG}")
        set_property(TARGET ${TARGET} APPEND PROPERTY
        IMPORTED_CONFIGURATIONS DEBUG)
        set_target_properties(${TARGET} PROPERTIES
        IMPORTED_LOCATION_DEBUG "${${NAME}_LIBRARY_DEBUG}")
    endif()
endfunction()
