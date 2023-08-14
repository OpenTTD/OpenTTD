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

# This function works around an 'issue' with CMake, where
# set_source_files_properties() only works in the scope of the file. We want
# to set properties for the source file on a more global level. To solve this,
# this function records the flags you want, and a macro adds them in the root
# CMakeLists.txt.
# See this URL for more information on the issue:
# http://cmake.3232098.n2.nabble.com/scope-of-set-source-files-properties-td4766111.html
#
# set_compile_flags([file1 ...] COMPILE_FLAGS cflag [cflag ...])
#
function(set_compile_flags)
    cmake_parse_arguments(PARAM "" "" "COMPILE_FLAGS" ${ARGN})
    set(PARAM_FILES "${PARAM_UNPARSED_ARGUMENTS}")

    get_property(SOURCE_PROPERTIES GLOBAL PROPERTY source_properties)

    foreach(FILE IN LISTS PARAM_FILES)
        list(APPEND SOURCE_PROPERTIES "${CMAKE_CURRENT_SOURCE_DIR}/${FILE}::${PARAM_COMPILE_FLAGS}")
    endforeach()

    set_property(GLOBAL PROPERTY source_properties "${SOURCE_PROPERTIES}")
endfunction()

# Call this macro in the same CMakeLists.txt and after add_executable().
# This makes sure all the COMPILE_FLAGS of set_compile_flags() are set
# correctly.
#
# process_compile_flags()
#
function(process_compile_flags)
    get_property(SOURCE_PROPERTIES GLOBAL PROPERTY source_properties)

    foreach(ENTRY ${SOURCE_PROPERTIES})
        string(REPLACE "::" ";" ENTRY "${ENTRY}")
        list(GET ENTRY 0 FILE)
        list(GET ENTRY 1 PROPERTIES)

        set_source_files_properties(${FILE} PROPERTIES COMPILE_FLAGS ${PROPERTIES})
    endforeach()
endfunction()
