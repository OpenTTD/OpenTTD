# Get script_window.hpp dependencies
file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/script_window.hpp.in ENUM_LINES REGEX "@enum")
foreach(ENUM IN LISTS ENUM_LINES)
    string(REGEX REPLACE ".* ([^ @]+)@.*" "\\1" FILE_PATTERN "${ENUM}")
    file(GLOB FILENAMES "${FILE_PATTERN}")
    list(APPEND DEPENDENCIES ${FILENAMES})
endforeach(ENUM)
list(REMOVE_DUPLICATES DEPENDENCIES)

# Add a command to generate script_window.hpp
add_custom_command_timestamp(OUTPUT ${CMAKE_BINARY_DIR}/generated/script/api/script_window.hpp
        COMMAND ${CMAKE_COMMAND}
                -DGENERATE_SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/script_window.hpp.in
                -DGENERATE_BINARY_FILE=${CMAKE_BINARY_DIR}/generated/script/api/script_window.hpp
                -P ${CMAKE_SOURCE_DIR}/cmake/scripts/GenerateWidget.cmake
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/generated/script/api/dummy # dummy directory for #include "../script_window.hpp"
        MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/script_window.hpp.in
        DEPENDS ${CMAKE_SOURCE_DIR}/cmake/scripts/GenerateWidget.cmake ${DEPENDENCIES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating script_window.hpp"
)
add_custom_target_timestamp(script_window
        DEPENDS
        ${CMAKE_BINARY_DIR}/generated/script/api/script_window.hpp
)

add_library(script_api
        INTERFACE
)
target_include_directories(script_api
        INTERFACE
        ${CMAKE_BINARY_DIR}/generated/script/api/
        ${CMAKE_BINARY_DIR}/generated/script/api/dummy # dummy path so #include "../script_window.hpp" works
        ${CMAKE_CURRENT_SOURCE_DIR}
)
add_dependencies(script_api
        script_window
)
add_library(openttd::script_api ALIAS script_api)


add_files(
    ai_changelog.hpp
    game_changelog.hpp
    script_accounting.hpp
    script_admin.hpp
    script_airport.hpp
    script_base.hpp
    script_basestation.hpp
    script_bridge.hpp
    script_bridgelist.hpp
    script_cargo.hpp
    script_cargolist.hpp
    script_cargomonitor.hpp
    script_client.hpp
    script_clientlist.hpp
    script_company.hpp
    script_companymode.hpp
    script_controller.hpp
    script_date.hpp
    script_depotlist.hpp
    script_engine.hpp
    script_enginelist.hpp
    script_error.hpp
    script_event.hpp
    script_event_types.hpp
    script_execmode.hpp
    script_game.hpp
    script_gamesettings.hpp
    script_goal.hpp
    script_group.hpp
    script_grouplist.hpp
    script_industry.hpp
    script_industrylist.hpp
    script_industrytype.hpp
    script_industrytypelist.hpp
    script_info_docs.hpp
    script_infrastructure.hpp
    script_list.hpp
    script_log.hpp
    script_map.hpp
    script_marine.hpp
    script_news.hpp
    script_object.hpp
    script_order.hpp
    script_priorityqueue.hpp
    script_rail.hpp
    script_railtypelist.hpp
    script_road.hpp
    script_roadtypelist.hpp
    script_sign.hpp
    script_signlist.hpp
    script_station.hpp
    script_stationlist.hpp
    script_story_page.hpp
    script_storypagelist.hpp
    script_storypageelementlist.hpp
    script_subsidy.hpp
    script_subsidylist.hpp
    script_testmode.hpp
    script_text.hpp
    script_tile.hpp
    script_tilelist.hpp
    script_town.hpp
    script_townlist.hpp
    script_tunnel.hpp
    script_types.hpp
    script_vehicle.hpp
    script_vehiclelist.hpp
    script_viewport.hpp
    script_waypoint.hpp
    script_waypointlist.hpp
    script_accounting.cpp
    script_admin.cpp
    script_airport.cpp
    script_base.cpp
    script_basestation.cpp
    script_bridge.cpp
    script_bridgelist.cpp
    script_cargo.cpp
    script_cargolist.cpp
    script_cargomonitor.cpp
    script_client.cpp
    script_clientlist.cpp
    script_company.cpp
    script_companymode.cpp
    script_controller.cpp
    script_date.cpp
    script_depotlist.cpp
    script_engine.cpp
    script_enginelist.cpp
    script_error.cpp
    script_event.cpp
    script_event_types.cpp
    script_execmode.cpp
    script_game.cpp
    script_gamesettings.cpp
    script_goal.cpp
    script_group.cpp
    script_grouplist.cpp
    script_industry.cpp
    script_industrylist.cpp
    script_industrytype.cpp
    script_industrytypelist.cpp
    script_infrastructure.cpp
    script_list.cpp
    script_log.cpp
    script_map.cpp
    script_marine.cpp
    script_news.cpp
    script_object.cpp
    script_order.cpp
    script_priorityqueue.cpp
    script_rail.cpp
    script_railtypelist.cpp
    script_road.cpp
    script_roadtypelist.cpp
    script_sign.cpp
    script_signlist.cpp
    script_station.cpp
    script_stationlist.cpp
    script_story_page.cpp
    script_storypagelist.cpp
    script_storypageelementlist.cpp
    script_subsidy.cpp
    script_subsidylist.cpp
    script_testmode.cpp
    script_text.cpp
    script_tile.cpp
    script_tilelist.cpp
    script_town.cpp
    script_townlist.cpp
    script_tunnel.cpp
    script_vehicle.cpp
    script_vehiclelist.cpp
    script_viewport.cpp
    script_waypoint.cpp
    script_waypointlist.cpp
    script_window.cpp
)
