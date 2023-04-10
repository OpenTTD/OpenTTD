add_files(
    os2.cpp
    CONDITION OPTION_OS2
)

if(OPTION_OS2)
    target_sources(openttd PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/os2_main.cpp)
endif()
