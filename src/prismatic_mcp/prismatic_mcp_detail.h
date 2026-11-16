/*
 * Part of korczis's PRIVATE RESEARCH FORK of OpenTTD. RESEARCH-ONLY.
 * Compiled only when OTTD_PRISMATIC_MCP_SERVER is defined.
 */

/** @file prismatic_mcp_detail.h Pure, side-effect-free MCP helpers (unit-testable, no game state). */

#ifndef PRISMATIC_MCP_DETAIL_H
#define PRISMATIC_MCP_DETAIL_H

#ifdef OTTD_PRISMATIC_MCP_SERVER

#include <cstdint>
#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <string_view>

/**
 * Pure helpers with no OpenTTD or game-state dependency, factored out of the
 * server so the security-critical logic (digest binding, header parsing, bearer
 * extraction, action-type gating) can be unit-tested in isolation. Nothing here
 * touches a pool, a socket, or a global.
 */
namespace PrismaticMCP::detail {

/** FNV-1a 64-bit over `tool + "|" + canonical_args`, rendered as 16 hex chars.
 * Deterministic; identical input → identical digest; any difference → different digest. */
inline std::string ArgDigestHex(std::string_view tool, std::string_view canonical_args)
{
	uint64_t h = 1469598103934665603ull;
	auto mix = [&h](std::string_view s) { for (unsigned char c : s) { h ^= c; h *= 1099511628211ull; } };
	mix(tool);
	mix("|");
	mix(canonical_args);
	static const char *d = "0123456789abcdef";
	std::string out(16, '0');
	for (int i = 15; i >= 0; i--) { out[i] = d[h & 0xF]; h >>= 4; }
	return out;
}

/** Why session- and world-bound decision state is being dropped. */
enum class ResetReason : uint8_t {
	ServerStopped,   ///< the MCP server stopped; the world goes on
	TokenRotated,    ///< the bearer token changed; the world goes on
	GameModeChanged, ///< the game mode changed (menu, editor, a game): the world is gone
	WorldReplaced,   ///< a new or loaded world (the tick counter went back): the world is gone
};

/** What a reset drops. */
struct ResetEffect {
	bool clear_approvals;     ///< every approval, granted or pending
	bool invalidate_awaiting; ///< decisions still waiting for their commands' results
};

/**
 * The rule: an approval binds consent to one session of one world, so every reset drops it.
 * A posted command still runs in the same world after a stop or a token rotation and its result
 * still arrives; only when the world is gone will it never come, so only then are waiting
 * decisions closed.
 */
constexpr ResetEffect EffectOf(ResetReason r)
{
	bool world_gone = r == ResetReason::GameModeChanged || r == ResetReason::WorldReplaced;
	return ResetEffect{true, world_gone};
}

/** @return The words a reset is logged and recorded with. */
constexpr std::string_view ReasonText(ResetReason r)
{
	switch (r) {
		case ResetReason::ServerStopped: return "server stopped";
		case ResetReason::TokenRotated: return "token rotated";
		case ResetReason::GameModeChanged: return "game mode changed";
		case ResetReason::WorldReplaced: return "a new or loaded world";
	}
	return "reset";
}

/** @return Whether a reset that invalidates waiting decisions closes a decision with this verdict. */
inline bool ClosedByReset(std::string_view verdict)
{
	return verdict == "PENDING_OBSERVATION";
}

/** The fixed typed action types this build can execute, the one place they are named; the game
 * commands add one "command.<Name>" type each, derived from the command table (prismatic_mcp_commands).
 * The validator, openttd.decision.list_action_types and the decision tools' input schemas read both. */
inline constexpr std::array<std::string_view, 5> ACTION_TYPES = {"game.set_pause", "ai.set_setting", "ai.start", "ai.stop", "ai.reload"};

/** Whether this build can execute a typed action of this type. Anything else is rejected. */
inline bool IsKnownActionType(std::string_view type)
{
	if (std::find(ACTION_TYPES.begin(), ACTION_TYPES.end(), type) != ACTION_TYPES.end()) return true;
	/* "command.<Name>": a game command by its Cmd* name; whether it exists is the catalogue's call. */
	if (!type.starts_with("command.") || type.size() == 8) return false;
	for (char c : type.substr(8)) {
		if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))) return false;
	}
	return true;
}

/** Case-insensitive Content-Length parse from a raw header block. nullopt if absent/invalid. */
inline std::optional<size_t> ParseContentLength(std::string_view header_block)
{
	std::string lower(header_block);
	for (char &c : lower) c = (char)((c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c);
	size_t p = lower.find("content-length:");
	if (p == std::string::npos) return std::nullopt;
	p += 15; // strlen("content-length:")
	while (p < lower.size() && (lower[p] == ' ' || lower[p] == '\t')) p++;
	size_t start = p;
	while (p < lower.size() && lower[p] >= '0' && lower[p] <= '9') p++;
	if (p == start) return std::nullopt;
	size_t value = 0;
	for (size_t i = start; i < p; i++) value = value * 10 + (size_t)(lower[i] - '0');
	return value;
}

/** Extract the token from an `Authorization: Bearer <tok>` value (case-insensitive scheme).
 * Returns empty view if the scheme is missing. */
inline std::string_view ExtractBearer(std::string_view authorization)
{
	if (authorization.size() <= 7) return {};
	std::string_view scheme = authorization.substr(0, 7);
	if (scheme != "Bearer " && scheme != "bearer ") return {};
	std::string_view tok = authorization.substr(7);
	while (!tok.empty() && (tok.front() == ' ' || tok.front() == '\t')) tok.remove_prefix(1);
	while (!tok.empty() && (tok.back() == ' ' || tok.back() == '\t' || tok.back() == '\r')) tok.remove_suffix(1);
	return tok;
}

/** Constant-time equality of two equal-length-or-not hex strings. */
inline bool ConstantTimeEquals(std::string_view a, std::string_view b)
{
	if (a.size() != b.size()) return false;
	unsigned diff = 0;
	for (size_t i = 0; i < a.size(); i++) diff |= (unsigned)(a[i] ^ b[i]);
	return diff == 0;
}

/**
 * Has the world changed under the server since the last poll? The tick counter only moves forward
 * within one game; a new game or a load restarts it, so a smaller value means another world, whose
 * approvals must not carry over (their expiry is counted in those ticks).
 */
inline bool WorldChanged(uint64_t last_tick, uint64_t now_tick)
{
	return now_tick < last_tick;
}

} // namespace PrismaticMCP::detail

#endif /* OTTD_PRISMATIC_MCP_SERVER */
#endif /* PRISMATIC_MCP_DETAIL_H */
