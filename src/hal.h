/* $Id$ */

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
	void (*main_loop)(void);
	bool (*change_resolution)(int w, int h);
	void (*toggle_fullscreen)(bool fullscreen);
} HalVideoDriver;

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

VARDEF HalMusicDriver *_music_driver;
VARDEF HalSoundDriver *_sound_driver;
VARDEF HalVideoDriver *_video_driver;

enum DriverType {
	VIDEO_DRIVER = 0,
	SOUND_DRIVER = 1,
	MUSIC_DRIVER = 2,
};

void GameLoop(void);

void CreateConsole(void);

#endif /* HAL_H */
