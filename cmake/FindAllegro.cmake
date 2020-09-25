#[=======================================================================[.rst:
FindAllegro
-------

Finds the allegro library.

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables:

``Allegro_FOUND``
  True if the system has the allegro library.
``Allegro_INCLUDE_DIRS``
  Include directories needed to use allegro.
``Allegro_LIBRARIES``
  Libraries needed to link to allegro.
``Allegro_VERSION``
  The version of the allegro library which was found.

Cache Variables
^^^^^^^^^^^^^^^

The following cache variables may also be set:

``Allegro_INCLUDE_DIR``
  The directory containing ``allegro.h``.
``Allegro_LIBRARY``
  The path to the allegro library.

#]=======================================================================]

find_package(PkgConfig QUIET)
pkg_check_modules(PC_Allegro QUIET allegro)

find_path(Allegro_INCLUDE_DIR
    NAMES allegro.h
    PATHS ${PC_Allegro_INCLUDE_DIRS}
)

find_library(Allegro_LIBRARY
    NAMES alleg
    PATHS ${PC_Allegro_LIBRARY_DIRS}
)

set(Allegro_VERSION ${PC_Allegro_VERSION})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Allegro
    FOUND_VAR Allegro_FOUND
    REQUIRED_VARS
        Allegro_LIBRARY
        Allegro_INCLUDE_DIR
    VERSION_VAR Allegro_VERSION
)

if(Allegro_FOUND)
    set(Allegro_LIBRARIES ${Allegro_LIBRARY})
    set(Allegro_INCLUDE_DIRS ${Allegro_INCLUDE_DIR})
endif()

mark_as_advanced(
    Allegro_INCLUDE_DIR
    Allegro_LIBRARY
)
