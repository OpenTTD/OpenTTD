add_subdirectory(baseset)

add_library(media
        INTERFACE
)
add_library(openttd::media ALIAS media)

if(NOT UNIX OR APPLE)
    return()
endif()

set(MEDIA_PNG_SOURCE_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/openttd.16.png
        ${CMAKE_CURRENT_SOURCE_DIR}/openttd.32.png
        ${CMAKE_CURRENT_SOURCE_DIR}/openttd.48.png
        ${CMAKE_CURRENT_SOURCE_DIR}/openttd.64.png
        ${CMAKE_CURRENT_SOURCE_DIR}/openttd.128.png
        ${CMAKE_CURRENT_SOURCE_DIR}/openttd.256.png
)

# Walk over all the png files, and generate a command to copy them
foreach(MEDIA_PNG_SOURCE_FILE IN LISTS MEDIA_PNG_SOURCE_FILES)
        get_filename_component(MEDIA_PNG_FILE_NAME "${MEDIA_PNG_SOURCE_FILE}" NAME)
        string(REGEX REPLACE "[^.]+.([0-9]*).*" "${CMAKE_CURRENT_BINARY_DIR}/icons/hicolor/\\1x\\1/apps/${BINARY_NAME}.png" MEDIA_PNG_BINARY_FILE "${MEDIA_PNG_FILE_NAME}")

        add_custom_command(OUTPUT ${MEDIA_PNG_BINARY_FILE}
            COMMAND ${CMAKE_COMMAND} -E copy
                    ${MEDIA_PNG_SOURCE_FILE}
                    ${MEDIA_PNG_BINARY_FILE}
            MAIN_DEPENDENCY ${MEDIA_PNG_SOURCE_FILE}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Copying ${MEDIA_PNG_FILE_NAME} file"
        )

        list(APPEND MEDIA_BINARY_FILES ${MEDIA_PNG_BINARY_FILE})
endforeach()

set(MEDIA_XPM_SOURCE_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/openttd.32.xpm
        ${CMAKE_CURRENT_SOURCE_DIR}/openttd.64.xpm
)

# Walk over all the xpm files, and generate a command to copy them
foreach(MEDIA_XPM_SOURCE_FILE IN LISTS MEDIA_XPM_SOURCE_FILES)
        get_filename_component(MEDIA_XPM_FILE_NAME "${MEDIA_XPM_SOURCE_FILE}" NAME)
        get_filename_component(MEDIA_XPM_FILE_EXT "${MEDIA_XPM_FILE_NAME}" EXT)
        set(MEDIA_XPM_BINARY_FILE "${CMAKE_CURRENT_BINARY_DIR}/pixmaps/${BINARY_NAME}${MEDIA_XPM_FILE_EXT}")

        add_custom_command(OUTPUT ${MEDIA_XPM_BINARY_FILE}
            COMMAND ${CMAKE_COMMAND} -E copy
                    ${MEDIA_XPM_SOURCE_FILE}
                    ${MEDIA_XPM_BINARY_FILE}
            MAIN_DEPENDENCY ${MEDIA_XPM_SOURCE_FILE}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Copying ${MEDIA_XPM_FILE_NAME} file"
        )

        list(APPEND MEDIA_BINARY_FILES ${MEDIA_XPM_BINARY_FILE})
endforeach()

# Create a new target which copies media files
add_custom_target(media_files
        DEPENDS
        ${MEDIA_BINARY_FILES}
)

add_dependencies(media
        media_files
)

get_target_property(LANG_SOURCE_FILES language_files LANG_SOURCE_FILES)

set(DESKTOP_BINARY_NAME ${BINARY_NAME}.desktop)

add_custom_command_timestamp(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${DESKTOP_BINARY_NAME}
        COMMAND ${CMAKE_COMMAND}
                -DDESKTOP_SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/openttd.desktop
                -DDESKTOP_BINARY_FILE=${CMAKE_CURRENT_BINARY_DIR}/${DESKTOP_BINARY_NAME}
                -DBINARY_NAME=${BINARY_NAME}
                -P ${CMAKE_SOURCE_DIR}/cmake/scripts/Desktop.cmake
                --
                ${LANG_SOURCE_FILES}
        MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/openttd.desktop
        DEPENDS ${LANG_SOURCE_FILES}
                ${CMAKE_SOURCE_DIR}/cmake/scripts/Desktop.cmake
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating desktop file"
)

# Create a new target which generates desktop file
add_custom_target_timestamp(desktop_file
        DEPENDS
        ${CMAKE_CURRENT_BINARY_DIR}/${DESKTOP_BINARY_NAME}
)

add_dependencies(media
        desktop_file
)
