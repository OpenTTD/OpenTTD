set(GENERATED_BINARY_DIR ${CMAKE_BINARY_DIR}/generated)
set(TABLE_BINARY_DIR ${GENERATED_BINARY_DIR}/table)

set(TABLE_INI_SOURCE_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/company_settings.ini
        ${CMAKE_CURRENT_SOURCE_DIR}/currency_settings.ini
        ${CMAKE_CURRENT_SOURCE_DIR}/difficulty_settings.ini
        ${CMAKE_CURRENT_SOURCE_DIR}/economy_settings.ini
        ${CMAKE_CURRENT_SOURCE_DIR}/game_settings.ini
        ${CMAKE_CURRENT_SOURCE_DIR}/gui_settings.ini
        ${CMAKE_CURRENT_SOURCE_DIR}/linkgraph_settings.ini
        ${CMAKE_CURRENT_SOURCE_DIR}/locale_settings.ini
        ${CMAKE_CURRENT_SOURCE_DIR}/misc_settings.ini
        ${CMAKE_CURRENT_SOURCE_DIR}/multimedia_settings.ini
        ${CMAKE_CURRENT_SOURCE_DIR}/network_settings.ini
        ${CMAKE_CURRENT_SOURCE_DIR}/network_private_settings.ini
        ${CMAKE_CURRENT_SOURCE_DIR}/network_secrets_settings.ini
        ${CMAKE_CURRENT_SOURCE_DIR}/news_display_settings.ini
        ${CMAKE_CURRENT_SOURCE_DIR}/old_gameopt_settings.ini
        ${CMAKE_CURRENT_SOURCE_DIR}/pathfinding_settings.ini
        ${CMAKE_CURRENT_SOURCE_DIR}/script_settings.ini
        ${CMAKE_CURRENT_SOURCE_DIR}/win32_settings.ini
        ${CMAKE_CURRENT_SOURCE_DIR}/window_settings.ini
        ${CMAKE_CURRENT_SOURCE_DIR}/world_settings.ini
)

if (HOST_BINARY_DIR)
    include(${HOST_BINARY_DIR}/settingsgen.cmake)
endif()

# Generate a command and target to create the settings table
add_custom_command_timestamp(OUTPUT ${TABLE_BINARY_DIR}/settings.h
        COMMAND ${CMAKE_COMMAND} -E make_directory ${TABLE_BINARY_DIR}
        COMMAND settingsgen
                -o ${TABLE_BINARY_DIR}/settings.h
                -b ${CMAKE_SOURCE_DIR}/src/table/settings.h.preamble
                -a ${CMAKE_SOURCE_DIR}/src/table/settings.h.postamble
                ${TABLE_INI_SOURCE_FILES}
        DEPENDS settingsgen ${TABLE_INI_SOURCE_FILES}
                ${CMAKE_SOURCE_DIR}/src/table/settings.h.preamble
                ${CMAKE_SOURCE_DIR}/src/table/settings.h.postamble
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating table/settings.h"
)
add_custom_target_timestamp(table_settings
        DEPENDS
        ${TABLE_BINARY_DIR}/settings.h
)

add_library(settings
        INTERFACE
)
target_include_directories(settings
        INTERFACE
        ${GENERATED_BINARY_DIR}
)
add_dependencies(settings
        table_settings
)
add_library(openttd::settings ALIAS settings)
