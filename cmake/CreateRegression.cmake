# Macro which contains all bits and pieces to create the regression tests.
# This creates both a standalone target 'regression', and it integrates with
# 'ctest'. The first is prefered, as it is more verbose, and takes care of
# dependencies correctly.
#
# create_regression(file1 ...)
#
macro(create_regression)
    set(REGRESSION_SOURCE_FILES ${ARGN})

    foreach(REGRESSION_SOURCE_FILE IN LISTS REGRESSION_SOURCE_FILES)
        string(REPLACE "${CMAKE_SOURCE_DIR}/regression/" "" REGRESSION_SOURCE_FILE_NAME "${REGRESSION_SOURCE_FILE}")
        string(CONCAT REGRESSION_BINARY_FILE "${CMAKE_BINARY_DIR}/ai/" "${REGRESSION_SOURCE_FILE_NAME}")

        add_custom_command(OUTPUT ${REGRESSION_BINARY_FILE}
                COMMAND ${CMAKE_COMMAND} -E copy
                        ${REGRESSION_SOURCE_FILE}
                        ${REGRESSION_BINARY_FILE}
                MAIN_DEPENDENCY ${REGRESSION_SOURCE_FILE}
                COMMENT "Copying ${REGRESSION_SOURCE_FILE_NAME} regression file"
        )

        list(APPEND REGRESSION_BINARY_FILES ${REGRESSION_BINARY_FILE})
    endforeach()

    get_filename_component(REGRESSION_TEST_NAME "${CMAKE_CURRENT_SOURCE_DIR}" NAME)

    # Create a new target which copies regression files
    add_custom_target(regression_${REGRESSION_TEST_NAME}_files
            DEPENDS
            ${REGRESSION_BINARY_FILES}
    )

    add_dependencies(regression_files regression_${REGRESSION_TEST_NAME}_files)

    add_custom_target(regression_${REGRESSION_TEST_NAME}
            COMMAND ${CMAKE_COMMAND}
                    -DOPENTTD_EXECUTABLE=$<TARGET_FILE:openttd>
                    -DEDITBIN_EXECUTABLE=${EDITBIN_EXECUTABLE}
                    -DREGRESSION_TEST=${REGRESSION_TEST_NAME}
                    -P "${CMAKE_SOURCE_DIR}/cmake/scripts/Regression.cmake"
            DEPENDS openttd regression_${REGRESSION_TEST_NAME}_files
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Running regression test ${REGRESSION_TEST_NAME}"
    )

    # Also make sure that 'make test' runs the regression
    add_test(NAME regression_${REGRESSION_TEST_NAME}
            COMMAND ${CMAKE_COMMAND}
                    -DOPENTTD_EXECUTABLE=$<TARGET_FILE:openttd>
                    -DEDITBIN_EXECUTABLE=${EDITBIN_EXECUTABLE}
                    -DREGRESSION_TEST=${REGRESSION_TEST_NAME}
                    -P "${CMAKE_SOURCE_DIR}/cmake/scripts/Regression.cmake"
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )

    add_dependencies(regression regression_${REGRESSION_TEST_NAME})
endmacro()
