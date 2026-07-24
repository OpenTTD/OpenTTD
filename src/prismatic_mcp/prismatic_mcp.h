/*
 * This file is part of korczis's PRIVATE RESEARCH FORK of OpenTTD and is NOT
 * part of upstream OpenTTD. See research/prismatic-mcp/ and AGENTS.md.
 *
 * RESEARCH-ONLY, UNSTABLE, DEBUG-ONLY. Compiled only when the CMake option
 * OPTION_PRISMATIC_MCP_SERVER=ON (which itself requires
 * OPTION_RESEARCH_INSTRUMENTATION=ON) defines OTTD_PRISMATIC_MCP_SERVER.
 */

/** @file prismatic_mcp.h Embedded, loopback-only MCP server for OpenTTD (research-only). */

#ifndef PRISMATIC_MCP_H
#define PRISMATIC_MCP_H

#ifdef OTTD_PRISMATIC_MCP_SERVER

#include <cstdint>
#include <string>

/**
 * Lifecycle + control surface of the embedded MCP server. Every function here
 * runs on the OpenTTD main (game) thread; the server never touches game state
 * from any other thread. All entry points are no-ops unless the server has been
 * started. See research/prismatic-mcp/04-architecture-decision-record.md.
 */
namespace PrismaticMCP {

/** MCP protocol revision this server pins to (stable spec). */
inline constexpr std::string_view PROTOCOL_VERSION = "2025-11-25";
/** Default loopback port for the Streamable-HTTP endpoint. */
inline constexpr uint16_t DEFAULT_PORT = 8731;

/** Called once from openttd_main after NetworkStartUp(); allocates nothing heavy. */
void Init();
/** Called once from ShutdownGame(); stops the listener and clears the token. */
void Shutdown();
/** Called every frame from GameLoop() on the main thread; services the listener. */
void Poll();
/** Invalidate cached world snapshots on a game-mode transition (new/load/menu). */
void OnGameModeChanged();

/** Start the loopback listener on @p port (0 = DEFAULT_PORT). Returns false on error. */
bool Start(uint16_t port, std::string &error);
/** Stop the listener; idempotent. */
void Stop();
/** True while the listener is bound and accepting. */
bool IsRunning();

/** The full endpoint URL, e.g. "http://127.0.0.1:8731/mcp" (empty if stopped). */
std::string GetEndpoint();
/** The current bearer token in hex (empty if stopped). For the local operator only. */
std::string GetToken();
/** Generate a fresh token and invalidate every existing session. */
void RotateToken();
/** One machine-readable status line for the console (never contains the token). */
std::string GetStatusLine();

} // namespace PrismaticMCP

#endif /* OTTD_PRISMATIC_MCP_SERVER */
#endif /* PRISMATIC_MCP_H */
