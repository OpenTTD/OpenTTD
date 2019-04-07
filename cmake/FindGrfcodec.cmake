# Autodetect grfcodec and nforenum.
#

find_program(GRFCODEC_EXECUTABLE grfcodec)
find_program(NFORENUM_EXECUTABLE nforenum)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Grfcodec
    FOUND_VAR GRFCODEC_FOUND
    REQUIRED_VARS
        GRFCODEC_EXECUTABLE
        NFORENUM_EXECUTABLE
)
