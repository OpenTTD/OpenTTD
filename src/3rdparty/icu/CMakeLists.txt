add_subdirectory(tests)

add_files(
    scriptrun.cpp
    scriptrun.h
    CONDITION ICU_I18N_FOUND
)
