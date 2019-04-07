# Autodetect if SSE4.1 can be used. If so, the assumption is, so can the other
# SSE version (SSE 2.0, SSSE 3.0).

include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_FLAGS "")

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    set(CMAKE_REQUIRED_FLAGS "-msse4.1")
endif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")

check_cxx_source_compiles("
    #include <xmmintrin.h>
    #include <smmintrin.h>
    #include <tmmintrin.h>
    int main() { return 0; }"
    SSE_FOUND
)
