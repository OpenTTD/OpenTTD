/* $Id$ */

/** @file autoreplace_func.h Functions related to autoreplacing. */

#ifndef AUTOREPLACE_FUNC_H
#define AUTOREPLACE_FUNC_H

#include "autoreplace_type.h"
#include "player.h"

/**
 * Remove all engine replacement settings for the player.
 * @param  erl The renewlist for a given player.
 * @return The new renewlist for the player.
 */
void RemoveAllEngineReplacement(EngineRenewList *erl);

/**
 * Retrieve the engine replacement in a given renewlist for an original engine type.
 * @param  erl The renewlist to search in.
 * @param  engine Engine type to be replaced.
 * @return The engine type to replace with, or INVALID_ENGINE if no
 * replacement is in the list.
 */
EngineID EngineReplacement(EngineRenewList erl, EngineID engine, GroupID group);

/**
 * Add an engine replacement to the given renewlist.
 * @param erl The renewlist to add to.
 * @param old_engine The original engine type.
 * @param new_engine The replacement engine type.
 * @param flags The calling command flags.
 * @return 0 on success, CMD_ERROR on failure.
 */
CommandCost AddEngineReplacement(EngineRenewList *erl, EngineID old_engine, EngineID new_engine, GroupID group, uint32 flags);

/**
 * Remove an engine replacement from a given renewlist.
 * @param erl The renewlist from which to remove the replacement
 * @param engine The original engine type.
 * @param flags The calling command flags.
 * @return 0 on success, CMD_ERROR on failure.
 */
CommandCost RemoveEngineReplacement(EngineRenewList *erl, EngineID engine, GroupID group, uint32 flags);

/**
 * Remove all engine replacement settings for the given player.
 * @param p Player.
 */
static inline void RemoveAllEngineReplacementForPlayer(Player *p)
{
	RemoveAllEngineReplacement(&p->engine_renew_list);
}

/**
 * Retrieve the engine replacement for the given player and original engine type.
 * @param p Player.
 * @param engine Engine type.
 * @return The engine type to replace with, or INVALID_ENGINE if no
 * replacement is in the list.
 */
static inline EngineID EngineReplacementForPlayer(const Player *p, EngineID engine, GroupID group)
{
	return EngineReplacement(p->engine_renew_list, engine, group);
}

/**
 * Check if a player has a replacement set up for the given engine.
 * @param p Player.
 * @param  engine Engine type to be replaced.
 * @return true if a replacement was set up, false otherwise.
 */
static inline bool EngineHasReplacementForPlayer(const Player *p, EngineID engine, GroupID group)
{
	return EngineReplacementForPlayer(p, engine, group) != INVALID_ENGINE;
}

/**
 * Add an engine replacement for the player.
 * @param p Player.
 * @param old_engine The original engine type.
 * @param new_engine The replacement engine type.
 * @param flags The calling command flags.
 * @return 0 on success, CMD_ERROR on failure.
 */
static inline CommandCost AddEngineReplacementForPlayer(Player *p, EngineID old_engine, EngineID new_engine, GroupID group, uint32 flags)
{
	return AddEngineReplacement(&p->engine_renew_list, old_engine, new_engine, group, flags);
}

/**
 * Remove an engine replacement for the player.
 * @param p Player.
 * @param engine The original engine type.
 * @param flags The calling command flags.
 * @return 0 on success, CMD_ERROR on failure.
 */
static inline CommandCost RemoveEngineReplacementForPlayer(Player *p, EngineID engine, GroupID group, uint32 flags)
{
	return RemoveEngineReplacement(&p->engine_renew_list, engine, group, flags);
}

#endif /* AUTOREPLACE_FUNC_H */
