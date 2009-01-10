/* $Id$ */

/** @file ai.cpp Base for all AIs. */

#include "../stdafx.h"
#include "../openttd.h"
#include "../variables.h"
#include "../command_func.h"
#include "../network/network.h"
#include "../core/alloc_func.hpp"
#include "../company_func.h"
#include "../company_base.h"
#include "ai.h"
#include "default/default.h"
#include "trolly/trolly.h"
#include "../signal_func.h"

AIStruct _ai;
AICompany _ai_company[MAX_COMPANIES];

/**
 * Dequeues commands put in the queue via AI_PutCommandInQueue.
 */
static void AI_DequeueCommands(CompanyID company)
{
	AICommand *com, *entry_com;

	entry_com = _ai_company[company].queue;

	/* It happens that DoCommandP issues a new DoCommandAI which adds a new command
	 *  to this very same queue (don't argue about this, if it currently doesn't
	 *  happen I can tell you it will happen with AIScript -- TrueLight). If we
	 *  do not make the queue NULL, that commands will be dequeued immediatly.
	 *  Therefor we safe the entry-point to entry_com, and make the queue NULL, so
	 *  the new queue can be safely built up. */
	_ai_company[company].queue = NULL;
	_ai_company[company].queue_tail = NULL;

	/* Dequeue all commands */
	while ((com = entry_com) != NULL) {
		_current_company = company;

		DoCommandP(com->tile, com->p1, com->p2, com->cmd, com->callback, com->text);

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
static void AI_PutCommandInQueue(CompanyID company, TileIndex tile, uint32 p1, uint32 p2, uint32 cmd, CommandCallback *callback, const char *text = NULL)
{
	AICommand *com;

	if (_ai_company[company].queue_tail == NULL) {
		/* There is no item in the queue yet, create the queue */
		_ai_company[company].queue = MallocT<AICommand>(1);
		_ai_company[company].queue_tail = _ai_company[company].queue;
	} else {
		/* Add an item at the end */
		_ai_company[company].queue_tail->next = MallocT<AICommand>(1);
		_ai_company[company].queue_tail = _ai_company[company].queue_tail->next;
	}

	/* This is our new item */
	com = _ai_company[company].queue_tail;

	/* Assign the info */
	com->tile  = tile;
	com->p1    = p1;
	com->p2    = p2;
	com->cmd = cmd;
	com->callback = callback;
	com->next  = NULL;
	com->text  = text == NULL ? NULL : strdup(text);
}

/**
 * Executes a raw DoCommand for the AI.
 */
CommandCost AI_DoCommandCc(TileIndex tile, uint32 p1, uint32 p2, uint32 flags, uint32 cmd, CommandCallback *callback, const char *text)
{
	CompanyID old_local_company;
	CommandCost res;

	/* If you enable DC_EXEC with DC_QUERY_COST you are a really strange
	 *   person.. should we check for those funny jokes?
	 */

	/* First, do a test-run to see if we can do this */
	res = DoCommand(tile, p1, p2, flags & ~DC_EXEC, cmd, text);
	/* The command failed, or you didn't want to execute, or you are quering, return */
	if (CmdFailed(res) || !(flags & DC_EXEC) || (flags & DC_QUERY_COST)) {
		return res;
	}

	/* NetworkSend_Command needs _local_company to be set correctly, so
	 * adjust it, and put it back right after the function */
	old_local_company = _local_company;
	_local_company = _current_company;

#ifdef ENABLE_NETWORK
	/* Send the command */
	if (_networking) {
		/* Network is easy, send it to his handler */
		NetworkSend_Command(tile, p1, p2, cmd, callback, text);
	} else {
#else
	{
#endif
		/* If we execute BuildCommands directly in SP, we have a big problem with events
		 *  so we need to delay is for 1 tick */
		AI_PutCommandInQueue(_current_company, tile, p1, p2, cmd, callback, text);
	}

	/* Set _local_company back */
	_local_company = old_local_company;

	return res;
}


CommandCost AI_DoCommand(TileIndex tile, uint32 p1, uint32 p2, uint32 flags, uint32 cmd, const char *text)
{
	return AI_DoCommandCc(tile, p1, p2, flags, cmd, NULL, text);
}


/**
 * Run 1 tick of the AI. Don't overdo it, keep it realistic.
 */
static void AI_RunTick(CompanyID company)
{
	extern void AiNewDoGameLoop(Company *c);

	Company *c = GetCompany(company);
	_current_company = company;

	if (_settings_game.ai.ainew_active) {
		AiNewDoGameLoop(c);
	} else {
		/* Enable all kind of cheats the old AI needs in order to operate correctly... */
		_is_old_ai_company = true;
		AiDoGameLoop(c);
		_is_old_ai_company = false;
	}

	/* AI could change some track, so update signals */
	UpdateSignalsInBuffer();
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
	if (_networking && (!_network_server || !_settings_game.ai.ai_in_multiplayer)) return;

	/* New tick */
	_ai.tick++;

	/* Make sure the AI follows the difficulty rule.. */
	assert(_settings_game.difficulty.competitor_speed <= 4);
	if ((_ai.tick & ((1 << (4 - _settings_game.difficulty.competitor_speed)) - 1)) != 0) return;

	/* Check for AI-client (so joining a network with an AI) */
	if (!_networking || _network_server) {
		/* Check if we want to run AIs (server or SP only) */
		const Company *c;

		FOR_ALL_COMPANIES(c) {
			if (c->is_ai) {
				/* This should always be true, else something went wrong... */
				assert(_ai_company[c->index].active);

				/* Run the script */
				AI_DequeueCommands(c->index);
				AI_RunTick(c->index);
			}
		}
	}

	_current_company = OWNER_NONE;
}

/**
 * A new AI sees the day of light. You can do here what ever you think is needed.
 */
void AI_StartNewAI(CompanyID company)
{
	assert(IsValidCompanyID(company));

	/* Called if a new AI is booted */
	_ai_company[company].active = true;
}

/**
 * This AI company died. Give it some chance to make a final puf.
 */
void AI_CompanyDied(CompanyID company)
{
	/* Called if this AI died */
	_ai_company[company].active = false;

	if (_companies_ainew[company].pathfinder == NULL) return;

	AyStarMain_Free(_companies_ainew[company].pathfinder);
	delete _companies_ainew[company].pathfinder;
	_companies_ainew[company].pathfinder = NULL;

}

/**
 * Initialize some AI-related stuff.
 */
void AI_Initialize()
{
	/* First, make sure all AIs are DEAD! */
	AI_Uninitialize();

	memset(&_ai, 0, sizeof(_ai));
	memset(&_ai_company, 0, sizeof(_ai_company));

	_ai.enabled = true;
}

/**
 * Deinitializer for AI-related stuff.
 */
void AI_Uninitialize()
{
	for (CompanyID c = COMPANY_FIRST; c < MAX_COMPANIES; c++) AI_CompanyDied(c);
}
