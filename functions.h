#ifndef FUNCTIONS_H
#define FUNCTIONS_H

/* vehicle.c */

/* window.c */


/* landscape.c */
void FindLandscapeHeight(TileInfo *ti, uint x, uint y);
void FindLandscapeHeightByTile(TileInfo *ti, TileIndex tile);

void DoClearSquare(TileIndex tile);
void CDECL ModifyTile(TileIndex tile, uint flags, ...);
void RunTileLoop(void);

uint GetPartialZ(int x, int y, int corners);
uint GetSlopeZ(int x, int y);
uint32 GetTileTrackStatus(TileIndex tile, TransportType mode);
void GetAcceptedCargo(TileIndex tile, AcceptedCargo ac);
void ChangeTileOwner(TileIndex tile, byte old_player, byte new_player);
void AnimateTile(TileIndex tile);
void ClickTile(TileIndex tile);
void GetTileDesc(TileIndex tile, TileDesc *td);
void DrawTile(TileInfo *ti);
void UpdateTownMaxPass(Town *t);

bool IsValidTile(TileIndex tile);

static inline Point RemapCoords(int x, int y, int z)
{
#if !defined(NEW_ROTATION)
	Point pt;
	pt.x = (y - x) * 2;
	pt.y = y + x - z;
#else
	Point pt;
	pt.x = (x + y) * 2;
	pt.y = x - y - z;
#endif
	return pt;
}

static inline Point RemapCoords2(int x, int y)
{
	return RemapCoords(x, y, GetSlopeZ(x, y));
}


/* clear_land.c */
void DrawHillyLandTile(TileInfo *ti);
void DrawClearLandTile(TileInfo *ti, byte set);
void DrawClearLandFence(TileInfo *ti, byte img);
void TileLoopClearHelper(TileIndex tile);

/* station_land.c */
void StationPickerDrawSprite(int x, int y, int railtype, int image);

/* track_land.c */
void DrawTrainDepotSprite(int x, int y, int image, int railtype);

/* road_land.c */
void DrawRoadDepotSprite(int x, int y, int image);

/* water_land.c */
void DrawShipDepotSprite(int x, int y, int image);
void TileLoop_Water(TileIndex tile);

/* players.c */
bool CheckPlayerHasMoney(int32 cost);
void SubtractMoneyFromPlayer(int32 cost);
void SubtractMoneyFromPlayerFract(byte player, int32 cost);
bool CheckOwnership(byte owner);
bool CheckTileOwnership(TileIndex tile);
StringID GetPlayerNameString(byte player, byte index);

/* standard */
void ShowInfo(const char *str);
void CDECL ShowInfoF(const char *str, ...);
void NORETURN CDECL error(const char *str, ...);

/* openttd.c */

// **************
// * Warning: DO NOT enable this unless you understand what it does
// *
// * If enabled, in a network game all randoms will be dumped to the
// *  stdout if the first client joins (or if you are a client). This
// *  is to help finding desync problems.
// *
// * Warning: DO NOT enable this unless you understand what it does
// **************

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


// Used for profiling
#define TIC() { extern uint32 rdtsc(void); uint32 _xxx_ = rdtsc(); static float __avg__;
#define TOC(s) 	_xxx_ = rdtsc() - _xxx_; __avg__=__avg__*0.99+_xxx_*0.01; printf("%s: %8d %f\n", s, _xxx_,__avg__); }


void SetDate(uint date);
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
void TextMessageDailyLoop(void);

bool AddAnimatedTile(TileIndex tile);
void DeleteAnimatedTile(TileIndex tile);
void AnimateAnimatedTiles(void);
void InitializeAnimatedTiles(void);

/* tunnelbridge_cmd.c */
bool CheckTunnelInWay(TileIndex tile, int z);
bool CheckBridge_Stuff(byte bridge_type, int bridge_len);
uint32 GetBridgeLength(TileIndex begin, TileIndex end);
int CalcBridgeLenCostFactor(int x);

typedef void CommandCallback(bool success, TileIndex tile, uint32 p1, uint32 p2);
bool DoCommandP(TileIndex tile, uint32 p1, uint32 p2, CommandCallback *callback, uint32 cmd);

/* network.c */
void NetworkUDPClose(void);
void NetworkStartUp(void);
void NetworkShutDown(void);
void NetworkGameLoop(void);
void NetworkUDPGameLoop(void);
bool NetworkServerStart(void);
bool NetworkClientConnectGame(const char* host, unsigned short port);
void NetworkReboot(void);
void NetworkDisconnect(void);
void NetworkSend_Command(TileIndex tile, uint32 p1, uint32 p2, uint32 cmd, CommandCallback *callback);

