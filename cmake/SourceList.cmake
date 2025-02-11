function(_add_files_tgt tgt)
    cmake_parse_arguments(PARAM "" "" "CONDITION" ${ARGN})
    set(PARAM_FILES "${PARAM_UNPARSED_ARGUMENTS}")

    if(PARAM_CONDITION)
        if(NOT (${PARAM_CONDITION}))
            return()
        endif()
    endif()

    foreach(FILE IN LISTS PARAM_FILES)
        # Some IDEs are not happy with duplicated filenames, so we detect that before adding the file.
        get_target_property(${tgt}_FILES ${tgt} SOURCES)
        if(${tgt}_FILES MATCHES "/${FILE}(;|$)")
            string(REGEX REPLACE "(^|.+;)([^;]+/${FILE})(;.+|$)" "\\2" RES "${${tgt}_FILES}")
            # Ignore header files duplicates in 3rdparty.
            if(NOT (${FILE} MATCHES "\.h" AND (${RES} MATCHES "3rdparty" OR ${CMAKE_CURRENT_SOURCE_DIR} MATCHES "3rdparty")))
                message(FATAL_ERROR "${tgt}: ${CMAKE_CURRENT_SOURCE_DIR}/${FILE} filename is a duplicate of ${RES}")
            endif()
        endif()

        target_sources(${tgt} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/${FILE})
    endforeach()
endfunction()

# Add a file to be compiled.
#
# add_files([file1 ...] CONDITION condition [condition ...])
#
# CONDITION is a complete statement that can be evaluated with if().
# If it evaluates true, the source files will be added; otherwise not.
# For example: ADD_IF SDL_FOUND AND Allegro_FOUND
#
function(add_files)
    _add_files_tgt(openttd_lib ${ARGV})
endfunction()

# Add a test file to be compiled.
#
# add_test_files([file1 ...] CONDITION condition [condition ...])
#
# CONDITION is a complete statement that can be evaluated with if().
# If it evaluates true, the source files will be added; otherwise not.
# For example: ADD_IF SDL_FOUND AND Allegro_FOUND
#
function(add_test_files)
    _add_files_tgt(openttd_test ${ARGV})
endfunction()
