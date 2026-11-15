# RESEARCH-ONLY embedded MCP server. Compiled only when OPTION_PRISMATIC_MCP_SERVER
# is ON (which cmake/Options.cmake gates on OPTION_RESEARCH_INSTRUMENTATION=ON and
# turns into -DOTTD_PRISMATIC_MCP_SERVER). When OFF, none of these sources are added
# to the build, so the feature has zero footprint. See research/prismatic-mcp/.
add_files(
    prismatic_mcp.h
    prismatic_mcp_server.cpp
    prismatic_mcp_hooks.h
    prismatic_mcp_ui.h
    prismatic_mcp_ui.cpp
    prismatic_mcp_view.h
    prismatic_mcp_view.cpp
    prismatic_mcp_world.h
    prismatic_mcp_world.cpp
    CONDITION OPTION_PRISMATIC_MCP_SERVER
)
