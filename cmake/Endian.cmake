# Add the definitions to indicate which endian we are building for.
#
# add_endian_definition()
#
function(add_endian_definition)
    if(CMAKE_VERSION VERSION_LESS "3.20.0")
        include(TestBigEndian)
        TEST_BIG_ENDIAN(IS_BIG_ENDIAN)

        if(IS_BIG_ENDIAN)
            add_definitions(-DTTD_ENDIAN=TTD_BIG_ENDIAN)
        else()
            add_definitions(-DTTD_ENDIAN=TTD_LITTLE_ENDIAN)
        endif()
    else()
        # Since CMake 3.20, the TestBigEndian module is deprecated and the byte order is available directly.
        if(CMAKE_CXX_BYTE_ORDER STREQUAL "BIG_ENDIAN" OR CMAKE_OBJCXX_BYTE_ORDER STREQUAL "BIG_ENDIAN")
            add_definitions(-DTTD_ENDIAN=TTD_BIG_ENDIAN)
        else()
            add_definitions(-DTTD_ENDIAN=TTD_LITTLE_ENDIAN)
        endif()
    endif()
endfunction()
