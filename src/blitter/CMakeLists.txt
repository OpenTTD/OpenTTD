add_files(
    32bpp_anim.cpp
    32bpp_anim.hpp
    32bpp_base.cpp
    32bpp_base.hpp
    32bpp_optimized.cpp
    32bpp_optimized.hpp
    32bpp_simple.cpp
    32bpp_simple.hpp
    8bpp_base.cpp
    8bpp_base.hpp
    8bpp_optimized.cpp
    8bpp_optimized.hpp
    8bpp_simple.cpp
    8bpp_simple.hpp
    CONDITION NOT OPTION_DEDICATED
)

add_files(
    32bpp_anim_sse2.cpp
    32bpp_anim_sse2.hpp
    32bpp_anim_sse4.cpp
    32bpp_anim_sse4.hpp
    32bpp_sse2.cpp
    32bpp_sse2.hpp
    32bpp_sse4.cpp
    32bpp_sse4.hpp
    32bpp_sse_func.hpp
    32bpp_sse_type.h
    32bpp_ssse3.cpp
    32bpp_ssse3.hpp
    CONDITION NOT OPTION_DEDICATED AND SSE_FOUND
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    set_compile_flags(
        32bpp_anim_sse2.cpp
        32bpp_sse2.cpp
        COMPILE_FLAGS -msse2)
    set_compile_flags(
        32bpp_ssse3.cpp
        COMPILE_FLAGS -mssse3)
    set_compile_flags(
        32bpp_anim_sse4.cpp
        32bpp_sse4.cpp
        COMPILE_FLAGS -msse4.1)
endif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")

add_files(
    base.hpp
    common.hpp
    factory.hpp
    null.cpp
    null.hpp
)
