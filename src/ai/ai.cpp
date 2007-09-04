/* $Id$ */

#include "../stdafx.h"
#include "../openttd.h"
#include "../variables.h"
#include "../command.h"
#include "../network/network.h"
#include "../helpers.hpp"
#include "ai.h"
#include "default/default.h"

/**
 * Dequeues commands put in the queue via AI_PutCommandInQueue.
 */
static void AI_DequeueCommands(PlayerID player)
{
	AICommand *com, *entry_com;

	entry_com = _ai_player[player].queue;

	/* It happens that DoCommandP issues a new DoCommandAI which adds a new command
	 *  to this very same queue (don't argue about this, if it currently doesn't
	 *  happen I can tell you it will happen with AIScript -- TrueLight). If we
	 *  do not make the queue NULL, that commands will be dequeued immediatly.
	 *  Therefor we safe the entry-point to entry_com, and make the queue NULL, so
	 *  the new queue can be safely built up. */
	_ai_player[player].queue = NULL;
	_ai_player[player].queue_tail = NULL;

	/* Dequeue all commands */
	while ((com = entry_com) != NULL) {
		_current_player = player;

		_cmd_text = com->text;
		DoCommandP(com->tile, com->p1, com->p2, com->callback, com->procc);

		/* Free item */
		entry_com = com->next;
		free(com->text);
		free(com);
	}
}

/**
 * Needed for SP; we need to delay DoCommand with 1 tick, because else events
 *  will make infinite loops (AIScript).
 */
static void AI_PutCommandInQueue(PlayerID player, TileIndex tile, uint32 p1, uint32 p2, uint procc, CommandCallback* callback)
{
	AICommand *com;

	if (_ai_player[player].queue_tail == NULL) {
		/* There is no item in the queue yet, create the queue */
		_ai_player[player].queue = MallocT<AICommand>(1);
		_ai_player[player].queue_tail = _ai_player[player].queue;
	} else {
		/* Add an item at the end */
		_ai_player[player].queue_tail->next = MallocT<AICommand>(1);
		_ai_player[player].queue_tail = _ai_player[player].queue_tail->next;
	}

	/* This is our new item */
	com = _ai_player[player].queue_tail;

	/* Assign the info */
	com->tile  = tile;
	com->p1    = p1;
	com->p2    = p2;
	com->procc = procc;
	com->callback = callback;
	com->next  = NULL;
	com->text  = NULL;

	/* Copy the cmd_text, if needed */
	if (_cmd_text != NULL) {
		com->text = strdup(_cmd_text);
		_cmd_text = NULL;
	}
}

/**
 * Executes a raw DoCommand for the AI.
 */
CommandCost AI_DoCommandCc(TileIndex tile, uint32 p1, uint32 p2, uint32 flags, uint procc, CommandCallback* callback)
{
	PlayerID old_lp;
	CommandCost res;
	const char* tmp_cmdtext;

	/* If you enable DC_EXEC with DC_QUERY_COST you are a really strange
	 *   person.. should we check for those funny jokes?
	 */

	/* The test already resets _cmd_text, so backup the pointer */
	tmp_cmdtext = _cmd_text;

	/* First, do a test-run to see if we can do this */
	res = DoCommand(tile, p1, p2, flags & ~DC_EXEC, procc);
	/* The command failed, or you didn't want to execute, or you are quering, return */
	if (CmdFailed(res) || !(flags & DC_EXEC) || (flags & DC_QUERY_COST)) {
		return res;
	}

	/* Restore _cmd_text */
	_cmd_text = tmp_cmdtext;

	/* If we did a DC_EXEC, and the command did not return an error, execute it
	 * over the network */
	if (flags & DC_NO_WATER) procc |= CMD_NO_WATER;

	/* NetworkSend_Command needs _local_player to be set correctly, so
	 * adjust it, and put it back right after the function */
	old_lp = _local_player;
	_local_player = _current_player;

#ifdef ENABLE_NETWORK
	/* Send the command */
	if (_networking) {
		/* Network is easy, send it to his handler */
		NetworkSend_Command(tile, p1, p2, procc, callback);
	} else {
#else
	{
#endif
		/* If we execute BuildCommands directly in SP, we have a big problem with events
		 *  so we need to delay is for 1 tick */
		AI_PutCommandInQueue(_current_player, tile, p1, p2, procc, callback);
	}

	/* Set _local_player back */
	_local_player = old_lp;

	return res;
}


CommandCost AI_DoCommand(TileIndex tile, uint32 p1, uint32 p2, uint32 flags, uint procc)
{
	return AI_DoCommandCc(tile, p1, p2, flags, procc, NULL);
}


/**
 * Run 1 tick of the AI. Don't overdo it, keep it realistic.
 */
static void AI_RunTick(PlayerID player)
{
	extern void AiNewDoGameLoop(Player *p);

	Player *p = GetPlayer(player);
	_current_player = player;

	if (_patches.ainew_active) {
		AiNewDoGameLoop(p);
	} else {
		/* Enable all kind of cheats the old AI needs in order to operate correctly... */
		_is_old_ai_player = true;
		AiDoGameLoop(p);
		_is_old_ai_player = false;
	}
}


/**
 * The gameloop for AIs.
 *  Handles one tick for all the AIs.
 */
void AI_RunGameLoop()
{
	/* Don't do anything if ai is disabled */
	if (!_ai.enabled) return;

	/* Don't do anything if we are a network-client, or the AI has been disabled */
	if (_networking && (!_network_server || !_patches.ai_in_multiplayer)) return;

	/* New tick */
	_ai.tick++;

	/* Make sure the AI follows the difficulty rule.. */
	assert(_opt.diff.competitor_speed <= 4);
	if ((_ai.tick & ((1 << (4 - _opt.diff.competitor_speed)) - 1)) != 0) return;

	/* Check for AI-client (so joining a network with an AI) */
	if (!_networking || _network_server) {
		/* Check if we want to run AIs (server or SP only) */
		const Player* p;

		FOR_ALL_PLAYERS(p) {
			if (p->is_active && p->is_ai) {
				/* This should always be true, else something went wrong... */
				assert(_ai_player[p->index].active);

				/* Run the script */
				AI_DequeueCommands(p->index);
				AI_RunTick(p->index);
			}
		}
	}

	_current_player = OWNER_NONE;
}

/**
 * A new AI sees the day of light. You can do here what ever you think is needed.
 */
void AI_StartNewAI(PlayerID player)
{
	assert(IsValidPlayer(player));

	/* Called if a new AI is booted */
	_ai_player[player].active = true;
}

/**
 * This AI player died. Give it some chance to make a final puf.
 */
void AI_PlayerDied(PlayerID player)
{
	/* Called if this AI died */
	_ai_player[player].active = false;
}

/**
 * Initialize some AI-related stuff.
 */
void AI_Initialize()
{
	/* First, make sure all AIs are DEAD! */
	AI_Uninitialize();

	memset(&_ai, 0, sizeof(_ai));
	memset(&_ai_player, 0, sizeof(_ai_player));

	_ai.enabled = true;
}

/**
 * Deinitializer for AI-related stuff.
 */
void AI_Uninitialize()
{
	const Player* p;

	FOR_ALL_PLAYERS(p) {
		if (p->is_active && p->is_ai) AI_PlayerDied(p->index);
	}
}
