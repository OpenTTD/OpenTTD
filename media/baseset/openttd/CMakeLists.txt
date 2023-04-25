# In case nmlc is found, generate the GRF.
# Otherwise, just use them from the cache (read: git).
# This is mainly because not many people have this tool installed,
# so it is cheaper to cache them in git, and only regenerate when you are
# working on it / have the tools installed.
if(NML_FOUND)
    include(CreateGrfCommand)
    create_grf_command()
endif()
