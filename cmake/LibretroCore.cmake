# LibretroCore.cmake - CMake configuration for building OpenTTD as a libretro core
#
# This file provides the configuration needed to build OpenTTD as a libretro core
# that can be loaded by RetroArch or other libretro frontends.
#
# Usage:
#   cmake -DLIBRETRO=ON ..
#
# This will produce a shared library (openttd_libretro.dll/so/dylib) that can be
# loaded as a libretro core.

option(LIBRETRO "Build OpenTTD as a libretro core" OFF)

if(LIBRETRO)
    message(STATUS "Building OpenTTD as a libretro core")

    # Define WITH_LIBRETRO for conditional compilation
    add_compile_definitions(WITH_LIBRETRO)
    add_compile_definitions(LIBRETRO_CORE_BUILD)

    # Disable features not needed/supported in libretro mode
    set(OPTION_DEDICATED OFF CACHE BOOL "Disable dedicated server for libretro" FORCE)
    set(PERSONAL_DIR "" CACHE STRING "Disable personal dir for libretro" FORCE)

    # Set the output name for the libretro core
    set(LIBRETRO_CORE_NAME "openttd_libretro" CACHE STRING "Name of the libretro core library")

    # Platform-specific settings
    if(WIN32)
        set(LIBRETRO_CORE_EXTENSION ".dll")
    elseif(APPLE)
        set(LIBRETRO_CORE_EXTENSION ".dylib")
    else()
        set(LIBRETRO_CORE_EXTENSION ".so")
    endif()

    message(STATUS "Libretro core will be named: ${LIBRETRO_CORE_NAME}${LIBRETRO_CORE_EXTENSION}")
endif()

# Function to configure the target as a libretro core
function(configure_libretro_core TARGET_NAME)
    if(NOT LIBRETRO)
        return()
    endif()

    message(STATUS "Configuring ${TARGET_NAME} as libretro core")

    # Set output name
    set_target_properties(${TARGET_NAME} PROPERTIES
        OUTPUT_NAME ${LIBRETRO_CORE_NAME}
        PREFIX ""
    )

    # Make sure it's a shared library
    set_target_properties(${TARGET_NAME} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/libretro"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/libretro"
    )

    # Platform-specific linker settings
    if(WIN32)
        # Export all symbols on Windows
        set_target_properties(${TARGET_NAME} PROPERTIES
            WINDOWS_EXPORT_ALL_SYMBOLS ON
        )
    elseif(APPLE)
        # macOS specific settings
        set_target_properties(${TARGET_NAME} PROPERTIES
            MACOSX_RPATH ON
        )
    else()
        # Linux/Unix - ensure symbols are exported
        target_link_options(${TARGET_NAME} PRIVATE
            -Wl,--no-undefined
        )
    endif()

    # Add include directory for libretro headers
    target_include_directories(${TARGET_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/src/video/libretro
    )
endfunction()

# Function to create a standalone libretro core target
function(add_libretro_core)
    if(NOT LIBRETRO)
        return()
    endif()

    # This function would be called from the main CMakeLists.txt
    # after all source files have been collected
    message(STATUS "Creating libretro core target")
endfunction()
