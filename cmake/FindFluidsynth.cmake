#[=======================================================================[.rst:
FindFluidsynth
-------

Finds the fluidsynth library.

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables:

``Fluidsynth_FOUND``
  True if the system has the fluidsynth library.
``Fluidsynth_INCLUDE_DIRS``
  Include directories needed to use fluidsynth.
``Fluidsynth_LIBRARIES``
  Libraries needed to link to fluidsynth.
``Fluidsynth_VERSION``
  The version of the fluidsynth library which was found.

Cache Variables
^^^^^^^^^^^^^^^

The following cache variables may also be set:

``Fluidsynth_INCLUDE_DIR``
  The directory containing ``fluidsynth.h``.
``Fluidsynth_LIBRARY``
  The path to the fluidsynth library.

#]=======================================================================]

find_package(PkgConfig QUIET)
pkg_check_modules(PC_Fluidsynth QUIET fluidsynth)

find_path(Fluidsynth_INCLUDE_DIR
    NAMES fluidsynth.h
    PATHS ${PC_Fluidsynth_INCLUDE_DIRS}
)

find_library(Fluidsynth_LIBRARY
    NAMES fluidsynth
    PATHS ${PC_Fluidsynth_LIBRARY_DIRS}
)

set(Fluidsynth_VERSION ${PC_Fluidsynth_VERSION})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Fluidsynth
    FOUND_VAR Fluidsynth_FOUND
    REQUIRED_VARS
        Fluidsynth_LIBRARY
        Fluidsynth_INCLUDE_DIR
    VERSION_VAR Fluidsynth_VERSION
)

if (Fluidsynth_FOUND)
    set(Fluidsynth_LIBRARIES ${Fluidsynth_LIBRARY})
    set(Fluidsynth_INCLUDE_DIRS ${Fluidsynth_INCLUDE_DIR})
endif ()

mark_as_advanced(
    Fluidsynth_INCLUDE_DIR
    Fluidsynth_LIBRARY
)
