add_files(
    32bpp_anim.cpp
    32bpp_anim.h
    32bpp_base.cpp
    32bpp_base.h
    32bpp_optimized.cpp
    32bpp_optimized.h
    32bpp_simple.cpp
    32bpp_simple.h
    8bpp_base.cpp
    8bpp_base.h
    8bpp_optimized.cpp
    8bpp_optimized.h
    8bpp_simple.cpp
    8bpp_simple.h
    CONDITION NOT OPTION_DEDICATED
)

add_files(
    32bpp_anim_sse2.cpp
    32bpp_anim_sse2.h
    32bpp_anim_sse4.cpp
    32bpp_anim_sse4.h
    32bpp_sse2.cpp
    32bpp_sse2.h
    32bpp_sse4.cpp
    32bpp_sse4.h
    32bpp_sse_func.h
    32bpp_sse_type.h
    32bpp_ssse3.cpp
    32bpp_ssse3.h
    CONDITION NOT OPTION_DEDICATED AND SSE_FOUND
)

add_files(
    40bpp_anim.cpp
    40bpp_anim.h
    CONDITION NOT OPTION_DEDICATED AND OPENGL_FOUND
)

add_files(
    base.h
    common.h
    factory.h
    null.cpp
    null.h
)
