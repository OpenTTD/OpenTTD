macro(_parse_arguments_with_multi_hack ORIGINAL_COMMAND_LINE)
    # cmake_parse_arguments() put all the MULTIS in a single variable; you
    # lose the ability to see for example multiple COMMANDs. To be able to
    # passthrough multiple MULTIS, we add a marker after every MULTI. This
    # allows us to reassemble the correct amount again before giving it to
    # the wrapped command with _reassemble_command_line().

    set(COMMAND_LINE "${ORIGINAL_COMMAND_LINE}")

    foreach(MULTI IN LISTS MULTIS)
        string(REPLACE "${MULTI}" "${MULTI};:::" COMMAND_LINE "${COMMAND_LINE}")
    endforeach()

    cmake_parse_arguments(PARAM "${OPTIONS}" "${SINGLES}" "${MULTIS}" ${COMMAND_LINE})
endmacro()

macro(_reassemble_command_line)
    # Reassemble the command line as we original got it.
    set(NEW_COMMAND_LINE ${PARAM_UNPARSED_ARGUMENTS})

    foreach(OPTION IN LISTS OPTIONS)
        if(PARAM_${OPTION})
            list(APPEND NEW_COMMAND_LINE "${OPTION}")
        endif()
    endforeach()

    foreach(SINGLE IN LISTS SINGLES)
        if(PARAM_${SINGLE})
            list(APPEND NEW_COMMAND_LINE "${SINGLE}" "${PARAM_${SINGLE}}")
        endif()
    endforeach()

    foreach(MULTI IN LISTS MULTIS)
        if(PARAM_${MULTI})
            # Replace our special marker with the name of the MULTI again. This
            # restores for example multiple COMMANDs again.
            string(REPLACE ":::" "${MULTI}" PARAM_${MULTI} "${PARAM_${MULTI}}")
            list(APPEND NEW_COMMAND_LINE "${PARAM_${MULTI}}")
        endif()
    endforeach()
endmacro()

# Generated files can be older than their dependencies, causing useless
# regenerations. This function replaces each file in OUTPUT with a .timestamp
# file, adds a command to touch it and move the original file in BYPRODUCTS,
# before calling add_custom_command().
#
# Note: Any add_custom_target() depending on files in original OUTPUT must use
# add_custom_target_timestamp() instead to have the correct dependencies.
#
# add_custom_command_timestamp(OUTPUT output1 [output2 ...]
#                       COMMAND command1 [ARGS] [args1...]
#                       [COMMAND command2 [ARGS] [args2...] ...]
#                       [MAIN_DEPENDENCY depend]
#                       [DEPENDS [depends...]]
#                       [BYPRODUCTS [files...]]
#                       [IMPLICIT_DEPENDS <lang1> depend1
#                                         [<lang2> depend2] ...]
#                       [WORKING_DIRECTORY dir]
#                       [COMMENT comment]
#                       [VERBATIM] [APPEND] [USES_TERMINAL])
function(add_custom_command_timestamp)
    set(OPTIONS VERBATIM APPEND USES_TERMINAL)
    set(SINGLES MAIN_DEPENDENCY WORKING_DIRECTORY COMMENT)
    set(MULTIS OUTPUT COMMAND DEPENDS BYPRODUCTS IMPLICIT_DEPENDS)

    _parse_arguments_with_multi_hack("${ARGN}")

    # Create a list of all the OUTPUTs (by removing our magic marker)
    string(REPLACE ":::;" "" OUTPUTS "${PARAM_OUTPUT}")

    # Reset the OUTPUT and BYPRODUCTS as an empty list (if needed).
    # Because they are MULTIS, we need to add our special marker here.
    set(PARAM_OUTPUT ":::")
    if(NOT PARAM_BYPRODUCTS)
        set(PARAM_BYPRODUCTS ":::")
    endif()

    foreach(OUTPUT IN LISTS OUTPUTS)
        # For every output, we add a 'cmake -E touch' entry to update the
        # timestamp on each run.
        get_filename_component(OUTPUT_FILENAME ${OUTPUT} NAME)
        string(APPEND PARAM_COMMAND ";:::;${CMAKE_COMMAND};-E;touch;${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_FILENAME}.timestamp")

        # We change the OUTPUT to a '.timestamp' variant, and make the real
        # output a byproduct.
        list(APPEND PARAM_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_FILENAME}.timestamp)
        list(APPEND PARAM_BYPRODUCTS ${OUTPUT})

        # Mark this file as being a byproduct; we use this again with
        # add_custom_target_timestamp() to know if we should point to the
        # '.timestamp' variant or not.
        set_source_files_properties(${OUTPUT} PROPERTIES BYPRODUCT ${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_FILENAME}.timestamp)
    endforeach()

    # Reassemble and call the wrapped command
    _reassemble_command_line()
    add_custom_command(${NEW_COMMAND_LINE})
endfunction()

# Generated files can be older than their dependencies, causing useless
# regenerations. This function adds a .timestamp file for each file in DEPENDS
# replaced by add_custom_command_timestamp(), before calling add_custom_target().
#
# add_custom_target_timestamp(Name [ALL] [command1 [args1...]]
#                      [COMMAND command2 [args2...] ...]
#                      [DEPENDS depend depend depend ... ]
#                      [BYPRODUCTS [files...]]
#                      [WORKING_DIRECTORY dir]
#                      [COMMENT comment]
#                      [VERBATIM] [USES_TERMINAL]
#                      [SOURCES src1 [src2...]])
function(add_custom_target_timestamp)
    set(OPTIONS VERBATIM USES_TERMINAL)
    set(SINGLES WORKING_DIRECTORY COMMENT)
    set(MULTIS COMMAND DEPENDS BYPRODUCTS SOURCES)
    # ALL is missing, as the order is important here. It will be picked up
    # by ${PARAM_UNPARSED_ARGUMENTS} when reassembling the command line.

    _parse_arguments_with_multi_hack("${ARGN}")

    # Create a list of all the DEPENDs (by removing our magic marker)
    string(REPLACE ":::;" "" DEPENDS "${PARAM_DEPENDS}")

    # Reset the DEPEND as an empty list.
    # Because it is a MULTI, we need to add our special marker here.
    set(PARAM_DEPENDS ":::")

    foreach(DEPEND IN LISTS DEPENDS)
        # Check if the output is produced by add_custom_command_timestamp()
        get_source_file_property(BYPRODUCT ${DEPEND} BYPRODUCT)

        if(BYPRODUCT STREQUAL "NOTFOUND")
            # If it is not, just keep it as DEPEND
            list(APPEND PARAM_DEPENDS "${DEPEND}")
        else()
            # If it is, the BYPRODUCT property points to the timestamp we want to depend on
            list(APPEND PARAM_DEPENDS "${BYPRODUCT}")
        endif()
    endforeach()

    # Reassemble and call the wrapped command
    _reassemble_command_line()
    add_custom_target(${NEW_COMMAND_LINE})
endfunction()
