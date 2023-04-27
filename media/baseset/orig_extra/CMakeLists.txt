# In case nmlc is found, generate the GRF.
# Otherwise, just use them from the cache (read: git).
# This is mainly because not many people have this tool installed,
# so it is cheaper to cache them in git, and only regenerate when you are
# working on it / have the tools installed.
if(NML_FOUND)
    include(CreateGrfCommand)
    create_grf_command(
        # NML files
        ${CMAKE_CURRENT_SOURCE_DIR}/airports_orig_extra.nml
        ${CMAKE_CURRENT_SOURCE_DIR}/canals_extra.nml
        ${CMAKE_CURRENT_SOURCE_DIR}/chars_orig_extra.nml
        ${CMAKE_CURRENT_SOURCE_DIR}/fix_graphics.nml
        ${CMAKE_CURRENT_SOURCE_DIR}/fix_gui_icons.nml
        ${CMAKE_CURRENT_SOURCE_DIR}/orig_extra.nml
        ${CMAKE_CURRENT_SOURCE_DIR}/shore.nml
        ${CMAKE_CURRENT_SOURCE_DIR}/rivers/arctic.nml
        ${CMAKE_CURRENT_SOURCE_DIR}/rivers/rapids.nml
        ${CMAKE_CURRENT_SOURCE_DIR}/rivers/temperate.nml
        ${CMAKE_CURRENT_SOURCE_DIR}/rivers/toyland.nml
        ${CMAKE_CURRENT_SOURCE_DIR}/rivers/tropic.nml
        #PNG files
        ${CMAKE_CURRENT_SOURCE_DIR}/fix_graphics.png
        ${CMAKE_CURRENT_SOURCE_DIR}/fix_gui_icons.png
        ${CMAKE_CURRENT_SOURCE_DIR}/shore.png
        ${CMAKE_CURRENT_SOURCE_DIR}/rivers/arctic_brown.png
        ${CMAKE_CURRENT_SOURCE_DIR}/rivers/arctic_snowy.png
        ${CMAKE_CURRENT_SOURCE_DIR}/rivers/rapids.png
        ${CMAKE_CURRENT_SOURCE_DIR}/rivers/temperate.png
        ${CMAKE_CURRENT_SOURCE_DIR}/rivers/toyland.png
        ${CMAKE_CURRENT_SOURCE_DIR}/rivers/tropic_desert.png
        ${CMAKE_CURRENT_SOURCE_DIR}/rivers/tropic_forest.png
        # We share some files with 'openttd' grf
        ${CMAKE_CURRENT_SOURCE_DIR}/../openttd/airports.png
        ${CMAKE_CURRENT_SOURCE_DIR}/../openttd/canals.png
        ${CMAKE_CURRENT_SOURCE_DIR}/../openttd/chars.png
        # LNG files
        ${CMAKE_CURRENT_SOURCE_DIR}/lang/english.lng
    )
endif()
