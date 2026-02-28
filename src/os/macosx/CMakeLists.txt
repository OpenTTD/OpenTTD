# macOS-specific platform files (exclude iOS which uses src/os/ios/)
add_files(
    crashlog_osx.cpp
    font_osx.cpp
    font_osx.h
    macos.h
    macos.mm
    macos_objective_c.h
    osx_stdafx.h
    string_osx.cpp
    string_osx.h
    survey_osx.cpp
    CONDITION APPLE AND NOT IOS_BUILD
)

if(APPLE AND NOT IOS_BUILD)
    target_sources(openttd PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/osx_main.cpp)
endif()
