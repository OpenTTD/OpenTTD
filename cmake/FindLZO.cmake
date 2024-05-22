#[=======================================================================[.rst:
FindLZO
-------

Finds the LZO library.

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables:

``LZO_FOUND``
  True if the system has the LZO library.
``LZO_INCLUDE_DIRS``
  Include directories needed to use LZO.
``LZO_LIBRARIES``
  Libraries needed to link to LZO.
``LZO_VERSION``
  The version of the LZO library which was found.

Cache Variables
^^^^^^^^^^^^^^^

The following cache variables may also be set:

``LZO_INCLUDE_DIR``
  The directory containing ``lzo/lzo1x.h``.
``LZO_LIBRARY``
  The path to the LZO library.

#]=======================================================================]

find_package(PkgConfig QUIET)
pkg_check_modules(PC_LZO QUIET lzo2)

find_path(LZO_INCLUDE_DIR
    NAMES lzo/lzo1x.h
    PATHS ${PC_LZO_INCLUDE_DIRS}
)

find_library(LZO_LIBRARY
    NAMES lzo2
    PATHS ${PC_LZO_LIBRARY_DIRS}
)

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
if(VCPKG_TOOLCHAIN AND LZO_LIBRARY AND LZO_LIBRARY MATCHES "${VCPKG_INSTALLED_DIR}")
    if(LZO_LIBRARY MATCHES "/debug/")
        set(LZO_LIBRARY_DEBUG ${LZO_LIBRARY})
        string(REPLACE "/debug/lib/" "/lib/" LZO_LIBRARY_RELEASE ${LZO_LIBRARY})
    else()
        set(LZO_LIBRARY_RELEASE ${LZO_LIBRARY})
        string(REPLACE "/lib/" "/debug/lib/" LZO_LIBRARY_DEBUG ${LZO_LIBRARY})
    endif()
    include(SelectLibraryConfigurations)
    select_library_configurations(LZO)
endif()

set(LZO_VERSION ${PC_LZO_VERSION})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LZO
    FOUND_VAR LZO_FOUND
    REQUIRED_VARS
        LZO_LIBRARY
        LZO_INCLUDE_DIR
    VERSION_VAR LZO_VERSION
)

if(LZO_FOUND)
    set(LZO_LIBRARIES ${LZO_LIBRARY})
    set(LZO_INCLUDE_DIRS ${LZO_INCLUDE_DIR})
endif()

mark_as_advanced(
    LZO_INCLUDE_DIR
    LZO_LIBRARY
)
