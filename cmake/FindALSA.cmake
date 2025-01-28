#[=======================================================================[.rst:
FindALSA
--------

Find Advanced Linux Sound Architecture (ALSA)

Find the alsa libraries (``asound``)

IMPORTED Targets
^^^^^^^^^^^^^^^^

.. versionadded:: 3.12

This module defines :prop_tgt:`IMPORTED` target ``ALSA::ALSA``, if
ALSA has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

``ALSA_FOUND``
  True if ALSA_INCLUDE_DIR & ALSA_LIBRARY are found

``ALSA_LIBRARIES``
  List of libraries when using ALSA.

``ALSA_INCLUDE_DIRS``
  Where to find the ALSA headers.

Cache variables
^^^^^^^^^^^^^^^

The following cache variables may also be set:

``ALSA_INCLUDE_DIR``
  the ALSA include directory

``ALSA_LIBRARY``
  the absolute path of the asound library
#]=======================================================================]

find_package(PkgConfig QUIET)
pkg_check_modules(PC_ALSA QUIET alsa)

find_path(ALSA_INCLUDE_DIR NAMES alsa/asoundlib.h
          DOC "The ALSA (asound) include directory"
)

find_library(ALSA_LIBRARY NAMES asound
          DOC "The ALSA (asound) library"
)

if(ALSA_INCLUDE_DIR AND EXISTS "${ALSA_INCLUDE_DIR}/alsa/version.h")
  file(STRINGS "${ALSA_INCLUDE_DIR}/alsa/version.h" alsa_version_str REGEX "^#define[\t ]+SND_LIB_VERSION_STR[\t ]+\".*\"")

  string(REGEX REPLACE "^.*SND_LIB_VERSION_STR[\t ]+\"([^\"]*)\".*$" "\\1" ALSA_VERSION_STRING "${alsa_version_str}")
  unset(alsa_version_str)
endif()

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(ALSA
                                  REQUIRED_VARS ALSA_LIBRARY ALSA_INCLUDE_DIR
                                  VERSION_VAR ALSA_VERSION_STRING)

if(ALSA_FOUND)
  set( ALSA_LIBRARIES ${ALSA_LIBRARY} )
  set( ALSA_INCLUDE_DIRS ${ALSA_INCLUDE_DIR} )
  if(NOT TARGET ALSA::ALSA)
    add_library(ALSA::ALSA UNKNOWN IMPORTED)
    set_target_properties(ALSA::ALSA PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${ALSA_INCLUDE_DIRS}")
    set_property(TARGET ALSA::ALSA APPEND PROPERTY IMPORTED_LOCATION "${ALSA_LIBRARY}")
  endif()
endif()

mark_as_advanced(ALSA_INCLUDE_DIR ALSA_LIBRARY)
