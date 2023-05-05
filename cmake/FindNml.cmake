# Autodetect nml.
#

find_program(NML_EXECUTABLE nmlc)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Nml
    FOUND_VAR NML_FOUND
    REQUIRED_VARS
        NML_EXECUTABLE
)
