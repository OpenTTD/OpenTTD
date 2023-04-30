#[=======================================================================[.rst:
FindHarfBuzz
-------

Finds the harfbuzz library.

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables:

``Harfbuzz_FOUND``
  True if the system has the harfbuzz library.
``Harfbuzz_INCLUDE_DIRS``
  Include directories needed to use harfbuzz.
``Harfbuzz_LIBRARIES``
  Libraries needed to link to harfbuzz.
``Harfbuzz_VERSION``
  The version of the harfbuzz library which was found.

Cache Variables
^^^^^^^^^^^^^^^

The following cache variables may also be set:

``Harfbuzz_INCLUDE_DIR``
  The directory containing ``hb.h``.
``Harfbuzz_LIBRARY``
  The path to the harfbuzz library.

#]=======================================================================]

find_package(PkgConfig QUIET)
pkg_check_modules(PC_Harfbuzz QUIET harfbuzz)

find_path(Harfbuzz_INCLUDE_DIR
    NAMES hb.h
    PATHS ${PC_Harfbuzz_INCLUDE_DIRS}
)

find_library(Harfbuzz_LIBRARY
    NAMES harfbuzz
    PATHS ${PC_Harfbuzz_LIBRARY_DIRS}
)

set(Harfbuzz_VERSION ${PC_Harfbuzz_VERSION})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Harfbuzz
    FOUND_VAR Harfbuzz_FOUND
    REQUIRED_VARS
        Harfbuzz_LIBRARY
        Harfbuzz_INCLUDE_DIR
    VERSION_VAR Harfbuzz_VERSION
)

if(Harfbuzz_FOUND)
    set(Harfbuzz_LIBRARIES ${Harfbuzz_LIBRARY})
    set(Harfbuzz_INCLUDE_DIRS ${Harfbuzz_INCLUDE_DIR})
endif()

mark_as_advanced(
    Harfbuzz_INCLUDE_DIR
    Harfbuzz_LIBRARY
)
