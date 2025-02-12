include(FindPackageHandleStandardArgs)

find_library(Opus_LIBRARY
    NAMES opus
)

include(FixVcpkgLibrary)
FixVcpkgLibrary(Opus)

set(Opus_COMPILE_OPTIONS "" CACHE STRING "Extra compile options of opus")

set(Opus_LINK_LIBRARIES "" CACHE STRING "Extra link libraries of opus")

set(Opus_LINK_FLAGS "" CACHE STRING "Extra link flags of opus")

find_path(Opus_INCLUDE_PATH
    NAMES opus.h
    PATH_SUFFIXES opus
)

find_package_handle_standard_args(Opus
    REQUIRED_VARS Opus_LIBRARY Opus_INCLUDE_PATH
)

if (Opus_FOUND)
    set(Opus_dirs ${Opus_INCLUDE_PATH})
    if(EXISTS "${Opus_INCLUDE_PATH}/opus")
        list(APPEND Opus_dirs "${Opus_INCLUDE_PATH}/opus")
    endif()
    if (NOT TARGET Opus::opus)
        add_library(Opus::opus UNKNOWN IMPORTED)
        set_target_properties(Opus::opus PROPERTIES
            IMPORTED_LOCATION "${Opus_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${Opus_dirs}"
            INTERFACE_COMPILE_OPTIONS "${Opus_COMPILE_OPTIONS}"
            INTERFACE_LINK_LIBRARIES "${Opus_LINK_LIBRARIES}"
            INTERFACE_LINK_FLAGS "${Opus_LINK_FLAGS}"
        )
        FixVcpkgTarget(Opus Opus::opus)
    endif()
endif()
