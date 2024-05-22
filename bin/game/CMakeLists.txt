set(GS_COMPAT_SOURCE_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/compat_1.2.nut
	${CMAKE_CURRENT_SOURCE_DIR}/compat_1.3.nut
	${CMAKE_CURRENT_SOURCE_DIR}/compat_1.4.nut
	${CMAKE_CURRENT_SOURCE_DIR}/compat_1.5.nut
	${CMAKE_CURRENT_SOURCE_DIR}/compat_1.6.nut
	${CMAKE_CURRENT_SOURCE_DIR}/compat_1.7.nut
	${CMAKE_CURRENT_SOURCE_DIR}/compat_1.8.nut
	${CMAKE_CURRENT_SOURCE_DIR}/compat_1.9.nut
	${CMAKE_CURRENT_SOURCE_DIR}/compat_1.10.nut
	${CMAKE_CURRENT_SOURCE_DIR}/compat_1.11.nut
	${CMAKE_CURRENT_SOURCE_DIR}/compat_12.nut
	${CMAKE_CURRENT_SOURCE_DIR}/compat_13.nut
	${CMAKE_CURRENT_SOURCE_DIR}/compat_14.nut
	${CMAKE_CURRENT_SOURCE_DIR}/compat_15.nut
)

foreach(GS_COMPAT_SOURCE_FILE IN LISTS GS_COMPAT_SOURCE_FILES)
    string(REPLACE "${CMAKE_SOURCE_DIR}/bin/" "" GS_COMPAT_SOURCE_FILE_NAME "${GS_COMPAT_SOURCE_FILE}")
    string(CONCAT GS_COMPAT_BINARY_FILE "${CMAKE_BINARY_DIR}/" "${GS_COMPAT_SOURCE_FILE_NAME}")

    add_custom_command(OUTPUT ${GS_COMPAT_BINARY_FILE}
            COMMAND ${CMAKE_COMMAND} -E copy
                    ${GS_COMPAT_SOURCE_FILE}
                    ${GS_COMPAT_BINARY_FILE}
            MAIN_DEPENDENCY ${GS_COMPAT_SOURCE_FILE}
            COMMENT "Copying ${GS_COMPAT_SOURCE_FILE_NAME}"
    )

    list(APPEND GS_COMPAT_BINARY_FILES ${GS_COMPAT_BINARY_FILE})
endforeach()

# Create a new target which copies all compat files
add_custom_target(gs_compat_files
        DEPENDS ${GS_COMPAT_BINARY_FILES}
)

add_dependencies(openttd
    gs_compat_files
)
