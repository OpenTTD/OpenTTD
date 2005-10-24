#ifndef AI_H
#define AI_H

#include "../functions.h"

/* How DoCommands look like for an AI */
typedef struct AICommand {
	uint32 tile;
	uint32 p1;
	uint32 p2;
	uint32 procc;

	uint32 dp[20];

	struct AICommand *next;
} AICommand;

/* The struct for an AIScript Player */
typedef struct AIPlayer {
	bool active;            //! Is this AI active?
	AICommand *queue;       //! The commands that he has in his queue
	AICommand *queue_tail;  //! The tail of this queue
} AIPlayer;

/* The struct to keep some data about the AI in general */
typedef struct AIStruct {
	/* General */
	bool enabled;           //! Is AI enabled?
	uint tick;              //! The current tick (something like _frame_counter, only for AIs)

	/* For network-clients (a OpenTTD client who acts as an AI connected to a server) */
	bool network_client;    //! Are we a network_client?
	uint8 network_player;   //! The current network player we are connected as
} AIStruct;

VARDEF AIStruct _ai;
VARDEF AIPlayer _ai_player[MAX_PLAYERS];

// ai.c
void AI_StartNewAI(PlayerID player);
void AI_PlayerDied(PlayerID player);
void AI_RunGameLoop(void);
void AI_Initialize(void);
void AI_Uninitialize(void);
int32 AI_DoCommand(uint tile, uint32 p1, uint32 p2, uint32 flags, uint procc);

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
