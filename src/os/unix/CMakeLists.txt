add_files(
    crashlog_unix.cpp
    survey_unix.cpp
    CONDITION UNIX AND NOT APPLE
)

add_files(
    library_loader_unix.cpp
    unix.cpp
    CONDITION UNIX
)

add_files(
    font_unix.cpp
    CONDITION Fontconfig_FOUND
)

if(UNIX AND NOT APPLE)
    target_sources(openttd PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/unix_main.cpp)
endif()
