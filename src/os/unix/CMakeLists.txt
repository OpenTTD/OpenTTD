add_files(
    crashlog_unix.cpp
    CONDITION UNIX AND NOT APPLE AND NOT OPTION_OS2
)

add_files(
    unix.cpp
    CONDITION UNIX AND NOT OPTION_OS2
)

add_files(
    font_unix.cpp
    CONDITION Fontconfig_FOUND
)
