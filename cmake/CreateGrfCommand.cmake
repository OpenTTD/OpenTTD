# Macro which contains all bits and pieces to create a single grf file based
# on NML, PNG and LNG files.
#
# create_grf_command()
#
function(create_grf_command)
    set(GRF_SOURCE_FILES ${ARGV})

    get_filename_component(GRF_SOURCE_FOLDER_NAME "${CMAKE_CURRENT_SOURCE_DIR}" NAME)
    get_filename_component(GRF_BINARY_FILE ${CMAKE_CURRENT_SOURCE_DIR}/../${GRF_SOURCE_FOLDER_NAME}.grf ABSOLUTE)

    add_custom_command(OUTPUT ${GRF_BINARY_FILE} ${GRF_BINARY_FILE}.md5
            COMMAND ${CMAKE_COMMAND}
                    -DGRF_SOURCE_FOLDER=${CMAKE_CURRENT_SOURCE_DIR}
                    -DGRF_BINARY_FOLDER=${CMAKE_CURRENT_BINARY_DIR}
                    -DGRF_BINARY_FILE=${GRF_BINARY_FILE}
                    -DNML_EXECUTABLE=${NML_EXECUTABLE}
                    -P ${CMAKE_SOURCE_DIR}/cmake/scripts/CreateGRF.cmake
            MAIN_DEPENDENCY ${CMAKE_SOURCE_DIR}/cmake/scripts/CreateGRF.cmake
            DEPENDS ${GRF_SOURCE_FILES}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Generating ${GRF_SOURCE_FOLDER_NAME}.grf"
    )

    # For conviance, if you want to only test building the GRF
    add_custom_target(${GRF_SOURCE_FOLDER_NAME}.grf
            DEPENDS
            ${GRF_BINARY_FILE}
    )
endfunction()
