include(FindPackageHandleStandardArgs)

find_library(OpusFile_LIBRARY
    NAMES opusfile
)

include(FixVcpkgLibrary)
FixVcpkgLibrary(OpusFile)

set(OpusFile_COMPILE_OPTIONS "" CACHE STRING "Extra compile options of opusfile")

set(OpusFile_LINK_LIBRARIES "" CACHE STRING "Extra link libraries of opusfile")

set(OpusFile_LINK_FLAGS "" CACHE STRING "Extra link flags of opusfile")

find_path(OpusFile_INCLUDE_PATH
    NAMES opusfile.h
    PATH_SUFFIXES opus
)

find_package_handle_standard_args(OpusFile
    REQUIRED_VARS OpusFile_LIBRARY OpusFile_INCLUDE_PATH
)

find_package(Ogg)
find_package(Opus)

if (OpusFile_FOUND)
    set(OpusFile_dirs ${OpusFile_INCLUDE_PATH})
    if(EXISTS "${OpusFile_INCLUDE_PATH}/opus")
        list(APPEND OpusFile_dirs "${OpusFile_INCLUDE_PATH}/opus")
    endif()
    if (NOT TARGET OpusFile::opusfile)
        add_library(OpusFile::opusfile UNKNOWN IMPORTED)
        set_target_properties(OpusFile::opusfile PROPERTIES
            IMPORTED_LOCATION "${OpusFile_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${OpusFile_dirs}"
            INTERFACE_COMPILE_OPTIONS "${OpusFile_COMPILE_OPTIONS}"
            INTERFACE_LINK_LIBRARIES "Ogg::ogg;Opus::opus;${OpusFile_LINK_LIBRARIES}"
            INTERFACE_LINK_FLAGS "${OpusFile_LINK_FLAGS}"
        )
        FixVcpkgTarget(OpusFile OpusFile::opusfile)
    endif()
endif()
