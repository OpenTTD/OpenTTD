#ifndef AI_H
#define AI_H

#include "../functions.h"
#include "../network.h"
#include "../player.h"
#include "../command.h"

/* How DoCommands look like for an AI */
typedef struct AICommand {
	uint32 tile;
	uint32 p1;
	uint32 p2;
	uint32 procc;
	CommandCallback* callback;

	char *text;
	uint uid;

	struct AICommand *next;
} AICommand;

/* The struct for an AIScript Player */
typedef struct AIPlayer {
	bool active;            ///< Is this AI active?
	AICommand *queue;       ///< The commands that he has in his queue
	AICommand *queue_tail;  ///< The tail of this queue
} AIPlayer;

/* The struct to keep some data about the AI in general */
typedef struct AIStruct {
	/* General */
	bool enabled;           ///< Is AI enabled?
	uint tick;              ///< The current tick (something like _frame_counter, only for AIs)
} AIStruct;

VARDEF AIStruct _ai;
VARDEF AIPlayer _ai_player[MAX_PLAYERS];

// ai.c
void AI_StartNewAI(PlayerID player);
void AI_PlayerDied(PlayerID player);
void AI_RunGameLoop(void);
void AI_Initialize(void);
void AI_Uninitialize(void);
int32 AI_DoCommand(TileIndex tile, uint32 p1, uint32 p2, uint32 flags, uint procc);
int32 AI_DoCommandCc(TileIndex tile, uint32 p1, uint32 p2, uint32 flags, uint procc, CommandCallback* callback);

/** Is it allowed to start a new AI.
 * This function checks some boundries to see if we should launch a new AI.
 * @return True if we can start a new AI.
 */
static inline bool AI_AllowNewAI(void)
{
	/* If disabled, no AI */
	if (!_ai.enabled)
		return false;

	/* If in network, but no server, no AI */
	if (_networking && !_network_server)
		return false;

	/* If in network, and server, possible AI */
	if (_networking && _network_server) {
		/* Do we want AIs in multiplayer? */
		if (!_patches.ai_in_multiplayer)
			return false;

		/* Only the NewAI is allowed... sadly enough the old AI just doesn't support this
		 *  system, because all commands are delayed by at least 1 tick, which causes
		 *  a big problem, because it uses variables that are only set AFTER the command
		 *  is really executed... */
		if (!_patches.ainew_active)
			return false;
	}

	return true;
}

#define AI_CHANCE16(a,b)    ((uint16)     AI_Random()  <= (uint16)((65536 * a) / b))
#define AI_CHANCE16R(a,b,r) ((uint16)(r = AI_Random()) <= (uint16)((65536 * a) / b))

/**
 * The random-function that should be used by ALL AIs.
 */
static inline uint AI_RandomRange(uint max)
{
	/* We pick RandomRange if we are in SP (so when saved, we do the same over and over)
	 *   but we pick InteractiveRandomRange if we are a network_server or network-client.
	 */
	if (_networking)
		return InteractiveRandomRange(max);
	else
		return RandomRange(max);
}

/**
 * The random-function that should be used by ALL AIs.
 */
static inline uint32 AI_Random(void)
{
/* We pick RandomRange if we are in SP (so when saved, we do the same over and over)
	 *   but we pick InteractiveRandomRange if we are a network_server or network-client.
	 */
	if (_networking)
		return InteractiveRandom();
	else
		return Random();
}

#endif /* AI_H */
