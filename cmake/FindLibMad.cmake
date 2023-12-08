#  LIBMAD_FOUND
#  LIBMAD_INCLUDE_DIR
#  LIBMAD_LIBRARY

find_path(LibMad_INCLUDE_DIR NAMES mad.h)

find_library(LibMad_LIBRARY NAMES mad)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibMad DEFAULT_MSG LibMad_LIBRARY LibMad_INCLUDE_DIR)

mark_as_advanced(LibMad_INCLUDE_DIR LibMad_LIBRARY)
