# In case both NFORenum and GRFCodec are found, generate the GRF.
# Otherwise, just use them from the cache (read: git).
# This is mainly because not many people have both of these tools installed,
# so it is cheaper to cache them in git, and only regenerate when you are
# working on it / have the tools installed.
if(GRFCODEC_FOUND)
    include(CreateGrfCommand)
    create_grf_command(
        NFO_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/airports.nfo
                         ${CMAKE_CURRENT_SOURCE_DIR}/airport_preview.nfo
                         ${CMAKE_CURRENT_SOURCE_DIR}/aqueduct.nfo
                         ${CMAKE_CURRENT_SOURCE_DIR}/autorail.nfo
                         ${CMAKE_CURRENT_SOURCE_DIR}/canals.nfo
                         ${CMAKE_CURRENT_SOURCE_DIR}/chars.nfo
                         ${CMAKE_CURRENT_SOURCE_DIR}/elrails.nfo
                         ${CMAKE_CURRENT_SOURCE_DIR}/foundations.nfo
                         ${CMAKE_CURRENT_SOURCE_DIR}/mono.nfo
                         ${CMAKE_CURRENT_SOURCE_DIR}/oneway.nfo
                         ${CMAKE_CURRENT_SOURCE_DIR}/openttd.nfo
                         ${CMAKE_CURRENT_SOURCE_DIR}/openttdgui.nfo
                         ${CMAKE_CURRENT_SOURCE_DIR}/palette.nfo
                         ${CMAKE_CURRENT_SOURCE_DIR}/roadstops.nfo
                         ${CMAKE_CURRENT_SOURCE_DIR}/signals.nfo
                         ${CMAKE_CURRENT_SOURCE_DIR}/sloped_tracks.nfo
                         ${CMAKE_CURRENT_SOURCE_DIR}/tramtracks.nfo
                         ${CMAKE_CURRENT_SOURCE_DIR}/tunnel_portals.nfo
                         ${CMAKE_CURRENT_SOURCE_DIR}/2ccmap.nfo
        PNG_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/airports.png
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
    )
endif()
