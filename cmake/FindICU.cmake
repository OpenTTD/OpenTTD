# CMake provides a FindICU module since version 3.7.
# But it doesn't use pkgconfig, doesn't set expected variables,
# And it returns incomplete dependencies if only some modules are searched.


#[=======================================================================[.rst:
FindICU
-------

Finds components of the ICU library.

Accepted components are: uc, i18n, le, lx, io, data

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables:

``ICU_FOUND``
  True if components of ICU library are found.
``ICU_VERSION``
  The version of the ICU library which was found.
``ICU_<c>_FOUND``
  True if the system has the <c> component of ICU library.
``ICU_<c>_INCLUDE_DIRS``
  Include directories needed to use the <c> component of ICU library.
``ICU_<c>_LIBRARIES``
  Libraries needed to link to the <c> component of ICU library.

#]=======================================================================]

find_package(PkgConfig QUIET)

set(ICU_KNOWN_COMPONENTS "uc" "i18n" "le" "lx" "io" "data")

foreach(MOD_NAME IN LISTS ICU_FIND_COMPONENTS)
    if(NOT MOD_NAME IN_LIST ICU_KNOWN_COMPONENTS)
        message(FATAL_ERROR "Unknown ICU component: ${MOD_NAME}")
    endif()
    pkg_check_modules(PC_ICU_${MOD_NAME} QUIET icu-${MOD_NAME})

    # Check the libraries returned by pkg-config really exist.
    unset(PC_LIBRARIES)
    foreach(LIBRARY IN LISTS PC_ICU_${MOD_NAME}_LIBRARIES)
        unset(PC_LIBRARY CACHE)
        find_library(PC_LIBRARY NAMES ${LIBRARY})
        if(NOT PC_LIBRARY)
            unset(PC_ICU_${MOD_NAME}_FOUND)
        endif()
        list(APPEND PC_LIBRARIES ${PC_LIBRARY})
    endforeach()
    unset(PC_LIBRARY CACHE)

    if(${PC_ICU_${MOD_NAME}_FOUND})
        set(ICU_COMPONENT_FOUND TRUE)
        set(ICU_${MOD_NAME}_FOUND TRUE)
        set(ICU_${MOD_NAME}_LIBRARIES ${PC_LIBRARIES})
        set(ICU_${MOD_NAME}_INCLUDE_DIRS ${PC_ICU_${MOD_NAME}_INCLUDE_DIRS})
        set(ICU_VERSION ${PC_ICU_${MOD_NAME}_VERSION})
    endif()
endforeach()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ICU
    FOUND_VAR ICU_FOUND
    REQUIRED_VARS ICU_COMPONENT_FOUND
    VERSION_VAR ICU_VERSION
    HANDLE_COMPONENTS
)
