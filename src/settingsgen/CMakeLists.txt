cmake_minimum_required(VERSION 3.9)

if (NOT HOST_BINARY_DIR)
    project(settingsgen)

    set(sourcefiles
            settingsgen.cpp
            ../core/alloc_func.cpp
            ../misc/getoptdata.cpp
            ../error.cpp
            ../ini_load.cpp
            ../string.cpp
    )
    add_definitions(-DSETTINGSGEN)
    add_executable(settingsgen ${sourcefiles})

    export(TARGETS settingsgen FILE ${CMAKE_BINARY_DIR}/settingsgen.cmake)
    add_dependencies(tools settingsgen)
endif()
