include(FindPackageHandleStandardArgs)

find_library(Soxr_LIBRARY
	NAMES soxr
)

include(FixVcpkgLibrary)
FixVcpkgLibrary(Soxr)

set(Soxr_COMPILE_OPTIONS "" CACHE STRING "Extra compile options of soxr")

set(Soxr_LINK_LIBRARIES "" CACHE STRING "Extra link libraries of soxr")

set(Soxr_LINK_FLAGS "" CACHE STRING "Extra link flags of soxr")

find_path(Soxr_INCLUDE_PATH
	NAMES soxr.h
)

find_package_handle_standard_args(Soxr
	REQUIRED_VARS Soxr_LIBRARY Soxr_INCLUDE_PATH
)

if(Soxr_FOUND)
	if(NOT TARGET Soxr::soxr)
		add_library(Soxr::soxr UNKNOWN IMPORTED)
		set_target_properties(Soxr::soxr PROPERTIES
			IMPORTED_LOCATION "${Soxr_LIBRARY}"
			INTERFACE_INCLUDE_DIRECTORIES "${Soxr_INCLUDE_PATH}"
			INTERFACE_COMPILE_OPTIONS "${Soxr_COMPILE_OPTIONS}"
			INTERFACE_LINK_LIBRARIES "${Soxr_LINK_LIBRARIES}"
			INTERFACE_LINK_FLAGS "${Soxr_LINK_FLAGS}"
		)
		FixVcpkgTarget(Soxr Soxr::soxr)

		# Prevent undefined references in UCRT64 MinGW environment
		if(MINGW AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
			set_property(TARGET Soxr::soxr APPEND PROPERTY INTERFACE_LINK_LIBRARIES gomp)
		endif()
	endif()
endif()
