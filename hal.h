#ifndef HAL_H
#define HAL_H

typedef struct {
	const char *(*start)(const char * const *parm);
	void (*stop)(void);
} HalCommonDriver;

typedef struct {
	const char *(*start)(const char * const *parm);
	void (*stop)(void);
	void (*make_dirty)(int left, int top, int width, int height);
	int (*main_loop)(void);
	bool (*change_resolution)(int w, int h);
	void (*toggle_fullscreen)(bool fullscreen);
} HalVideoDriver;

enum {
	ML_QUIT = 0,
	ML_SWITCHDRIVER = 1,
};

typedef struct {
	const char *(*start)(const char * const *parm);
	void (*stop)(void);
} HalSoundDriver;

typedef struct {
	const char *(*start)(const char * const *parm);
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

VARDEF HalMusicDriver *_music_driver;
VARDEF HalSoundDriver *_sound_driver;
VARDEF HalVideoDriver *_video_driver;

extern const DriverDesc _video_driver_descs[];
extern const DriverDesc _sound_driver_descs[];
extern const DriverDesc _music_driver_descs[];

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


// Deals with finding savegames
typedef struct {
	uint16 id;
	byte type;
	uint64 mtime;
	char title[64];
	char name[256-12-64];
} FiosItem;

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
// Return path, free space and stringID
StringID FiosGetDescText(const char **path, uint32 *tot);
// Delete a name
void FiosDelete(const char *name);
// Make a filename from a name
void FiosMakeSavegameName(char *buf, const char *name);

int CDECL compare_FiosItems(const void *a, const void *b);

void CreateConsole(void);

#endif /* HAL_H */
