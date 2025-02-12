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

include(FixVcpkgLibrary)
FixVcpkgLibrary(LZO)

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
