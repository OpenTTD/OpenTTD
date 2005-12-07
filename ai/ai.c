/* $Id$ */

#include "../stdafx.h"
#include "../openttd.h"
#include "../variables.h"
#include "../command.h"
#include "../network.h"
#include "../debug.h"
#include "ai.h"
#include "default/default.h"
#include "../string.h"

/* Here we define the events */
#define DEF_EVENTS
#include "ai_event.h"
#undef DEF_EVENTS

/* Here we keep track of all commands that are called via AI_DoCommandChecked,
 *  in special we save the unique_id here. Now this id is given back
 *  when the command fails or succeeds and is detected as added in this storage. */
static AICommand *command_uid[MAX_PLAYERS];
static AICommand *command_uid_tail[MAX_PLAYERS];
static uint uids[MAX_PLAYERS];

/**
 * Dequeues commands put in the queue via AI_PutCommandInQueue.
 */
void AI_DequeueCommands(byte player)
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

		/* Copy the DP back in place */
		memcpy(_decode_parameters, com->dp, sizeof(com->dp));
		DoCommandP(com->tile, com->p1, com->p2, NULL, com->procc);

		/* Free item */
		entry_com = com->next;
		free(com);
	}
}

/**
 * Needed for SP; we need to delay DoCommand with 1 tick, because else events
 *  will make infinite loops (AIScript).
 */
void AI_PutCommandInQueue(byte player, uint tile, uint32 p1, uint32 p2, uint procc)
{
	AICommand *com;

	if (_ai_player[player].queue_tail == NULL) {
		/* There is no item in the queue yet, create the queue */
		_ai_player[player].queue = malloc(sizeof(AICommand));
		_ai_player[player].queue_tail = _ai_player[player].queue;
	} else {
		/* Add an item at the end */
		_ai_player[player].queue_tail->next = malloc(sizeof(AICommand));
		_ai_player[player].queue_tail = _ai_player[player].queue_tail->next;
	}

	/* This is our new item */
	com = _ai_player[player].queue_tail;

	/* Assign the info */
	com->tile  = tile;
	com->p1    = p1;
	com->p2    = p2;
	com->procc = procc;
	com->next  = NULL;

	/* Copy the decode_parameters */
	memcpy(com->dp, _decode_parameters, sizeof(com->dp));
}

/**
 * Executes a raw DoCommand for the AI.
 */
int32 AI_DoCommand(uint tile, uint32 p1, uint32 p2, uint32 flags, uint procc)
{
	PlayerID old_lp;
	int32 res = 0;

	/* If you enable DC_EXEC with DC_QUERY_COST you are a really strange
	 *   person.. should we check for those funny jokes?
	 */

	/* First, do a test-run to see if we can do this */
	res = DoCommandByTile(tile, p1, p2, flags & ~DC_EXEC, procc);
	/* The command failed, or you didn't want to execute, or you are quering, return */
	if ((CmdFailed(res)) || !(flags & DC_EXEC) || (flags & DC_QUERY_COST))
		return res;

	/* If we did a DC_EXEC, and the command did not return an error, execute it
	    over the network */
	if (flags & DC_AUTO)                  procc |= CMD_AUTO;
	if (flags & DC_NO_WATER)              procc |= CMD_NO_WATER;

	/* NetworkSend_Command needs _local_player to be set correctly, so
	    adjust it, and put it back right after the function */
	old_lp = _local_player;
	_local_player = _current_player;

	/* Send the command */
	if (_networking)
		/* Network is easy, send it to his handler */
		NetworkSend_Command(tile, p1, p2, procc, NULL);
	else
		/* If we execute BuildCommands directly in SP, we have a big problem with events
		 *  so we need to delay is for 1 tick */
		AI_PutCommandInQueue(_current_player, tile, p1, p2, procc);

	/* Set _local_player back */
	_local_player = old_lp;

	return res;
}

int32 AI_DoCommandChecked(uint tile, uint32 p1, uint32 p2, uint32 flags, uint procc)
{
	AICommand *new;
	uint unique_id = uids[_current_player]++;
	int32 res;

	res = AI_DoCommand(tile, p1, p2, flags & ~DC_EXEC, procc);
	if (CmdFailed(res))
		return CMD_ERROR;

	/* Save the command and his things, together with the unique_id */
	new = malloc(sizeof(AICommand));
	new->tile  = tile;
	new->p1    = p1;
	new->p2    = p2;
	new->procc = procc;
	new->next  = NULL;
	new->dp[0] = unique_id;

	/* Add it to the back of the list */
	if (command_uid_tail[_current_player] == NULL)
		command_uid_tail[_current_player] = new;
	else
		command_uid_tail[_current_player]->next = new;

	if (command_uid[_current_player] == NULL)
		command_uid[_current_player] = new;

	AI_DoCommand(tile, p1, p2, flags, procc);
	return unique_id;
}

