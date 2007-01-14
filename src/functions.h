/* $Id$ */

#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include "gfx.h"

void DoClearSquare(TileIndex tile);
void RunTileLoop(void);

uint GetPartialZ(int x, int y, Slope corners);
uint GetSlopeZ(int x, int y);
uint32 GetTileTrackStatus(TileIndex tile, TransportType mode);
void GetAcceptedCargo(TileIndex tile, AcceptedCargo ac);
void ChangeTileOwner(TileIndex tile, PlayerID old_player, PlayerID new_player);
void AnimateTile(TileIndex tile);
void ClickTile(TileIndex tile);
void GetTileDesc(TileIndex tile, TileDesc *td);
void UpdateTownMaxPass(Town *t);

bool IsValidTile(TileIndex tile);

static inline Point RemapCoords(int x, int y, int z)
{
	Point pt;
	pt.x = (y - x) * 2;
	pt.y = y + x - z;
	return pt;
}

static inline Point RemapCoords2(int x, int y)
{
	return RemapCoords(x, y, GetSlopeZ(x, y));
}


/* clear_land.c */
void DrawHillyLandTile(const TileInfo *ti);
void DrawClearLandTile(const TileInfo *ti, byte set);
void DrawClearLandFence(const TileInfo *ti);
void TileLoopClearHelper(TileIndex tile);

/* water_land.c */
void DrawShipDepotSprite(int x, int y, int image);
void TileLoop_Water(TileIndex tile);

/* players.c */
bool CheckPlayerHasMoney(int32 cost);
void SubtractMoneyFromPlayer(int32 cost);
void SubtractMoneyFromPlayerFract(PlayerID player, int32 cost);
bool CheckOwnership(Owner owner);
bool CheckTileOwnership(TileIndex tile);
StringID GetPlayerNameString(PlayerID player, uint index);

/* standard */
void ShowInfo(const char *str);
void CDECL ShowInfoF(const char *str, ...);
void NORETURN CDECL error(const char *str, ...);

/* openttd.c */

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
uint32 RandomMT(void);


#ifdef MERSENNE_TWISTER
	static inline uint32 Random(void) { return RandomMT(); }
	uint RandomRange(uint max);
#else

#ifdef RANDOM_DEBUG
	#define Random() DoRandom(__LINE__, __FILE__)
	uint32 DoRandom(int line, const char *file);
	#define RandomRange(max) DoRandomRange(max, __LINE__, __FILE__)
	uint DoRandomRange(uint max, int line, const char *file);
#else
	uint32 Random(void);
	uint RandomRange(uint max);
#endif
#endif // MERSENNE_TWISTER

static inline TileIndex RandomTileSeed(uint32 r) { return TILE_MASK(r); }
static inline TileIndex RandomTile(void) { return TILE_MASK(Random()); }


uint32 InteractiveRandom(void); /* Used for random sequences that are not the same on the other end of the multiplayer link */
uint InteractiveRandomRange(uint max);

/* facedraw.c */
void DrawPlayerFace(uint32 face, int color, int x, int y);

/* texteff.c */
void MoveAllTextEffects(void);
void AddTextEffect(StringID msg, int x, int y, uint16 duration);
void InitTextEffects(void);
void DrawTextEffects(DrawPixelInfo *dpi);

void InitTextMessage(void);
void DrawTextMessage(void);
void CDECL AddTextMessage(uint16 color, uint8 duration, const char *message, ...);
void UndrawTextMessage(void);

bool AddAnimatedTile(TileIndex tile);
void DeleteAnimatedTile(TileIndex tile);
void AnimateAnimatedTiles(void);
void InitializeAnimatedTiles(void);

/* tunnelbridge_cmd.c */
bool CheckBridge_Stuff(byte bridge_type, uint bridge_len);
uint32 GetBridgeLength(TileIndex begin, TileIndex end);
int CalcBridgeLenCostFactor(int x);

/* misc_cmd.c */
void PlaceTreesRandomly(void);

void InitializeLandscapeVariables(bool only_constants);

/* misc.c */
bool IsCustomName(StringID id);
void DeleteName(StringID id);
char *GetName(char *buff, StringID id, const char* last);

// AllocateNameUnique also tests if the name used is not used anywere else
//  and if it is used, it returns an error.
#define AllocateNameUnique(name, skip) RealAllocateName(name, skip, true)
#define AllocateName(name, skip) RealAllocateName(name, skip, false)
StringID RealAllocateName(const char *name, byte skip, bool check_double);
void ConvertNameArray(void);

/* misc functions */
void MarkTileDirty(int x, int y);
void MarkTileDirtyByTile(TileIndex tile);
void InvalidateWindow(WindowClass cls, WindowNumber number);
void InvalidateWindowWidget(WindowClass cls, WindowNumber number, byte widget_index);
void InvalidateWindowClasses(WindowClass cls);
void InvalidateWindowClassesData(WindowClass cls);
void DeleteWindowById(WindowClass cls, WindowNumber number);
void DeleteWindowByClass(WindowClass cls);

void SetObjectToPlaceWnd(CursorID icon, SpriteID pal, byte mode, Window *w);
void SetObjectToPlace(CursorID icon, SpriteID pal, byte mode, WindowClass window_class, WindowNumber window_num);

void ResetObjectToPlace(void);

bool ScrollWindowTo(int x, int y, Window * w);

bool ScrollMainWindowToTile(TileIndex tile);
bool ScrollMainWindowTo(int x, int y);
void DrawSprite(SpriteID img, SpriteID pal, int x, int y);
bool EnsureNoVehicle(TileIndex tile);
bool EnsureNoVehicleOnGround(TileIndex tile);
void MarkAllViewportsDirty(int left, int top, int right, int bottom);
void ShowCostOrIncomeAnimation(int x, int y, int z, int32 cost);
void ShowFeederIncomeAnimation(int x, int y, int z, int32 cost);

void DrawFoundation(TileInfo *ti, uint f);

bool CheckIfAuthorityAllows(TileIndex tile);
Town *ClosestTownFromTile(TileIndex tile, uint threshold);
void ChangeTownRating(Town *t, int add, int max);

uint GetTownRadiusGroup(const Town* t, TileIndex tile);
int FindFirstBit(uint32 x);
void ShowHighscoreTable(int difficulty, int8 rank);
TileIndex AdjustTileCoordRandomly(TileIndex a, byte rng);

void AfterLoadTown(void);
void UpdatePatches(void);
void AskExitGame(void);
void AskExitToGameMenu(void);

void RedrawAutosave(void);

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

// callback from drivers that is called if the game size changes dynamically
void GameSizeChanged(void);
bool FileExists(const char *filename);
bool ReadLanguagePack(int index);
void InitializeLanguagePacks(void);
const char *GetCurrentLocale(const char *param);
void *ReadFileToMem(const char *filename, size_t *lenp, size_t maxsize);

void LoadFromConfig(void);
void SaveToConfig(void);
void CheckConfig(void);
int ttd_main(int argc, char* argv[]);
void HandleExitGameRequest(void);

void DeterminePaths(void);

#endif /* FUNCTIONS_H */
