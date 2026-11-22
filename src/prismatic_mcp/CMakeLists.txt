# RESEARCH-ONLY embedded MCP server. Compiled only when OPTION_PRISMATIC_MCP_SERVER
# is ON (which cmake/Options.cmake gates on OPTION_RESEARCH_INSTRUMENTATION=ON and
# turns into -DOTTD_PRISMATIC_MCP_SERVER). When OFF, none of these sources are added
# to the build, so the feature has zero footprint. See research/prismatic-mcp/.
add_files(
    prismatic_mcp.h
    prismatic_mcp_ai.cpp
    prismatic_mcp_ai.h
    prismatic_mcp_api.cpp
    prismatic_mcp_api.h
    prismatic_mcp_commands.cpp
    prismatic_mcp_commands.h
    prismatic_mcp_detail.h
    prismatic_mcp_params.hpp
    prismatic_mcp_server.cpp
    prismatic_mcp_console.h
    prismatic_mcp_console.cpp
    prismatic_mcp_coverage.h
    prismatic_mcp_coverage.cpp
    prismatic_mcp_entities.h
    prismatic_mcp_entities.cpp
    prismatic_mcp_graph.h
    prismatic_mcp_graph.cpp
    prismatic_mcp_hooks.h
    prismatic_mcp_menu.h
    prismatic_mcp_menu.cpp
    prismatic_mcp_registry.h
    prismatic_mcp_registry.cpp
    prismatic_mcp_ui.h
    prismatic_mcp_text.h
    prismatic_mcp_ui.cpp
    prismatic_mcp_view.h
    prismatic_mcp_view.cpp
    prismatic_mcp_world.h
    prismatic_mcp_world.cpp
    CONDITION OPTION_PRISMATIC_MCP_SERVER
)
