#ifndef HAL_H
#define HAL_H

typedef struct {
	char *(*start)(char **parm);
	void (*stop)(void);
} HalCommonDriver;

typedef struct {
	const char *(*start)(char **parm);
	void (*stop)(void);
	void (*make_dirty)(int left, int top, int width, int height);
	int (*main_loop)(void);
	bool (*change_resolution)(int w, int h);
} HalVideoDriver;

enum {
	ML_QUIT = 0,
	ML_SWITCHDRIVER = 1,
};

typedef struct {
	char *(*start)(char **parm);
	void (*stop)(void);
} HalSoundDriver;

typedef struct {
	char *(*start)(char **parm);
	void (*stop)(void);

	void (*play_song)(const char *filename);
	void (*stop_song)(void);
	bool (*is_song_playing)(void);
	void (*set_volume)(byte vol);
} HalMusicDriver;

typedef struct {
	const char *name;
	const char *longname;
	const void *drv;
	uint32 flags;
} DriverDesc;

enum {
	HALERR_OK = 0,
	HALERR_ERROR = 1,
};

extern const HalMusicDriver _null_music_driver;
extern const HalVideoDriver _null_video_driver;
extern const HalSoundDriver _null_sound_driver;

VARDEF HalMusicDriver *_music_driver;
VARDEF HalSoundDriver *_sound_driver;
VARDEF HalVideoDriver *_video_driver;

extern const DriverDesc _video_driver_descs[];
extern const DriverDesc _sound_driver_descs[];
extern const DriverDesc _music_driver_descs[];

#if defined(WITH_SDL)
extern const HalSoundDriver _sdl_sound_driver;
extern const HalVideoDriver _sdl_video_driver;
#endif

#if defined(UNIX)
extern const HalMusicDriver _extmidi_music_driver;
#endif

#if defined(__BEOS__)
extern const HalMusicDriver _bemidi_music_driver;
#endif

#if defined(__OS2__)
extern const HalMusicDriver _os2_music_driver;
#endif

extern const HalVideoDriver _dedicated_video_driver;

enum DriverType {
	VIDEO_DRIVER = 0,
	SOUND_DRIVER = 1,
	MUSIC_DRIVER = 2,
};

extern void GameLoop(void);
extern bool _dbg_screen_rect;

void LoadDriver(int driver, const char *name);

char *GetDriverParam(char **parm, const char *name);
bool GetDriverParamBool(char **parm, const char *name);
int GetDriverParamInt(char **parm, const char *name, int def);



// Deals with finding savegames
typedef struct {
	uint16 id;
	byte type;
	uint64 mtime;
	char title[64];
	char name[256-12-64];
	int old_extension;
} FiosItem;

// extensions of old savegames, scenarios
static const char* const _old_extensions[] = {
	// old savegame types
	"ss1", // Transport Tycoon Deluxe preset game
	"sv1", // Transport Tycoon Deluxe (Patch) saved game
	"sv2", // Transport Tycoon Deluxe (Patch) saved 2-player game
	// old scenario game type
	"sv0", // Transport Tycoon Deluxe (Patch) scenario
	"ss0", // Transport Tycoon Deluxe preset scenario
};

enum {
	FIOS_TYPE_DRIVE = 0,
	FIOS_TYPE_PARENT = 1,
	FIOS_TYPE_DIR = 2,
	FIOS_TYPE_FILE = 3,
	FIOS_TYPE_OLDFILE = 4,
	FIOS_TYPE_SCENARIO = 5,
	FIOS_TYPE_OLD_SCENARIO = 6,
};


// Variables to display file lists
FiosItem *_fios_list;
int _fios_num;
int _saveload_mode;

// get the name of an oldstyle savegame
void GetOldSaveGameName(char *title, const char *file);
// get the name of an oldstyle scenario
void GetOldScenarioGameName(char *title, const char *file);

// Get a list of savegames
FiosItem *FiosGetSavegameList(int *num, int mode);
// Get a list of scenarios
FiosItem *FiosGetScenarioList(int *num, int mode);
// Free the list of savegames
void FiosFreeSavegameList(void);
// Browse to. Returns a filename w/path if we reached a file.
char *FiosBrowseTo(const FiosItem *item);
// Get descriptive texts.
// Returns a path as well as a
//  string describing the path.
StringID FiosGetDescText(const char **path);
// Delete a name
void FiosDelete(const char *name);
// Make a filename from a name
void FiosMakeSavegameName(char *buf, const char *name);

void CreateConsole(void);

#endif /* HAL_H */