/**
 * A command is executed for real, and is giving us his result (failed yes/no). Inform the AI with it via
 *  an event.Z
 */
void AI_CommandResult(uint32 cmd, uint32 p1, uint32 p2, TileIndex tile, bool succeeded)
{
	AICommand *command = command_uid[_current_player];

	if (command == NULL)
		return;

	if (command->procc != (cmd & 0xFF) || command->p1 != p1 || command->p2 != p2 || command->tile != tile) {
		/* If we come here, we see a command that isn't at top in our list. This is okay, if the command isn't
		 *  anywhere else in our list, else we have a big problem.. so check for that. If it isn't in our list,
		 *  it is okay, else, drop the game.
		 * Why do we have a big problem in the case it is in our list? Simply, we have a command sooner in
		 *  our list that wasn't triggered to be failed or succeeded, so it is sitting there without an
		 *  event, so the script will never know if it succeeded yes/no, so it can hang.. this is VERY bad
		 *  and should never ever happen. */
		while (command != NULL && (command->procc != (cmd & 0xFF) || command->p1 != p1 || command->p2 != p2 || command->tile != tile)) {
			command = command->next;
		}
		assert(command == NULL);
		return;
	}

	command_uid[_current_player] = command_uid[_current_player]->next;
	if (command_uid[_current_player] == NULL)
		command_uid_tail[_current_player] = NULL;

	ai_event(_current_player, succeeded ? ottd_Event_CommandSucceeded : ottd_Event_CommandFailed, tile, command->dp[0]);
	free(command);
}

/**
 * Run 1 tick of the AI. Don't overdo it, keep it realistic.
 */
static void AI_RunTick(PlayerID player)
{
	_current_player = player;

#ifdef GPMI
	if (_ai.gpmi) {
		gpmi_call_RunTick(_ai_player[player].module, _frame_counter);
		return;
	}
#endif /* GPMI */

	{
		extern void AiNewDoGameLoop(Player *p);

		Player *p = GetPlayer(player);

		if (_patches.ainew_active) {
			AiNewDoGameLoop(p);
		} else {
			/* Enable all kind of cheats the old AI needs in order to operate correctly... */
			_is_old_ai_player = true;
			AiDoGameLoop(p);
			_is_old_ai_player = false;
		}
	}
}


/**
 * The gameloop for AIs.
 *  Handles one tick for all the AIs.
 */
void AI_RunGameLoop(void)
{
	/* Don't do anything if ai is disabled */
	if (!_ai.enabled) return;

	/* Don't do anything if we are a network-client
	 *  (too bad when a client joins, he thinks the AIs are real, so it wants to control
	 *   them.. this avoids that, while loading a network game in singleplayer, does make
	 *   the AIs to continue ;))
	 */
	if (_networking && !_network_server && !_ai.network_client)
		return;

	/* New tick */
	_ai.tick++;

	/* Make sure the AI follows the difficulty rule.. */
	assert(_opt.diff.competitor_speed <= 4);
	if ((_ai.tick & ((1 << (4 - _opt.diff.competitor_speed)) - 1)) != 0)
		return;

	/* Check for AI-client (so joining a network with an AI) */
	if (_ai.network_client && _ai_player[_ai.network_playas].active) {
		/* Run the script */
		AI_DequeueCommands(_ai.network_playas);
		AI_RunTick(_ai.network_playas);
	} else if (!_networking || _network_server) {
		/* Check if we want to run AIs (server or SP only) */
		Player *p;

		FOR_ALL_PLAYERS(p) {
			if (p->is_active && p->is_ai && _ai_player[p->index].active) {
				/* Run the script */
				AI_DequeueCommands(p->index);
				AI_RunTick(p->index);
			}
		}
	}

	_current_player = OWNER_NONE;
}

#ifdef GPMI

void AI_ShutdownAIControl(bool with_error)
{
	if (_ai.gpmi_mod != NULL)
		gpmi_mod_unload(_ai.gpmi_mod);
	if (_ai.gpmi_pkg != NULL)
		gpmi_pkg_unload(_ai.gpmi_pkg);

	if (with_error) {
		DEBUG(ai, 0)("[AI] Failed to load AI Control, switching back to built-in AIs..");
		_ai.gpmi = false;
	}
}

void (*ottd_GetNextAIData)(char **library, char **param);
void (*ottd_SetAIParam)(char *param);

