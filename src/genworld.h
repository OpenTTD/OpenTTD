/* $Id$ */

/** @file genworld.h */

#ifndef GENWORLD_H
#define GENWORLD_H

/* If OTTDThread isn't defined, define it to a void, but make sure to undefine
 *  it after this include. This makes including genworld.h easier, as you
 *  don't need to include thread.h before it, while it stays possible to
 *  include it after it, and still work.
 */
#ifndef OTTDThread
#define TEMPORARY_OTTDTHREAD_DEFINITION
#define OTTDThread void
#endif

/*
 * Order of these enums has to be the same as in lang/english.txt
 * Otherwise you will get inconsistent behaviour.
 */
enum {
	LG_ORIGINAL     = 0,  ///< The original landscape generator
	LG_TERRAGENESIS = 1,  ///< TerraGenesis Perlin landscape generator

	GENERATE_NEW_SEED = (uint)-1, ///< Create a new random seed
};

typedef void gw_done_proc();
typedef void gw_abort_proc();

struct gw_info {
	bool active;           ///< Is generating world active
	bool abort;            ///< Whether to abort the thread ASAP
	bool wait_for_draw;    ///< Are we waiting on a draw event
	bool quit_thread;      ///< Do we want to quit the active thread
	bool threaded;         ///< Whether we run _GenerateWorld threaded
	int mode;              ///< What mode are we making a world in
	PlayerID lp;           ///< The local_player before generating
	uint size_x;           ///< X-size of the map
	uint size_y;           ///< Y-size of the map
	gw_done_proc *proc;    ///< Proc that is called when done (can be NULL)
	gw_abort_proc *abortp; ///< Proc that is called when aborting (can be NULL)
	OTTDThread *thread;    ///< The thread we are in (can be NULL)
};

#ifdef TEMPORARY_OTTDTHREAD_DEFINITION
#undef OTTDThread
#undef TEMPORARY_OTTDTHREAD_DEFINITION
#endif

enum gwp_class {
	GWP_MAP_INIT,    ///< Initialize/allocate the map, start economy
	GWP_LANDSCAPE,   ///< Create the landscape
	GWP_ROUGH_ROCKY, ///< Make rough and rocky areas
	GWP_TOWN,        ///< Generate towns
	GWP_INDUSTRY,    ///< Generate industries
	GWP_UNMOVABLE,   ///< Generate unmovables (radio tower, light houses)
	GWP_TREE,        ///< Generate trees
	GWP_GAME_INIT,   ///< Initialize the game
	GWP_RUNTILELOOP, ///< Runs the tile loop 1280 times to make snow etc
	GWP_GAME_START,  ///< Really prepare to start the game
	GWP_CLASS_COUNT
};

/**
 * Check if we are currently in the process of generating a world.
 */
static inline bool IsGeneratingWorld()
{
	extern gw_info _gw;

	return _gw.active;
}

/* genworld.cpp */
void SetGeneratingWorldPaintStatus(bool status);
bool IsGeneratingWorldReadyForPaint();
bool IsGenerateWorldThreaded();
void GenerateWorldSetCallback(gw_done_proc *proc);
void GenerateWorldSetAbortCallback(gw_abort_proc *proc);
void WaitTillGeneratedWorld();
void GenerateWorld(int mode, uint size_x, uint size_y);
void AbortGeneratingWorld();
bool IsGeneratingWorldAborted();
void HandleGeneratingWorldAbortion();

/* genworld_gui.cpp */
void SetGeneratingWorldProgress(gwp_class cls, uint total);
void IncreaseGeneratingWorldProgress(gwp_class cls);
void PrepareGenerateWorldProgress();
void ShowGenerateWorldProgress();
void StartNewGameWithoutGUI(uint seed);
void ShowCreateScenario();
void StartScenarioEditor();

#endif /* GENWORLD_H */
