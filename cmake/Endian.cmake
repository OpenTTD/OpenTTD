# Add the definitions to indicate which endian we are building for.
#
# add_endian_definition()
#
function(add_endian_definition)
    include(TestBigEndian)
    TEST_BIG_ENDIAN(IS_BIG_ENDIAN)

    if (IS_BIG_ENDIAN)
        add_definitions(-DTTD_ENDIAN=TTD_BIG_ENDIAN)
    else (IS_BIG_ENDIAN)
        add_definitions(-DTTD_ENDIAN=TTD_LITTLE_ENDIAN)
    endif (IS_BIG_ENDIAN)
endfunction()