void AI_LoadAIControl(void)
{
	/* Load module */
	_ai.gpmi_mod = gpmi_mod_load("ottd_ai_control_mod", NULL);
	if (_ai.gpmi_mod == NULL) {
		AI_ShutdownAIControl(true);
		return;
	}

	/* Load package */
	if (gpmi_pkg_load("ottd_ai_control_pkg", 0, NULL, NULL, &_ai.gpmi_pkg)) {
		AI_ShutdownAIControl(true);
		return;
	}

	/* Now link all the functions */
	{
		ottd_GetNextAIData = gpmi_pkg_resolve(_ai.gpmi_pkg, "ottd_GetNextAIData");
		ottd_SetAIParam = gpmi_pkg_resolve(_ai.gpmi_pkg, "ottd_SetAIParam");

		if (ottd_GetNextAIData == NULL || ottd_SetAIParam == NULL)
			AI_ShutdownAIControl(true);
	}

	ottd_SetAIParam(_ai.gpmi_param);
}

/**
 * Dump an entry of the GPMI error stack (callback routine). This helps the user to trace errors back to their roots.
 */
void AI_PrintErrorStack(gpmi_err_stack_t *entry, char *string)
{
	DEBUG(ai, 0)("%s", string);
}

#endif /* GPMI */

/**
 * A new AI sees the day of light. You can do here what ever you think is needed.
 */
void AI_StartNewAI(PlayerID player)
{
	assert(player < MAX_PLAYERS);

#ifdef GPMI
	/* Keep this in a different IF, because the function can turn _ai.gpmi off!! */
	if (_ai.gpmi && _ai.gpmi_mod == NULL)
		AI_LoadAIControl();

	if (_ai.gpmi) {
		char *library = NULL;
		char *params = NULL;

		ottd_GetNextAIData(&library, &params);
		gpmi_error_stack_enable = 1;

		if (library != NULL) {
			_ai_player[player].module = gpmi_mod_load(library, params);
			free(library);
		}
		if (params != NULL)
			free(params);

		if (_ai_player[player].module == NULL) {
			DEBUG(ai, 0)("[AI] Failed to load AI, aborting. GPMI error stack:");
			gpmi_err_stack_process_str(AI_PrintErrorStack);
			return;
		}
		gpmi_error_stack_enable = 0;

	}
#endif /* GPMI */

	/* Called if a new AI is booted */
	_ai_player[player].active = true;
}

/**
 * This AI player died. Give it some chance to make a final puf.
 */
void AI_PlayerDied(PlayerID player)
{
	if (_ai.network_client && _ai.network_playas == player)
		_ai.network_playas = OWNER_SPECTATOR;

	/* Called if this AI died */
	_ai_player[player].active = false;

	if (command_uid[player] != NULL) {
		while (command_uid[player] != NULL) {
			AICommand *command = command_uid[player];
			command_uid[player] = command->next;
			free(command);
		}

		command_uid[player] = NULL;
		command_uid_tail[player] = NULL;
	}

#ifdef GPMI
	if (_ai_player[player].module != NULL)
		gpmi_mod_unload(_ai_player[player].module);
#endif /* GPMI */
}

/**
 * Initialize some AI-related stuff.
 */
void AI_Initialize(void)
{
	bool tmp_ai_network_client = _ai.network_client;
	bool tmp_ai_gpmi = _ai.gpmi;
#ifdef GPMI
	char *tmp_ai_gpmi_param = strdup(_ai.gpmi_param);
#endif /* GPMI */

	/* First, make sure all AIs are DEAD! */
	AI_Uninitialize();

	memset(&_ai, 0, sizeof(_ai));
	memset(_ai_player, 0, sizeof(_ai_player));
	memset(uids, 0, sizeof(uids));
	memset(command_uid, 0, sizeof(command_uid));
	memset(command_uid_tail, 0, sizeof(command_uid_tail));

	_ai.network_client = tmp_ai_network_client;
	_ai.network_playas = OWNER_SPECTATOR;
	_ai.enabled = true;
	_ai.gpmi = tmp_ai_gpmi;
#ifdef GPMI
	ttd_strlcpy(_ai.gpmi_param, tmp_ai_gpmi_param, sizeof(_ai.gpmi_param));
	free(tmp_ai_gpmi_param);
#endif /* GPMI */
}

/**
 * Deinitializer for AI-related stuff.
 */
void AI_Uninitialize(void)
{
	Player* p;

	FOR_ALL_PLAYERS(p) {
		if (p->is_active && p->is_ai && _ai_player[p->index].active) AI_PlayerDied(p->index);
	}

#ifdef GPMI
	AI_ShutdownAIControl(false);
#endif /* GPMI */
}
