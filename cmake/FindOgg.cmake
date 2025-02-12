include(FindPackageHandleStandardArgs)

find_library(Ogg_LIBRARY
    NAMES ogg
)

include(FixVcpkgLibrary)
FixVcpkgLibrary(Ogg)

set(Ogg_COMPILE_OPTIONS "" CACHE STRING "Extra compile options of ogg")

set(Ogg_LINK_LIBRARIES "" CACHE STRING "Extra link libraries of ogg")

set(Ogg_LINK_FLAGS "" CACHE STRING "Extra link flags of ogg")

find_path(Ogg_INCLUDE_PATH
    NAMES ogg.h
    PATH_SUFFIXES ogg
)

find_package_handle_standard_args(Ogg
    REQUIRED_VARS Ogg_LIBRARY Ogg_INCLUDE_PATH
)

if (Ogg_FOUND)
    set(Ogg_dirs ${Ogg_INCLUDE_PATH})
    if(EXISTS "${Ogg_INCLUDE_PATH}/ogg")
        list(APPEND Ogg_dirs "${Ogg_INCLUDE_PATH}/ogg")
    endif()
    if (NOT TARGET Ogg::ogg)
        add_library(Ogg::ogg UNKNOWN IMPORTED)
        set_target_properties(Ogg::ogg PROPERTIES
            IMPORTED_LOCATION "${Ogg_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${Ogg_dirs}"
            INTERFACE_COMPILE_OPTIONS "${Ogg_COMPILE_OPTIONS}"
            INTERFACE_LINK_LIBRARIES "${Ogg_LINK_LIBRARIES}"
            INTERFACE_LINK_FLAGS "${Ogg_LINK_FLAGS}"
        )
        FixVcpkgTarget(Ogg Ogg::ogg)
    endif()
endif()
