cmake_minimum_required(VERSION 3.9)

if (NOT HOST_BINARY_DIR)
    project(strgen)

    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake")

    set(sourcefiles
            strgen.cpp
            strgen_base.cpp
            ../core/alloc_func.cpp
            ../misc/getoptdata.cpp
            ../string.cpp
    )
    add_definitions(-DSTRGEN)
    add_executable(strgen ${sourcefiles})

    include(Endian)
    add_endian_definition()

    export(TARGETS strgen FILE ${CMAKE_BINARY_DIR}/strgen.cmake)
    add_dependencies(tools strgen)
endif()

if(OPTION_TOOLS_ONLY)
    return()
endif()

# Source Files
add_files(strgen_base.cpp)

# Header Files
add_files(strgen.h)
