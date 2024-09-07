add_subdirectory(tests)

add_files(
    scriptrun.cpp
    scriptrun.h
    CONDITION ICU_i18n_FOUND
)
