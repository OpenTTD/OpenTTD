/* $Id$ */

#ifndef HAL_H
#define HAL_H

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

enum WindowKeyCodes {
	WKC_SHIFT = 0x8000,
	WKC_CTRL  = 0x4000,
	WKC_ALT   = 0x2000,
	WKC_META  = 0x1000,

	// Special ones
	WKC_NONE        =  0,
	WKC_ESC         =  1,
	WKC_BACKSPACE   =  2,
	WKC_INSERT      =  3,
	WKC_DELETE      =  4,

	WKC_PAGEUP      =  5,
	WKC_PAGEDOWN    =  6,
	WKC_END         =  7,
	WKC_HOME        =  8,

	// Arrow keys
	WKC_LEFT        =  9,
	WKC_UP          = 10,
	WKC_RIGHT       = 11,
	WKC_DOWN        = 12,

	// Return & tab
	WKC_RETURN      = 13,
	WKC_TAB         = 14,

	// Numerical keyboard
	WKC_NUM_0       = 16,
	WKC_NUM_1       = 17,
	WKC_NUM_2       = 18,
	WKC_NUM_3       = 19,
	WKC_NUM_4       = 20,
	WKC_NUM_5       = 21,
	WKC_NUM_6       = 22,
	WKC_NUM_7       = 23,
	WKC_NUM_8       = 24,
	WKC_NUM_9       = 25,
	WKC_NUM_DIV     = 26,
	WKC_NUM_MUL     = 27,
	WKC_NUM_MINUS   = 28,
	WKC_NUM_PLUS    = 29,
	WKC_NUM_ENTER   = 30,
	WKC_NUM_DECIMAL = 31,

	// Space
	WKC_SPACE       = 32,

	// Function keys
	WKC_F1          = 33,
	WKC_F2          = 34,
	WKC_F3          = 35,
	WKC_F4          = 36,
	WKC_F5          = 37,
	WKC_F6          = 38,
	WKC_F7          = 39,
	WKC_F8          = 40,
	WKC_F9          = 41,
	WKC_F10         = 42,
	WKC_F11         = 43,
	WKC_F12         = 44,

	// backquote is the key left of "1"
	// we only store this key here, no matter what character is really mapped to it
	// on a particular keyboard. (US keyboard: ` and ~ ; German keyboard: ^ and °)
	WKC_BACKQUOTE   = 45,
	WKC_PAUSE       = 46,

	// 0-9 are mapped to 48-57
	// A-Z are mapped to 65-90
	// a-z are mapped to 97-122
};


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

extern HalMusicDriver *_music_driver;
extern HalSoundDriver *_sound_driver;
extern HalVideoDriver *_video_driver;

enum DriverType {
	VIDEO_DRIVER = 0,
	SOUND_DRIVER = 1,
	MUSIC_DRIVER = 2,
};

enum GameModes {
	GM_MENU,
	GM_NORMAL,
	GM_EDITOR
};

void GameLoop(void);

void CreateConsole(void);

typedef int32 CursorID;
typedef byte Pixel;

typedef struct Point {
	int x,y;
} Point;

typedef struct Rect {
	int left,top,right,bottom;
} Rect;


typedef struct CursorVars {
	Point pos, size, offs, delta; ///< position, size, offset from top-left, and movement
	Point draw_pos, draw_size;    ///< position and size bounding-box for drawing
	CursorID sprite; ///< current image of cursor

	int wheel;       ///< mouse wheel movement
	const CursorID *animate_list, *animate_cur; ///< in case of animated cursor, list of frames
	uint animate_timeout;                       ///< current frame in list of animated cursor

	bool visible;    ///< cursor is visible
	bool dirty;      ///< the rect occupied by the mouse is dirty (redraw)
	bool fix_at;     ///< mouse is moving, but cursor is not (used for scrolling)
	bool in_window;  ///< mouse inside this window, determines drawing logic
} CursorVars;

typedef struct DrawPixelInfo {
	Pixel *dst_ptr;
	int left, top, width, height;
	int pitch;
	uint16 zoom;
} DrawPixelInfo;


extern byte _dirkeys;        // 1 = left, 2 = up, 4 = right, 8 = down
extern bool _fullscreen;
extern CursorVars _cursor;
extern bool _ctrl_pressed;   // Is Ctrl pressed?
extern bool _shift_pressed;  // Is Shift pressed?
extern byte _fast_forward;

extern bool _left_button_down;
extern bool _left_button_clicked;
extern bool _right_button_down;
extern bool _right_button_clicked;

extern DrawPixelInfo _screen;
extern bool _exit_game;
extern bool _networking;         ///< are we in networking mode?
extern byte _game_mode;
extern byte _pause;


void HandleKeypress(uint32 key);
void HandleMouseEvents(void);
void CSleep(int milliseconds);
void UpdateWindows(void);

uint32 InteractiveRandom(void); /* Used for random sequences that are not the same on the other end of the multiplayer link */
uint InteractiveRandomRange(uint max);
void DrawTextMessage(void);
void DrawMouseCursor(void);
void ScreenSizeChanged(void);
void HandleExitGameRequest(void);
void GameSizeChanged(void);
void UndrawMouseCursor(void);

extern int _pal_first_dirty;
extern int _pal_last_dirty;
extern int _num_resolutions;
extern uint16 _resolutions[32][2];
extern uint16 _cur_resolution[2];

typedef struct Colour {
	byte r;
	byte g;
	byte b;
} Colour;
extern Colour _cur_palette[256];

#ifdef __cplusplus
} // extern "C"
#endif //__cplusplus

#endif /* HAL_H */
