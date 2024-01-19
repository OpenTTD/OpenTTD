include(GNUInstallDirs)

# Set the options for the directories (personal, shared, global).
#
# set_directory_options()
#
function(set_directory_options)
    if(APPLE)
        set(DEFAULT_PERSONAL_DIR "Documents/OpenTTD")
        set(DEFAULT_SHARED_DIR "/Library/Application Support/OpenTTD")
        set(DEFAULT_GLOBAL_DIR "(not set)")
    elseif(WIN32)
        set(DEFAULT_PERSONAL_DIR "OpenTTD")
        set(DEFAULT_SHARED_DIR "(not set)")
        set(DEFAULT_GLOBAL_DIR "(not set)")
    elseif(UNIX)
        set(DEFAULT_PERSONAL_DIR ".${BINARY_NAME}")
        set(DEFAULT_SHARED_DIR "(not set)")
        set(DEFAULT_GLOBAL_DIR "${CMAKE_INSTALL_FULL_DATADIR}/${BINARY_NAME}")
    else()
        message(FATAL_ERROR "Unknown OS found; please consider creating a Pull Request to add support for this OS.")
    endif()

    if(NOT PERSONAL_DIR)
        set(PERSONAL_DIR "${DEFAULT_PERSONAL_DIR}" CACHE STRING "Personal directory")
        message(STATUS "Detecting Personal Data directory - ${PERSONAL_DIR}")
    endif()

    if(NOT SHARED_DIR)
        set(SHARED_DIR "${DEFAULT_SHARED_DIR}" CACHE STRING "Shared directory")
        message(STATUS "Detecting Shared Data directory - ${SHARED_DIR}")
    endif()

    if(NOT GLOBAL_DIR)
        set(GLOBAL_DIR "${DEFAULT_GLOBAL_DIR}" CACHE STRING "Global directory")
        message(STATUS "Detecting Global Data directory - ${GLOBAL_DIR}")
    endif()

    set(HOST_BINARY_DIR "" CACHE PATH "Full path to native cmake build directory")
endfunction()

# Set some generic options that influence what is being build.
#
# set_options()
#
function(set_options)
    option(OPTION_PACKAGE_DEPENDENCIES "Copy dependencies into lib/ for easy packaging (Linux only)" OFF)

    if(UNIX AND NOT APPLE AND NOT OPTION_PACKAGE_DEPENDENCIES)
        set(DEFAULT_OPTION_INSTALL_FHS ON)
    else()
        set(DEFAULT_OPTION_INSTALL_FHS OFF)
    endif()

    option(OPTION_FORCE_COLORED_OUTPUT "Always produce ANSI-colored output (GNU/Clang only)." OFF)

    option(OPTION_DEDICATED "Build dedicated server only (no GUI)" OFF)
    option(OPTION_INSTALL_FHS "Install with Filesystem Hierarchy Standard folders" ${DEFAULT_OPTION_INSTALL_FHS})
    option(OPTION_USE_ASSERTS "Use assertions; leave enabled for nightlies, betas, and RCs" ON)
    if(EMSCRIPTEN)
        # Although pthreads is supported, it is not in a way yet that is
        # useful for us.
        option(OPTION_USE_THREADS "Use threads" OFF)
    else()
        option(OPTION_USE_THREADS "Use threads" ON)
    endif()
    option(OPTION_USE_NSIS "Use NSIS to create windows installer; enable only for stable releases" OFF)
    option(OPTION_TOOLS_ONLY "Build only tools target" OFF)
    option(OPTION_DOCS_ONLY "Build only docs target" OFF)
    option(OPTION_ALLOW_INVALID_SIGNATURE "Allow loading of content with invalid signatures" OFF)

    if (OPTION_DOCS_ONLY)
        set(OPTION_TOOLS_ONLY ON PARENT_SCOPE)
    endif()

    option(OPTION_SURVEY_KEY "Survey-key to use for the opt-in survey (empty if you have none)" "")
endfunction()

# Show the values of the generic options.
#
# show_options()
#
function(show_options)
    message(STATUS "Option Package Dependencies - ${OPTION_PACKAGE_DEPENDENCIES}")
    message(STATUS "Option Dedicated - ${OPTION_DEDICATED}")
    message(STATUS "Option Install FHS - ${OPTION_INSTALL_FHS}")
    message(STATUS "Option Use assert - ${OPTION_USE_ASSERTS}")
    message(STATUS "Option Use threads - ${OPTION_USE_THREADS}")
    message(STATUS "Option Use NSIS - ${OPTION_USE_NSIS}")

    if(OPTION_SURVEY_KEY)
        message(STATUS "Option Survey Key - USED")
    else()
        message(STATUS "Option Survey Key - NOT USED")
    endif()

    if(OPTION_ALLOW_INVALID_SIGNATURE)
        message(STATUS "Option Allow Invalid Signature - USED")
        message(WARNING "Ignoring invalid signatures is a security risk! Use with care!")
    endif()
endfunction()

# Add the definitions for the options that are selected.
#
# add_definitions_based_on_options()
#
function(add_definitions_based_on_options)
    if(OPTION_DEDICATED)
        add_definitions(-DDEDICATED)
    endif()

    if(NOT OPTION_USE_THREADS)
        add_definitions(-DNO_THREADS)
    endif()

    if(OPTION_USE_ASSERTS)
        add_definitions(-DWITH_ASSERT)
    else()
        add_definitions(-DNDEBUG)
    endif()

    if(OPTION_SURVEY_KEY)
        add_definitions(-DSURVEY_KEY="${OPTION_SURVEY_KEY}")
    endif()

    if(OPTION_ALLOW_INVALID_SIGNATURE)
        add_definitions(-DALLOW_INVALID_SIGNATURE)
    endif()
endfunction()
