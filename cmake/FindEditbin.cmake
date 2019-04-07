# Autodetect editbin. Only useful for MSVC.

get_filename_component(MSVC_COMPILE_DIRECTORY ${CMAKE_CXX_COMPILER} DIRECTORY)
find_program(
    EDITBIN_EXECUTABLE editbin.exe
    HINTS ${MSVC_COMPILE_DIRECTORY}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Editbin
    FOUND_VAR EDITBIN_FOUND
    REQUIRED_VARS EDITBIN_EXECUTABLE
)
