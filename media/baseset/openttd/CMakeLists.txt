# In case nmlc is found, generate the GRF.
# Otherwise, just use them from the cache (read: git).
# This is mainly because not many people have this tool installed,
# so it is cheaper to cache them in git, and only regenerate when you are
# working on it / have the tools installed.
if(NML_FOUND)
    include(CreateGrfCommand)
    create_grf_command(
        # NML files
        ${CMAKE_CURRENT_SOURCE_DIR}/2ccmap.nml
        ${CMAKE_CURRENT_SOURCE_DIR}/airports.nml
        ${CMAKE_CURRENT_SOURCE_DIR}/airport_preview.nml
        ${CMAKE_CURRENT_SOURCE_DIR}/aqueduct.nml
        ${CMAKE_CURRENT_SOURCE_DIR}/autorail.nml
        ${CMAKE_CURRENT_SOURCE_DIR}/canals.nml
        ${CMAKE_CURRENT_SOURCE_DIR}/chars.nml
        ${CMAKE_CURRENT_SOURCE_DIR}/elrails.nml
        ${CMAKE_CURRENT_SOURCE_DIR}/foundations.nml
        ${CMAKE_CURRENT_SOURCE_DIR}/mono.nml
        ${CMAKE_CURRENT_SOURCE_DIR}/oneway.nml
        ${CMAKE_CURRENT_SOURCE_DIR}/openttd.nml
        ${CMAKE_CURRENT_SOURCE_DIR}/openttdgui.nml
        ${CMAKE_CURRENT_SOURCE_DIR}/palette.nml
        ${CMAKE_CURRENT_SOURCE_DIR}/roadstops.nml
        ${CMAKE_CURRENT_SOURCE_DIR}/signals.nml
        ${CMAKE_CURRENT_SOURCE_DIR}/sloped_tracks.nml
        ${CMAKE_CURRENT_SOURCE_DIR}/tramtracks.nml
        ${CMAKE_CURRENT_SOURCE_DIR}/tunnel_portals.nml
        # PNG files
        ${CMAKE_CURRENT_SOURCE_DIR}/airports.png
        ${CMAKE_CURRENT_SOURCE_DIR}/airport_preview.png
        ${CMAKE_CURRENT_SOURCE_DIR}/aqueduct.png
        ${CMAKE_CURRENT_SOURCE_DIR}/autorail.png
        ${CMAKE_CURRENT_SOURCE_DIR}/canals.png
        ${CMAKE_CURRENT_SOURCE_DIR}/canal_locks.png
        ${CMAKE_CURRENT_SOURCE_DIR}/chars.png
        ${CMAKE_CURRENT_SOURCE_DIR}/elrails.png
        ${CMAKE_CURRENT_SOURCE_DIR}/foundations.png
        ${CMAKE_CURRENT_SOURCE_DIR}/mono.png
        ${CMAKE_CURRENT_SOURCE_DIR}/oneway.png
        ${CMAKE_CURRENT_SOURCE_DIR}/openttdgui.png
        ${CMAKE_CURRENT_SOURCE_DIR}/openttdgui_build_tram.png
        ${CMAKE_CURRENT_SOURCE_DIR}/openttdgui_convert_road.png
        ${CMAKE_CURRENT_SOURCE_DIR}/openttdgui_convert_tram.png
        ${CMAKE_CURRENT_SOURCE_DIR}/openttdgui_group_livery.png
        ${CMAKE_CURRENT_SOURCE_DIR}/roadstops.png
        ${CMAKE_CURRENT_SOURCE_DIR}/signals.png
        ${CMAKE_CURRENT_SOURCE_DIR}/sloped_tracks.png
        ${CMAKE_CURRENT_SOURCE_DIR}/tramtracks.png
        ${CMAKE_CURRENT_SOURCE_DIR}/tramtracks_bare_depot.png
        ${CMAKE_CURRENT_SOURCE_DIR}/tunnel_portals.png
        # LNG files
        ${CMAKE_CURRENT_SOURCE_DIR}/lang/english.lng
    )
endif()
