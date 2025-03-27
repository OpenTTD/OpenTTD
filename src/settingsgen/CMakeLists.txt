cmake_minimum_required(VERSION 3.16)

if (NOT HOST_BINARY_DIR)
    project(settingsgen)

    set(sourcefiles
            settingsgen.cpp
            ../3rdparty/fmt/format.cc
            ../misc/getoptdata.cpp
            ../error.cpp
            ../ini_load.cpp
            ../string.cpp
            ../core/string_builder.cpp
            ../core/utf8.cpp
    )
    add_definitions(-DSETTINGSGEN)
    add_executable(settingsgen ${sourcefiles})

    export(TARGETS settingsgen FILE ${CMAKE_BINARY_DIR}/settingsgen.cmake)
    add_dependencies(tools settingsgen)
endif()
