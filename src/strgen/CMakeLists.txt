cmake_minimum_required(VERSION 3.5)

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


# Source Files
add_files(strgen_base.cpp)

# Header Files
add_files(strgen.h)
