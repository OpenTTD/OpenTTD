# Autodetect editbin. Only useful for MSVC.

if(NOT EDITBIN_DIRECTORY)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        get_filename_component(MSVC_COMPILE_DIRECTORY ${CMAKE_CXX_COMPILER} DIRECTORY)
        set(EDITBIN_DIRECTORY ${MSVC_COMPILE_DIRECTORY})
    else()
        # For clang-cl build
        # find editbin.exe from environmental variable VCToolsInstallDir
        set(EDITBIN_DIRECTORY "$ENV{VCToolsInstallDir}/bin/Hostx64/x64")
    endif()
endif()

message(CHECK_START "Finding editbin.exe")
find_program(
    EDITBIN_EXECUTABLE editbin.exe
    HINTS ${EDITBIN_DIRECTORY}
)

if(EDITBIN_EXECUTABLE)
    message(CHECK_PASS "found")
else()
    message(CHECK_FAIL "not found , please manually specify EDITBIN_DIRECTORY")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Editbin
    FOUND_VAR EDITBIN_FOUND
    REQUIRED_VARS EDITBIN_EXECUTABLE
)
