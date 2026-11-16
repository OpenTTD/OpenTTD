/*
 * This file is part of korczis's research fork of OpenTTD and is NOT part of upstream OpenTTD.
 * RESEARCH-ONLY / UNSTABLE.
 */

/** @file prismatic_mcp_registry.h Every MCP server announces itself in a shared instance registry, so a cluster of instances is discoverable. */

#ifndef PRISMATIC_MCP_REGISTRY_H
#define PRISMATIC_MCP_REGISTRY_H

#ifdef OTTD_PRISMATIC_MCP_SERVER

#include <string>

namespace PrismaticMCP::Registry {

/**
 * Write (or rewrite) this process's record in the instance registry: one owner-only JSON file per
 * instance in $PRISMATIC_MCP_REGISTRY (default ~/.local/state/prismatic-mcp/instances), schema
 * mcp-instance/v1 (tools/research/schema/mcp-instance.v1.schema.json). The record names the
 * endpoint, what kind of process this is (server, client, game), and the path of the token file
 * when one was written; it never holds the token itself.
 * @param endpoint The MCP endpoint URL.
 * @param port The MCP port (part of the record's name).
 * @param token_file Path of the 0600 token file, or empty when none was written.
 */
void Announce(const std::string &endpoint, uint16_t port, const std::string &token_file = {});

/**
 * Add the token file's path to the record Announce() wrote (the console's `prismatic_mcp start
 * <port> <token-file>` writes the file after the server started). No-op before an Announce().
 * @param token_file Path of the 0600 token file.
 */
void AnnounceTokenFile(const std::string &token_file);

/** Remove this process's record, if it wrote one. */
void Withdraw();

} // namespace PrismaticMCP::Registry

#endif /* OTTD_PRISMATIC_MCP_SERVER */

#endif /* PRISMATIC_MCP_REGISTRY_H */
