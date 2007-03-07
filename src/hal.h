/* $Id$ */

/** @file hal.h Hardware Abstraction Layer declarations */

#ifndef HAL_H
#define HAL_H

struct HalCommonDriver {
	const char *(*start)(const char * const *parm);
	void (*stop)();
};

struct HalVideoDriver {
	const char *(*start)(const char * const *parm);
	void (*stop)();
	void (*make_dirty)(int left, int top, int width, int height);
	void (*main_loop)();
	bool (*change_resolution)(int w, int h);
	void (*toggle_fullscreen)(bool fullscreen);
};

struct HalSoundDriver {
	const char *(*start)(const char * const *parm);
	void (*stop)();
};

struct HalMusicDriver {
	const char *(*start)(const char * const *parm);
	void (*stop)();

	void (*play_song)(const char *filename);
	void (*stop_song)();
	bool (*is_song_playing)();
	void (*set_volume)(byte vol);
};

extern HalMusicDriver *_music_driver;
extern HalSoundDriver *_sound_driver;
extern HalVideoDriver *_video_driver;

enum DriverType {
	VIDEO_DRIVER = 0,
	SOUND_DRIVER = 1,
	MUSIC_DRIVER = 2,
};

#endif /* HAL_H */
