# CMakeLists.txt for libretro video driver

add_files(
    libretro.h
    libretro_core.cpp
    libretro_core.h
    libretro_v.cpp
    libretro_v.h
    CONDITION OPTION_LIBRETRO
)
