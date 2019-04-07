# In case both NFORenum and GRFCodec are found, generate the GRF.
# Otherwise, just use them from the cache (read: git).
# This is mainly because not many people have both of these tools installed,
# so it is cheaper to cache them in git, and only regenerate when you are
# working on it / have the tools installed.
if (GRFCODEC_FOUND)
    include(CreateGrfCommand)
    create_grf_command(
        # We share some files with 'openttd' grf
        ${CMAKE_CURRENT_SOURCE_DIR}/../openttd/airports.png
        ${CMAKE_CURRENT_SOURCE_DIR}/../openttd/canals.png
        ${CMAKE_CURRENT_SOURCE_DIR}/../openttd/chars.png
    )
endif (GRFCODEC_FOUND)
