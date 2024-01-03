# In case both NFORenum and GRFCodec are found, generate the GRF.
# Otherwise, just use them from the cache (read: git).
# This is mainly because not many people have both of these tools installed,
# so it is cheaper to cache them in git, and only regenerate when you are
# working on it / have the tools installed.
if(GRFCODEC_FOUND)
    include(CreateGrfCommand)
    create_grf_command(
        NFO_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/rivers/arctic.nfo
                         ${CMAKE_CURRENT_SOURCE_DIR}/rivers/rapids.nfo
                         ${CMAKE_CURRENT_SOURCE_DIR}/rivers/temperate.nfo
                         ${CMAKE_CURRENT_SOURCE_DIR}/rivers/toyland.nfo
                         ${CMAKE_CURRENT_SOURCE_DIR}/rivers/toyland_rapids.nfo
                         ${CMAKE_CURRENT_SOURCE_DIR}/rivers/tropic.nfo
                         ${CMAKE_CURRENT_SOURCE_DIR}/airports_orig_extra.nfo
                         ${CMAKE_CURRENT_SOURCE_DIR}/canals_extra.nfo
                         ${CMAKE_CURRENT_SOURCE_DIR}/chars_orig_extra.nfo
                         ${CMAKE_CURRENT_SOURCE_DIR}/fix_graphics.nfo
                         ${CMAKE_CURRENT_SOURCE_DIR}/fix_gui_icons.nfo
                         ${CMAKE_CURRENT_SOURCE_DIR}/orig_extra.nfo
                         ${CMAKE_CURRENT_SOURCE_DIR}/shore.nfo
        PNG_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/rivers/arctic_brown.png
                         ${CMAKE_CURRENT_SOURCE_DIR}/rivers/arctic_snowy.png
                         ${CMAKE_CURRENT_SOURCE_DIR}/rivers/rapids.png
                         ${CMAKE_CURRENT_SOURCE_DIR}/rivers/rapids_shading.png
                         ${CMAKE_CURRENT_SOURCE_DIR}/rivers/temperate.png
                         ${CMAKE_CURRENT_SOURCE_DIR}/rivers/toyland.png
                         ${CMAKE_CURRENT_SOURCE_DIR}/rivers/toyland_rapids.png
                         ${CMAKE_CURRENT_SOURCE_DIR}/rivers/toyland_rapids_shading.png
                         ${CMAKE_CURRENT_SOURCE_DIR}/rivers/tropic_desert.png
                         ${CMAKE_CURRENT_SOURCE_DIR}/rivers/tropic_forest.png
                         ${CMAKE_CURRENT_SOURCE_DIR}/fix_graphics.png
                         ${CMAKE_CURRENT_SOURCE_DIR}/fix_gui_icons.png
                         ${CMAKE_CURRENT_SOURCE_DIR}/shore.png
                         # We share some files with 'openttd' grf
                         ${CMAKE_CURRENT_SOURCE_DIR}/../openttd/airports.png
                         ${CMAKE_CURRENT_SOURCE_DIR}/../openttd/canals.png
                         ${CMAKE_CURRENT_SOURCE_DIR}/../openttd/chars.png
    )
endif()
