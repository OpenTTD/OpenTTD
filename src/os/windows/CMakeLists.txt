add_files(
    crashlog_win.cpp
    font_win32.cpp
    font_win32.h
    string_uniscribe.cpp
    string_uniscribe.h
    social_win.cpp
    survey_win.cpp
    win32.cpp
    win32.h
    CONDITION WIN32
)

if(WIN32)
    target_sources(openttd PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/win32_main.cpp)
endif()
