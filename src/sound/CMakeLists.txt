if (NOT OPTION_DEDICATED)
    add_files(
        allegro_s.cpp
        allegro_s.h
        CONDITION Allegro_FOUND
    )

    add_files(
        sdl_s.cpp
        sdl_s.h
        CONDITION SDL_FOUND
    )

    add_files(
        sdl2_s.cpp
        sdl_s.h
        CONDITION SDL2_FOUND
    )

    add_files(
        cocoa_s.cpp
        cocoa_s.h
        CONDITION APPLE
    )

    add_files(
        win32_s.cpp
        win32_s.h
        CONDITION WIN32
    )

    add_files(
        xaudio2_s.cpp
        xaudio2_s.h
        CONDITION WIN32 AND XAUDIO2_FOUND
    )
endif (NOT OPTION_DEDICATED)

add_files(
    sound_driver.hpp
    null_s.cpp
    null_s.h
)
