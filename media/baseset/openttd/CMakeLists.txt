# In case both NFORenum and GRFCodec are found, generate the GRF.
# Otherwise, just use them from the cache (read: git).
# This is mainly because not many people have both of these tools installed,
# so it is cheaper to cache them in git, and only regenerate when you are
# working on it / have the tools installed.
if (NFORENUM_FOUND AND GRFCODEC_FOUND)
    include(CreateGrfCommand REQUIRED)
    create_grf_command()
endif (NFORENUM_FOUND AND GRFCODEC_FOUND)
