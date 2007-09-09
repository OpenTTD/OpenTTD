/* $Id$ */

/** @file functions.h */

#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include "gfx.h"

void UpdateTownMaxPass(Town *t);

/* clear_land.cpp */
void DrawHillyLandTile(const TileInfo *ti);
void DrawClearLandTile(const TileInfo *ti, byte set);
void DrawClearLandFence(const TileInfo *ti);
void TileLoopClearHelper(TileIndex tile);

/* water_land.cpp */
void DrawShipDepotSprite(int x, int y, int image);
void TileLoop_Water(TileIndex tile);

/* players.cpp */
bool CheckPlayerHasMoney(CommandCost cost);
void SubtractMoneyFromPlayer(CommandCost cost);
void SubtractMoneyFromPlayerFract(PlayerID player, CommandCost cost);
bool CheckOwnership(Owner owner);
bool CheckTileOwnership(TileIndex tile);

/* standard */
void ShowInfo(const char *str);
void CDECL ShowInfoF(const char *str, ...);

/* openttd.cpp */

/**************
 * Warning: DO NOT enable this unless you understand what it does
 *
 * If enabled, in a network game all randoms will be dumped to the
 *  stdout if the first client joins (or if you are a client). This
 *  is to help finding desync problems.
 *
 * Warning: DO NOT enable this unless you understand what it does
 **************/

//#define RANDOM_DEBUG


// Enable this to produce higher quality random numbers.
// Doesn't work with network yet.
//#define MERSENNE_TWISTER

// Mersenne twister functions
void SeedMT(uint32 seed);
uint32 RandomMT();


#ifdef MERSENNE_TWISTER
	static inline uint32 Random() { return RandomMT(); }
	uint RandomRange(uint max);
#else

#ifdef RANDOM_DEBUG
	#define Random() DoRandom(__LINE__, __FILE__)
	uint32 DoRandom(int line, const char *file);
	#define RandomRange(max) DoRandomRange(max, __LINE__, __FILE__)
	uint DoRandomRange(uint max, int line, const char *file);
#else
	uint32 Random();
	uint RandomRange(uint max);
#endif
#endif // MERSENNE_TWISTER

static inline TileIndex RandomTileSeed(uint32 r) { return TILE_MASK(r); }
static inline TileIndex RandomTile() { return TILE_MASK(Random()); }


uint32 InteractiveRandom(); // Used for random sequences that are not the same on the other end of the multiplayer link
uint InteractiveRandomRange(uint max);

/* texteff.cpp */
bool AddAnimatedTile(TileIndex tile);
void DeleteAnimatedTile(TileIndex tile);
void AnimateAnimatedTiles();
void InitializeAnimatedTiles();

/* tunnelbridge_cmd.cpp */
bool CheckBridge_Stuff(byte bridge_type, uint bridge_len);
uint32 GetBridgeLength(TileIndex begin, TileIndex end);
int CalcBridgeLenCostFactor(int x);

/* misc_cmd.cpp */
void PlaceTreesRandomly();

void InitializeLandscapeVariables(bool only_constants);

/* misc.cpp */
bool IsCustomName(StringID id);
void DeleteName(StringID id);
char *GetName(char *buff, StringID id, const char *last);

#define AllocateName(name, skip) RealAllocateName(name, skip, false)
StringID RealAllocateName(const char *name, byte skip, bool check_double);
void ConvertNameArray();

/* misc functions */
/**
 * Mark a tile given by its coordinate dirty for repaint.
 *
 * @ingroup dirty
 */
void MarkTileDirty(int x, int y);

/**
 * Mark a tile given by its index dirty for repaint.
 *
 * @ingroup dirty
 */
void MarkTileDirtyByTile(TileIndex tile);
void InvalidateWindow(WindowClass cls, WindowNumber number);
void InvalidateWindowWidget(WindowClass cls, WindowNumber number, byte widget_index);
void InvalidateWindowClasses(WindowClass cls);
void InvalidateWindowClassesData(WindowClass cls);
void DeleteWindowById(WindowClass cls, WindowNumber number);
void DeleteWindowByClass(WindowClass cls);

void SetObjectToPlaceWnd(CursorID icon, SpriteID pal, byte mode, Window *w);
void SetObjectToPlace(CursorID icon, SpriteID pal, byte mode, WindowClass window_class, WindowNumber window_num);

void ResetObjectToPlace();

bool ScrollWindowTo(int x, int y, Window *w, bool instant = false);

bool ScrollMainWindowToTile(TileIndex tile, bool instant = false);
bool ScrollMainWindowTo(int x, int y, bool instant = false);
void DrawSprite(SpriteID img, SpriteID pal, int x, int y);
bool EnsureNoVehicle(TileIndex tile);
bool EnsureNoVehicleOnGround(TileIndex tile);

/**
 * Mark all viewports dirty for repaint.
 *
 * @ingroup dirty
 */
void MarkAllViewportsDirty(int left, int top, int right, int bottom);
void ShowCostOrIncomeAnimation(int x, int y, int z, Money cost);
void ShowFeederIncomeAnimation(int x, int y, int z, Money cost);

bool CheckIfAuthorityAllows(TileIndex tile);
Town *ClosestTownFromTile(TileIndex tile, uint threshold);
void ChangeTownRating(Town *t, int add, int max);

uint GetTownRadiusGroup(const Town* t, TileIndex tile);
void ShowHighscoreTable(int difficulty, int8 rank);

int FindFirstBit(uint32 x);

void AfterLoadTown();
void UpdatePatches();
void AskExitGame();
void AskExitToGameMenu();

void RedrawAutosave();

StringID RemapOldStringID(StringID s);

void UpdateViewportSignPos(ViewportSign *sign, int left, int top, StringID str);

enum {
	SLD_LOAD_GAME,
	SLD_LOAD_SCENARIO,
	SLD_SAVE_GAME,
	SLD_SAVE_SCENARIO,
	SLD_LOAD_HEIGHTMAP,
	SLD_NEW_GAME,
};
void ShowSaveLoadDialog(int mode);

/* callback from drivers that is called if the game size changes dynamically */
void GameSizeChanged();
bool FileExists(const char *filename);
const char *GetCurrentLocale(const char *param);
void *ReadFileToMem(const char *filename, size_t *lenp, size_t maxsize);

void LoadFromConfig();
void SaveToConfig();
void CheckConfig();
int ttd_main(int argc, char* argv[]);
void HandleExitGameRequest();

#endif /* FUNCTIONS_H */
