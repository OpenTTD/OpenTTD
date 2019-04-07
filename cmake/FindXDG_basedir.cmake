#[=======================================================================[.rst:
FindXDG_basedir
-------

Finds the xdg-basedir library.

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables:

``XDG_basedir_FOUND``
  True if the system has the xdg-basedir library.
``XDG_basedir_INCLUDE_DIRS``
  Include directories needed to use xdg-basedir.
``XDG_basedir_LIBRARIES``
  Libraries needed to link to xdg-basedir.
``XDG_basedir_VERSION``
  The version of the xdg-basedir library which was found.

Cache Variables
^^^^^^^^^^^^^^^

The following cache variables may also be set:

``XDG_basedir_INCLUDE_DIR``
  The directory containing ``xdg-basedir.h``.
``XDG_basedir_LIBRARY``
  The path to the xdg-basedir library.

#]=======================================================================]

find_package(PkgConfig QUIET)
pkg_check_modules(PC_XDG_basedir QUIET libxdg-basedir)

find_path(XDG_basedir_INCLUDE_DIR
    NAMES basedir.h
    PATHS ${PC_XDG_basedir_INCLUDE_DIRS}
)

find_library(XDG_basedir_LIBRARY
    NAMES xdg-basedir
    PATHS ${PC_XDG_basedir_LIBRARY_DIRS}
)

set(XDG_basedir_VERSION ${PC_XDG_basedir_VERSION})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(XDG_basedir
    FOUND_VAR XDG_basedir_FOUND
    REQUIRED_VARS
        XDG_basedir_LIBRARY
        XDG_basedir_INCLUDE_DIR
    VERSION_VAR XDG_basedir_VERSION
)

if (XDG_basedir_FOUND)
    set(XDG_basedir_LIBRARIES ${XDG_basedir_LIBRARY})
    set(XDG_basedir_INCLUDE_DIRS ${XDG_basedir_INCLUDE_DIR})
endif ()

mark_as_advanced(
    XDG_basedir_INCLUDE_DIR
    XDG_basedir_LIBRARY
)
