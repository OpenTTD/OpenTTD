set(GENERATED_BINARY_DIR ${CMAKE_BINARY_DIR}/generated)
set(TABLE_BINARY_DIR ${GENERATED_BINARY_DIR}/table)

set(TABLE_INI_SOURCE_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/company_settings.ini
        ${CMAKE_CURRENT_SOURCE_DIR}/currency_settings.ini
        ${CMAKE_CURRENT_SOURCE_DIR}/gameopt_settings.ini
        ${CMAKE_CURRENT_SOURCE_DIR}/misc_settings.ini
        ${CMAKE_CURRENT_SOURCE_DIR}/settings.ini
        ${CMAKE_CURRENT_SOURCE_DIR}/win32_settings.ini
        ${CMAKE_CURRENT_SOURCE_DIR}/window_settings.ini
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

add_files(
    airport_defaults.h
    airport_movement.h
    airporttile_ids.h
    airporttiles.h
    animcursors.h
    autorail.h
    bridge_land.h
    build_industry.h
    cargo_const.h
    clear_land.h
    control_codes.h
    elrail_data.h
    engines.h
    genland.h
    heightmap_colours.h
    industry_land.h
    landscape_sprite.h
    newgrf_debug_data.h
    object_land.h
    palette_convert.h
    palettes.h
    pricebase.h
    railtypes.h
    road_land.h
    roadtypes.h
    roadveh_movement.h
    sprites.h
    station_land.h
    strgen_tables.h
    string_colours.h
    town_land.h
    townname.h
    track_land.h
    train_cmd.h
    tree_land.h
    unicode.h
    water_land.h
)

add_files(
    opengl_shader.h
    CONDITION OPENGL_FOUND
)