/* misc_cmd.c */
void PlaceTreesRandomly(void);

void InitializeLandscapeVariables(bool only_constants);

/* misc.c */
void DeleteName(StringID id);
char *GetName(int id, char *buff);

// AllocateNameUnique also tests if the name used is not used anywere else
//  and if it is used, it returns an error.
#define AllocateNameUnique(name, skip) RealAllocateName(name, skip, true)
#define AllocateName(name, skip) RealAllocateName(name, skip, false)
StringID RealAllocateName(const char *name, byte skip, bool check_double);
void ConvertDayToYMD(YearMonthDay *ymd, uint16 date);
uint ConvertYMDToDay(uint year, uint month, uint day);
uint ConvertIntDate(uint date);

/* misc functions */
void MarkTileDirty(int x, int y);
void MarkTileDirtyByTile(TileIndex tile);
void InvalidateWindow(byte cls, WindowNumber number);
void InvalidateWindowWidget(byte cls, WindowNumber number, byte widget_index);
void InvalidateWindowClasses(byte cls);
void DeleteWindowById(WindowClass cls, WindowNumber number);
void DeleteWindowByClass(WindowClass cls);

void SetObjectToPlaceWnd(CursorID icon, byte mode, Window *w);
void SetObjectToPlace(CursorID icon, byte mode, WindowClass window_class, WindowNumber window_num);

void ResetObjectToPlace(void);

bool ScrollWindowToTile(TileIndex tile, Window * w);
bool ScrollWindowTo(int x, int y, Window * w);

bool ScrollMainWindowToTile(TileIndex tile);
bool ScrollMainWindowTo(int x, int y);
void DrawSprite(uint32 img, int x, int y);
uint GetCorrectTileHeight(TileIndex tile);
bool EnsureNoVehicle(TileIndex tile);
bool EnsureNoVehicleZ(TileIndex tile, byte z);
void MarkAllViewportsDirty(int left, int top, int right, int bottom);
void ShowCostOrIncomeAnimation(int x, int y, int z, int32 cost);
void ShowFeederIncomeAnimation(int x, int y, int z, int32 cost);

void DrawFoundation(TileInfo *ti, uint f);

bool CheckIfAuthorityAllows(TileIndex tile);
Town *ClosestTownFromTile(TileIndex tile, uint threshold);
void ChangeTownRating(Town *t, int add, int max);

uint GetRoadBitsByTile(TileIndex tile);
int GetTownRadiusGroup(Town *t, TileIndex tile);
void ShowNetworkChatQueryWindow(byte desttype, byte dest);
void ShowNetworkGiveMoneyWindow(byte player);
void ShowNetworkNeedGamePassword(void);
void ShowNetworkNeedCompanyPassword(void);
int FindFirstBit(uint32 x);
void ShowHighscoreTable(int difficulty, int8 rank);
void ShowEndGameChart(void);
TileIndex AdjustTileCoordRandomly(TileIndex a, byte rng);

void AfterLoadTown(void);
void GenRandomNewGame(uint32 rnd1, uint32 rnd2);
void StartScenarioEditor(uint32 rnd1, uint32 rnd2);
void AskExitGame(void);
void AskExitToGameMenu(void);

void RedrawAutosave(void);

StringID RemapOldStringID(StringID s);

void UpdateViewportSignPos(ViewportSign *sign, int left, int top, StringID str);

enum {
	SLD_LOAD_GAME = 0,
	SLD_LOAD_SCENARIO = 1,
	SLD_SAVE_GAME = 2,
	SLD_SAVE_SCENARIO = 3,
	SLD_NEW_GAME = 4,
};
void ShowSaveLoadDialog(int mode);

// callback from drivers that is called if the game size changes dynamically
void GameSizeChanged(void);
bool FileExists(const char *filename);
bool ReadLanguagePack(int index);
void InitializeLanguagePacks(void);
void *ReadFileToMem(const char *filename, size_t *lenp, size_t maxsize);
int GetLanguageList(char **languages, int max);

void CheckSwitchToEuro(void);

void LoadFromConfig(void);
void SaveToConfig(void);
void CheckConfig(void);
int ttd_main(int argc, char* argv[]);
byte GetOSVersion(void);

void DeterminePaths(void);
char * CDECL str_fmt(const char *str, ...);

void bubblesort(void *base, size_t nmemb, size_t size, int(*compar)(const void *, const void *));
bool CreateOTTDThread(void *func, void *param);
void CloseOTTDThread(void);
void JoinOTTDThread(void);
#endif /* FUNCTIONS_H */
