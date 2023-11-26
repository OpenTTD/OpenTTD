add_subdirectory(openttd)
add_subdirectory(orig_extra)

set(BASESET_SOURCE_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/orig_dos.obg
        ${CMAKE_CURRENT_SOURCE_DIR}/orig_dos_de.obg
        ${CMAKE_CURRENT_SOURCE_DIR}/orig_win.obg
        ${CMAKE_CURRENT_SOURCE_DIR}/no_music.obm
        ${CMAKE_CURRENT_SOURCE_DIR}/orig_dos.obm
        ${CMAKE_CURRENT_SOURCE_DIR}/orig_tto.obm
        ${CMAKE_CURRENT_SOURCE_DIR}/orig_win.obm
        ${CMAKE_CURRENT_SOURCE_DIR}/no_sound.obs
        ${CMAKE_CURRENT_SOURCE_DIR}/orig_dos.obs
        ${CMAKE_CURRENT_SOURCE_DIR}/orig_win.obs
)
set(BASESET_OTHER_SOURCE_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/openttd.grf
        ${CMAKE_CURRENT_SOURCE_DIR}/opntitle.dat
        ${CMAKE_CURRENT_SOURCE_DIR}/orig_extra.grf
        ${CMAKE_CURRENT_SOURCE_DIR}/../openttd.32.bmp
)

# Done by the subdirectories, if nforenum / grfcodec is installed
if(GRFCODEC_FOUND)
    set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/openttd.grf PROPERTIES GENERATED TRUE)
    set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/orig_extra.grf PROPERTIES GENERATED TRUE)

    list(APPEND BASESET_BINARY_FILES openttd.grf)
    list(APPEND BASESET_BINARY_FILES orig_extra.grf)
endif()

set(BASESET_EXTRAGRF_FILE ${CMAKE_CURRENT_SOURCE_DIR}/orig_extra.grf)

# Walk over all the baseset files, and generate a command to configure them
foreach(BASESET_SOURCE_FILE IN LISTS BASESET_SOURCE_FILES)
    get_filename_component(BASESET_SOURCE_FILE_NAME "${BASESET_SOURCE_FILE}" NAME)
    set(BASESET_BINARY_FILE "${CMAKE_BINARY_DIR}/baseset/${BASESET_SOURCE_FILE_NAME}")

    get_target_property(LANG_SOURCE_FILES language_files LANG_SOURCE_FILES)

    add_custom_command_timestamp(OUTPUT ${BASESET_BINARY_FILE}
            COMMAND ${CMAKE_COMMAND}
                    -DBASESET_SOURCE_FILE=${BASESET_SOURCE_FILE}
                    -DBASESET_BINARY_FILE=${BASESET_BINARY_FILE}
                    -DBASESET_EXTRAGRF_FILE=${BASESET_EXTRAGRF_FILE}
                    -P ${CMAKE_SOURCE_DIR}/cmake/scripts/Baseset.cmake
                    --
                    ${LANG_SOURCE_FILES}
            MAIN_DEPENDENCY ${BASESET_SOURCE_FILE}
            DEPENDS ${LANG_SOURCE_FILES}
                    ${BASESET_EXTRAGRF_FILE}
                    ${BASESET_EXTRAGRF_FILE}.hash
                    ${CMAKE_SOURCE_DIR}/cmake/scripts/Baseset.cmake
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Generating ${BASESET_SOURCE_FILE_NAME} baseset metadata file"
    )

    list(APPEND BASESET_BINARY_FILES ${BASESET_BINARY_FILE})
endforeach()

# Walk over all the other baseset files, and generate a command to copy them
foreach(BASESET_OTHER_SOURCE_FILE IN LISTS BASESET_OTHER_SOURCE_FILES)
    get_filename_component(BASESET_OTHER_SOURCE_FILE_NAME "${BASESET_OTHER_SOURCE_FILE}" NAME)
    set(BASESET_OTHER_BINARY_FILE "${CMAKE_BINARY_DIR}/baseset/${BASESET_OTHER_SOURCE_FILE_NAME}")

    add_custom_command(OUTPUT ${BASESET_OTHER_BINARY_FILE}
            COMMAND ${CMAKE_COMMAND} -E copy
                    ${BASESET_OTHER_SOURCE_FILE}
                    ${BASESET_OTHER_BINARY_FILE}
            MAIN_DEPENDENCY ${BASESET_OTHER_SOURCE_FILE}
            COMMENT "Copying ${BASESET_OTHER_SOURCE_FILE_NAME} baseset file"
    )

    list(APPEND BASESET_BINARY_FILES ${BASESET_OTHER_BINARY_FILE})
endforeach()

# Create a new target which generates all baseset metadata files
add_custom_target_timestamp(baseset_files
        DEPENDS
        ${BASESET_BINARY_FILES}
)

add_library(basesets
        INTERFACE
)
add_dependencies(basesets
        baseset_files
)
add_library(openttd::basesets ALIAS basesets)
