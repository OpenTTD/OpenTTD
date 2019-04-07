cmake_minimum_required(VERSION 3.5)

project(settingsgen)

set(sourcefiles
        settingsgen.cpp
        ../core/alloc_func.cpp
        ../misc/getoptdata.cpp
        ../ini_load.cpp
        ../string.cpp
)
add_definitions(-DSETTINGSGEN)
add_executable(settingsgen ${sourcefiles})
