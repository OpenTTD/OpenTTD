#include "stdafx.h"
#include "ttd.h"
#include "table/strings.h"
#include "gfx.h"
#include "sound.h"
#include "window.h"
#include <windows.h>
#include <mmsystem.h>
#include "hal.h"
#include <winnt.h>
#include <wininet.h>
#include <io.h>
#include <fcntl.h>
#include "network.h"

#define SMART_PALETTE_ANIM

static struct {
	HWND main_wnd;
	HBITMAP dib_sect;
	void *bitmap_bits;
	void *buffer_bits;
	void *alloced_bits;
	HPALETTE gdi_palette;
	int width,height;
	int width_org, height_org;
	bool cursor_visible;
	bool switch_driver;
	bool fullscreen;
	bool double_size;
	bool has_focus;
	bool running;
} _wnd;

static HINSTANCE _inst;
static bool _has_console;

#if defined(MINGW32) || defined(__CYGWIN__)
	#define __TIMESTAMP__   __DATE__ __TIME__
#endif

#ifdef WIN32_ENABLE_DIRECTMUSIC_SUPPORT
extern const HalMusicDriver _dmusic_midi_driver;
#endif

static void MakePalette()
{
	LOGPALETTE *pal;
	int i;
	byte *b;

	pal = alloca(sizeof(LOGPALETTE) + (256-1) * sizeof(PALETTEENTRY));

	pal->palVersion = 0x300;
	pal->palNumEntries = 256;

	for(i=0,b=_cur_palette; i!=256;i++,b+=3) {
		pal->palPalEntry[i].peRed = b[0];
		pal->palPalEntry[i].peGreen = b[1];
		pal->palPalEntry[i].peBlue = b[2];
		pal->palPalEntry[i].peFlags = 0;

	}
	_wnd.gdi_palette = CreatePalette(pal);
	if (_wnd.gdi_palette == NULL)
		error("CreatePalette failed!\n");
}

static void UpdatePalette(HDC dc, uint start, uint count)
{
	RGBQUAD rgb[256];
	uint i;
	byte *b;

		for(i=0,b = _cur_palette + start*3; i!=count; i++,b+=3) {
		rgb[i].rgbRed = b[0];
		rgb[i].rgbGreen = b[1];
		rgb[i].rgbBlue = b[2];
		rgb[i].rgbReserved = 0;
	}

	SetDIBColorTable(dc, start, count, rgb);
}

static bool MyShowCursor(bool show)
{
	if (_wnd.cursor_visible == show)
		return show;

	_wnd.cursor_visible = show;
	ShowCursor(show);

	return !show;
}

typedef struct {
	byte vk_from;
	byte vk_count;
	byte map_to;
} VkMapping;

#define AS(x,z) {x,0,z}
#define AM(x,y,z,w) {x,y-x,z}

#ifndef	VK_OEM_3
#define VK_OEM_3 0xC0
#endif

static const VkMapping _vk_mapping[] = {
	// Pageup stuff + up/down
	AM(VK_PRIOR,VK_DOWN, WKC_PAGEUP, WKC_DOWN),
	// Map letters & digits
	AM('A','Z','A','Z'),
	AM('0','9','0','9'),

	AS(VK_ESCAPE,		WKC_ESC),
	AS(VK_PAUSE, WKC_PAUSE),
	AS(VK_BACK,			WKC_BACKSPACE),
	AM(VK_INSERT,VK_DELETE,WKC_INSERT, WKC_DELETE),

	AS(VK_SPACE,		WKC_SPACE),
	AS(VK_RETURN,		WKC_RETURN),
	AS(VK_TAB,			WKC_TAB),

	// Function keys
	AM(VK_F1, VK_F12,	WKC_F1, WKC_F12),

	// Numeric part.
	// What is the virtual keycode for numeric enter??
	AM(VK_NUMPAD0,VK_NUMPAD9, WKC_NUM_0, WKC_NUM_9),
	AS(VK_DIVIDE,			WKC_NUM_DIV),
	AS(VK_MULTIPLY,		WKC_NUM_MUL),
	AS(VK_SUBTRACT,		WKC_NUM_MINUS),
	AS(VK_ADD,				WKC_NUM_PLUS),
	AS(VK_DECIMAL,		WKC_NUM_DECIMAL),
	{0}
};

static uint MapWindowsKey(uint key)
{
	const VkMapping	*map = _vk_mapping - 1;
	uint from;
	do {
		map++;
		from = map->vk_from;
		if (from == 0) {
			return 0; // Unknown key pressed.
			}
	}	while ((uint)(key - from) > map->vk_count);

	key = key - from + map->map_to;

	if (GetAsyncKeyState(VK_SHIFT)<0)	key |= WKC_SHIFT;
	if (GetAsyncKeyState(VK_CONTROL)<0)	key |= WKC_CTRL;
	if (GetAsyncKeyState(VK_MENU)<0)	key |= WKC_ALT;
	return key;
}

static void MakeWindow(bool full_screen);
static bool AllocateDibSection(int w, int h);

static void ClientSizeChanged(int w, int h)
{
	if (_wnd.double_size) { w >>= 1; h >>= 1; }

	// allocate new dib section of the new size
	if (AllocateDibSection(w, h)) {
		// mark all palette colors dirty
		_pal_first_dirty = 0;
		_pal_last_dirty = 255;
		GameSizeChanged();

		// redraw screen
		if (_wnd.running) {
			_screen.dst_ptr = _wnd.buffer_bits;
			UpdateWindows();
		}
	}
}

void DoExitSave();

static LRESULT CALLBACK WndProcGdi(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg) {
	case WM_PAINT: {
		PAINTSTRUCT ps;
		HDC dc,dc2;
		HBITMAP old_bmp;
		HPALETTE old_palette;
		BeginPaint(hwnd, &ps);
		dc = ps.hdc;
		dc2 = CreateCompatibleDC(dc);
		old_bmp = SelectObject(dc2, _wnd.dib_sect);
		old_palette = SelectPalette(dc, _wnd.gdi_palette, FALSE);

		if (_pal_last_dirty != -1) {
			UpdatePalette(dc2, _pal_first_dirty, _pal_last_dirty - _pal_first_dirty + 1);
			_pal_last_dirty = -1;
		}

		BitBlt(dc, 0, 0, _wnd.width, _wnd.height, dc2, 0, 0, SRCCOPY);
		SelectPalette(dc, old_palette, TRUE);
		SelectObject(dc2, old_bmp);
		DeleteDC(dc2);
		EndPaint(hwnd, &ps);
		}
		return 0;

	case WM_PALETTECHANGED:
		if ((HWND)wParam == hwnd)
			return 0;
		// FALL THROUGH
	case WM_QUERYNEWPALETTE: {
		HDC hDC = GetWindowDC(hwnd);
		HPALETTE hOldPalette = SelectPalette(hDC, _wnd.gdi_palette, FALSE);
		UINT nChanged = RealizePalette(hDC);
		SelectPalette(hDC, hOldPalette, TRUE);
		ReleaseDC(hwnd, hDC);
		if (nChanged)
			InvalidateRect(hwnd, NULL, FALSE);
		return 0;
	}

	case WM_CLOSE:
		if (_game_mode == GM_MENU) { // do not ask to quit on the main screen
			_exit_game = true;
		} else if (_patches.autosave_on_exit) {
				DoExitSave();
				_exit_game = true;
		} else
			AskExitGame();

		return 0;

	case WM_LBUTTONDOWN:
		SetCapture(hwnd);
		_left_button_down = true;
		return 0;

	case WM_LBUTTONUP:
		ReleaseCapture();
		_left_button_down = false;
		_left_button_clicked = false;
		return 0;

	case WM_RBUTTONDOWN:
		SetCapture(hwnd);
		_right_button_down = true;
		_right_button_clicked = true;
		return 0;

	case WM_RBUTTONUP:
		ReleaseCapture();
		_right_button_down = false;
		return 0;

	case WM_MOUSEMOVE: {
		int x = (int16)LOWORD(lParam);
		int y = (int16)HIWORD(lParam);
		POINT pt;

		if (_wnd.double_size) {
			x >>= 1;
			y >>= 1;
		}

		if (_cursor.fix_at) {
			int dx = x - _cursor.pos.x;
			int dy = y - _cursor.pos.y;
			if (dx != 0 || dy != 0) {
				_cursor.delta.x += dx;
				_cursor.delta.y += dy;

				pt.x = _cursor.pos.x;
				pt.y = _cursor.pos.y;

				if (_wnd.double_size) {
					pt.x *= 2;
					pt.y *= 2;
				}
				ClientToScreen(hwnd, &pt);
				SetCursorPos(pt.x, pt.y);
			}
		} else {
			_cursor.delta.x += x - _cursor.pos.x;
			_cursor.delta.y += y - _cursor.pos.y;
			_cursor.pos.x = x;
			_cursor.pos.y = y;
			_cursor.dirty = true;
		}
		MyShowCursor(false);
		return 0;
	}

	case WM_CHAR: {
		uint16 scancode = (( lParam & 0xFF0000 ) >> 16 );
		if( scancode == 41 )
			_pressed_key = WKC_BACKQUOTE << 16;
	} break;
	
	
	case WM_KEYDOWN: {
		// this is the rewritten ascii input function
		// it disables windows deadkey handling --> more linux like :D
    unsigned short w = 0;
		int r = 0;
		byte ks[256];
		unsigned int scan = 0;
		GetKeyboardState(ks);
		r = ToAscii(wParam, scan, ks, &w, 0);
		if (r == 0) w = 0; // no translation was possible

		_pressed_key = w | MapWindowsKey(wParam) << 16;

		if ((_pressed_key>>16) == ('D' | WKC_CTRL) && !_wnd.fullscreen) {
			_double_size ^= 1;
			_wnd.double_size = _double_size;
			ClientSizeChanged(_wnd.width, _wnd.height);
			MarkWholeScreenDirty();
		}
	}	break;


	case WM_SYSKEYDOWN: /* user presses F10 or Alt, both activating the title-menu */
		switch(wParam) {
		case VK_RETURN: /* Full Screen */
			MakeWindow(!_wnd.fullscreen);
			return 0;
		case VK_MENU: /* Just ALT */
			return 0; // do nothing
		default: /* ALT in combination with something else */
			_pressed_key = MapWindowsKey(wParam) << 16;
			break;
		}
		break;
	case WM_NCMOUSEMOVE:
		MyShowCursor(true);
		return 0;

	case WM_SIZE: {
		if (wParam != SIZE_MINIMIZED) {
			ClientSizeChanged(LOWORD(lParam), HIWORD(lParam));
		}
		return 0;
	}
	case WM_SIZING: {
		RECT* r = (RECT*)lParam;
		RECT r2;
		int w, h;

		SetRect(&r2, 0, 0, 0, 0);
		AdjustWindowRect(&r2, GetWindowLong(hwnd, GWL_STYLE), FALSE);

		w = r->right - r->left - (r2.right - r2.left);
		h = r->bottom - r->top - (r2.bottom - r2.top);
		if (_wnd.double_size) { w >>= 1; h >>= 1; }
		w = clamp(w, 64, MAX_SCREEN_WIDTH);
		h = clamp(h, 64, MAX_SCREEN_HEIGHT);
		if (_wnd.double_size) { w <<= 1; h <<= 1; }
		SetRect(&r2, 0, 0, w, h);

		AdjustWindowRect(&r2, GetWindowLong(hwnd, GWL_STYLE), FALSE);
		w = r2.right - r2.left;
		h = r2.bottom - r2.top;

		switch (wParam) {
		case WMSZ_BOTTOM:
			r->bottom = r->top + h;
			break;
		case WMSZ_BOTTOMLEFT:
			r->bottom = r->top + h;
			r->left = r->right - w;
			break;
		case WMSZ_BOTTOMRIGHT:
			r->bottom = r->top + h;
			r->right = r->left + w;
			break;
		case WMSZ_LEFT:
			r->left = r->right - w;
			break;
		case WMSZ_RIGHT:
			r->right = r->left + w;
			break;
		case WMSZ_TOP:
			r->top = r->bottom - h;
			break;
		case WMSZ_TOPLEFT:
			r->top = r->bottom - h;
			r->left = r->right - w;
			break;
		case WMSZ_TOPRIGHT:
			r->top = r->bottom - h;
			r->right = r->left + w;
			break;
		}
		return TRUE;
	}

// needed for wheel
#if !defined(WM_MOUSEWHEEL)
# define WM_MOUSEWHEEL                   0x020A
#endif  //WM_MOUSEWHEEL
#if !defined(GET_WHEEL_DELTA_WPARAM)
# define GET_WHEEL_DELTA_WPARAM(wparam) ((short)HIWORD(wparam))
#endif  //GET_WHEEL_DELTA_WPARAM

	case WM_MOUSEWHEEL: {
		int delta = GET_WHEEL_DELTA_WPARAM(wParam);
		if (delta < 0) {
			_cursor.wheel++;
		} else if (delta > 0) {
			_cursor.wheel--;
		}
		return 0;
	}

	case WM_ACTIVATEAPP:
		_wnd.has_focus = (bool)wParam;
		break;
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

static void RegisterWndClass()
{
	static bool registered;
	if (!registered) {
		HINSTANCE hinst = GetModuleHandle(NULL);
		WNDCLASS wnd = {
			0,
			WndProcGdi,
			0,
			0,
			hinst,
			LoadIcon(hinst, MAKEINTRESOURCE(100)),
			LoadCursor(NULL, IDC_ARROW),
			0,
			0,
			"TTD"
		};
		registered = true;
		if (!RegisterClass(&wnd))
			error("RegisterClass failed");
	}
}

extern const char _openttd_revision[];

static void MakeWindow(bool full_screen)
{
	_fullscreen = full_screen;

	_wnd.double_size = _double_size && !full_screen;

	// recreate window?
	if ((full_screen|_wnd.fullscreen) && _wnd.main_wnd) {
		DestroyWindow(_wnd.main_wnd);
		_wnd.main_wnd = 0;
	}

	if (full_screen) {
		DEVMODE settings;
		memset(&settings, 0, sizeof(DEVMODE));
		settings.dmSize = sizeof(DEVMODE);
		settings.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;

		if (_fullscreen_bpp) {
			settings.dmBitsPerPel = _fullscreen_bpp;
			settings.dmFields |= DM_BITSPERPEL;
		}
		settings.dmPelsWidth = _wnd.width_org;
		settings.dmPelsHeight = _wnd.height_org;
		if ((settings.dmDisplayFrequency = _display_hz) != 0)
			settings.dmFields |= DM_DISPLAYFREQUENCY;
		if ( !ChangeDisplaySettings(&settings, CDS_FULLSCREEN) == DISP_CHANGE_SUCCESSFUL ) {
			MakeWindow(false);
			return;
		}
	} else if (_wnd.fullscreen) {
		// restore display?
		ChangeDisplaySettings(NULL, 0);
	}

	{
		RECT r;
		uint style;
		int x, y, w, h;

		if ((_wnd.fullscreen=full_screen) != false) {
			style = WS_POPUP | WS_VISIBLE;
			SetRect(&r, 0, 0, _wnd.width_org, _wnd.height_org);
		} else {
			style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
			SetRect(&r, 0, 0, _wnd.width, _wnd.height);
		}

		AdjustWindowRect(&r, style, FALSE);
		w = r.right - r.left;
		h = r.bottom - r.top;
		x = (GetSystemMetrics(SM_CXSCREEN)-w)>>1;
		y = (GetSystemMetrics(SM_CYSCREEN)-h)>>1;

		if (_wnd.main_wnd) {
			SetWindowPos(_wnd.main_wnd, 0, x, y, w, h, SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER);
		} else {
			char Windowtitle[50] = "OpenTTD ";
			// also show revision number/release in window title
			strncat(Windowtitle, _openttd_revision, sizeof(Windowtitle)-(strlen(Windowtitle) + 1));

			_wnd.main_wnd = CreateWindow("TTD", Windowtitle, style, x, y, w, h, 0, 0, _inst, 0);
			if (_wnd.main_wnd == NULL)
				error("CreateWindow failed");
		}
	}
	GameSizeChanged(); // invalidate all windows, force redraw
}

static bool AllocateDibSection(int w, int h)
{
	BITMAPINFO *bi;
	HDC dc;

	w = clamp(w, 64, MAX_SCREEN_WIDTH);
	h = clamp(h, 64, MAX_SCREEN_HEIGHT);

	if (w == _screen.width && h == _screen.height)
		return false;

	_screen.width = w;
	_screen.pitch = (w + 3) & ~0x3;
	_screen.height = h;

	if (_wnd.alloced_bits) {
		free(_wnd.alloced_bits);
		_wnd.alloced_bits = NULL;
	}

	bi = alloca(sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD)*256);
	memset(bi, 0, sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD)*256);
	bi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);

	if (_wnd.double_size) {
		w = (w + 3) & ~0x3;
		_wnd.alloced_bits = _wnd.buffer_bits = (byte*)malloc(w * h);
		w *= 2;
		h *= 2;
	}

	bi->bmiHeader.biWidth = _wnd.width = w;
	bi->bmiHeader.biHeight = -(_wnd.height = h);

	bi->bmiHeader.biPlanes = 1;
	bi->bmiHeader.biBitCount = 8;
	bi->bmiHeader.biCompression = BI_RGB;

	if (_wnd.dib_sect)
		DeleteObject(_wnd.dib_sect);

	dc = GetDC(0);
	_wnd.dib_sect = CreateDIBSection(dc, bi, DIB_RGB_COLORS, &_wnd.bitmap_bits, NULL, 0);
	if (_wnd.dib_sect == NULL)
		error("CreateDIBSection failed");
	ReleaseDC(0, dc);

	if (!_wnd.double_size)
		_wnd.buffer_bits = _wnd.bitmap_bits;

	return true;
}

static const uint16 default_resolutions[][2] = {
	{640,480},
	{800,600},
	{1024,768},
	{1152,864},
	{1280,960},
	{1280,1024},
	{1400,1050},
	{1600,1200},
};

static void FindResolutions()
{
	int i = 0, n = 0;
	DEVMODE dm;

	while (EnumDisplaySettings(NULL, i++, &dm)) {
		if (dm.dmBitsPerPel == 8 &&
				IS_INT_INSIDE(dm.dmPelsWidth, 640, MAX_SCREEN_WIDTH+1) &&
				IS_INT_INSIDE(dm.dmPelsHeight, 480, MAX_SCREEN_HEIGHT+1) &&
				(n == 0 || _resolutions[n-1][0] != dm.dmPelsWidth || _resolutions[n-1][1] != dm.dmPelsHeight)) {
			_resolutions[n][0] = dm.dmPelsWidth;
			_resolutions[n][1] = dm.dmPelsHeight;
			if (++n == sizeof(_resolutions) / (sizeof(uint16)*2)) break;
		}
	}

	if (n == 0) {
		memcpy(_resolutions, default_resolutions, sizeof(default_resolutions));
		n = 6;
	}

	_num_resolutions = n;
}


static const char *Win32GdiStart(char **parm)
{
	memset(&_wnd, 0, sizeof(_wnd));
	_wnd.cursor_visible = true;

	RegisterWndClass();

	MakePalette();

	FindResolutions();

	// fullscreen uses those
	_wnd.width_org = _cur_resolution[0];
	_wnd.height_org = _cur_resolution[1];

	AllocateDibSection(_cur_resolution[0], _cur_resolution[1]);
	MarkWholeScreenDirty();

	MakeWindow(_fullscreen);

	return NULL;
}

static void Win32GdiStop()
{
	if ( _wnd.fullscreen ) {
		ChangeDisplaySettings(NULL, 0);
	}
	MyShowCursor(true);
	DeleteObject(_wnd.gdi_palette);
	DeleteObject(_wnd.dib_sect);
	DestroyWindow(_wnd.main_wnd);
}

// simple upscaler by 2
static void filter(int left, int top, int width, int height)
{
	uint p = _screen.pitch;
	byte *s = (byte*)_wnd.buffer_bits + top * p + left;
	byte *d = (byte*)_wnd.bitmap_bits + top * p * 4 + left * 2;
	int i;

	while (height) {
		for(i=0; i!=width; i++) {
			d[i*2] = d[i*2+1] = d[i*2+p*2] = d[i*2+1+p*2] = s[i];
		}
		s += p;
		d += p * 4;
		height--;
	}
}

static void Win32GdiMakeDirty(int left, int top, int width, int height)
{
	RECT r = {left, top, left+width, top+height};
	if (_wnd.double_size) {
		filter(left, top, width, height);
		//filter(0, 0, 640, 480);
		r.left *= 2;r.top *= 2;r.right *= 2;r.bottom *= 2;
	}
	InvalidateRect(_wnd.main_wnd, &r, FALSE);
}

static void CheckPaletteAnim()
{
	if (_pal_last_dirty == -1)
		return;
	InvalidateRect(_wnd.main_wnd, NULL, FALSE);
}

static int Win32GdiMainLoop()
{
	MSG mesg;
	uint32 next_tick = GetTickCount() + 30, cur_ticks;

	_wnd.running = true;

	while(true) {
		while (PeekMessage(&mesg, NULL, 0, 0, PM_REMOVE)) {
			InteractiveRandom(); // randomness
			TranslateMessage(&mesg);
			DispatchMessage(&mesg);
		}
		if (_exit_game) return ML_QUIT;
		if (_wnd.switch_driver) return ML_SWITCHDRIVER;

#if defined(_DEBUG)
		if (_wnd.has_focus && GetAsyncKeyState(VK_SHIFT) < 0) {
#else
		if (_wnd.has_focus && GetAsyncKeyState(VK_TAB) < 0) {
#endif
			if (!_networking) _fast_forward |= 2;
		} else if (_fast_forward&2) {
			_fast_forward = 0;
		}


		cur_ticks=GetTickCount();
		if ((_fast_forward && !_pause) || cur_ticks > next_tick)
			next_tick = cur_ticks;

		if (cur_ticks == next_tick) {
			next_tick += 30;
			_ctrl_pressed = _wnd.has_focus && GetAsyncKeyState(VK_CONTROL)<0;
			_shift_pressed = _wnd.has_focus && GetAsyncKeyState(VK_SHIFT)<0;
			_dbg_screen_rect = _wnd.has_focus && GetAsyncKeyState(VK_CAPITAL)<0;

			// determine which directional keys are down
			_dirkeys =
				(GetAsyncKeyState(VK_LEFT) < 0 ? 1 : 0) +
				(GetAsyncKeyState(VK_UP) < 0 ? 2 : 0) +
				(GetAsyncKeyState(VK_RIGHT) < 0 ? 4 : 0) +
				(GetAsyncKeyState(VK_DOWN) < 0 ? 8 : 0);

			GameLoop();
			_cursor.delta.x = _cursor.delta.y = 0;

			if (_force_full_redraw)
				MarkWholeScreenDirty();

			GdiFlush();
			_screen.dst_ptr = _wnd.buffer_bits;
			UpdateWindows();
			CheckPaletteAnim();
		} else {
			Sleep(1);
			GdiFlush();
			_screen.dst_ptr = _wnd.buffer_bits;
			DrawTextMessage();
			DrawMouseCursor();
		}
	}
}

static bool Win32GdiChangeRes(int w, int h)
{
	_wnd.width = _wnd.width_org = w;
	_wnd.height = _wnd.height_org = h;

	MakeWindow(_fullscreen); // _wnd.fullscreen screws up ingame resolution switching

	return true;
}

const HalVideoDriver _win32_video_driver = {
	Win32GdiStart,
	Win32GdiStop,
	Win32GdiMakeDirty,
	Win32GdiMainLoop,
	Win32GdiChangeRes,
};


/**********************
 * WIN32 MIDI PLAYER
 **********************/

struct {
	bool stop_song;
	bool terminate;
	bool playing;
	int new_vol;
	HANDLE wait_obj;
	char start_song[260];
} _midi;

static void Win32MidiPlaySong(const char *filename)
{
	strcpy(_midi.start_song, filename);
	_midi.playing = true;
	_midi.stop_song = false;
	SetEvent(_midi.wait_obj);
}

static void Win32MidiStopSong()
{
	if (_midi.playing) {
		_midi.stop_song = true;
		_midi.start_song[0] = 0;
		SetEvent(_midi.wait_obj);
	}
}

static bool Win32MidiIsSongPlaying()
{
	return _midi.playing;
}

static void Win32MidiSetVolume(byte vol)
{
	_midi.new_vol = vol;
	SetEvent(_midi.wait_obj);
}

static long CDECL MidiSendCommand(const char *cmd, ...) {
	va_list va;
	char buf[512];
	va_start(va, cmd);
	vsprintf(buf, cmd, va);
	va_end(va);
	return mciSendStringA(buf, NULL, 0, 0);
}

static bool MidiIntPlaySong(const char *filename)
{
	MidiSendCommand("close all");
	if (MidiSendCommand("open %s type sequencer alias song", filename) != 0)
		return false;

	if (MidiSendCommand("play song from 0") != 0)
		return false;
	return true;
}

static void MidiIntStopSong()
{
	MidiSendCommand("close all");
}

static void MidiIntSetVolume(int vol)
{
	uint v = (vol * 65535 / 127);
	midiOutSetVolume((HMIDIOUT)-1, v + (v<<16));
}

static bool MidiIntIsSongPlaying()
{
	char buf[16];
	mciSendStringA("status song mode", buf, sizeof(buf), 0);
	return strcmp(buf, "playing") == 0 || strcmp(buf, "seeking") == 0;
}

static DWORD WINAPI MidiThread(LPVOID arg)
{
	char *s;
	int vol;

	_midi.wait_obj = CreateEvent(NULL, FALSE, FALSE, NULL);

	do {
		if ((vol=_midi.new_vol) != -1) {
			_midi.new_vol = -1;
			MidiIntSetVolume(vol);

		}
		if ((s=_midi.start_song)[0]) {
			_midi.playing = MidiIntPlaySong(s);
			s[0] = 0;

			// Delay somewhat in case we don't manage to play.
			if (!_midi.playing) {
				Sleep(5000);
			}
		}
		if (_midi.stop_song != false && _midi.playing) {
			_midi.stop_song = false;
			_midi.playing = false;
			MidiIntStopSong();
		}

		if (_midi.playing && !MidiIntIsSongPlaying())
			_midi.playing = false;

		WaitForMultipleObjects(1, &_midi.wait_obj, FALSE, 1000);
	} while (!_midi.terminate);

	DeleteObject(_midi.wait_obj);
	return 0;
}

static char *Win32MidiStart(char **parm)
{
	DWORD threadId;
	memset(&_midi, 0, sizeof(_midi));
	_midi.new_vol = -1;
	CreateThread(NULL, 8192, MidiThread, 0, 0, &threadId);
	return 0;
}

static void Win32MidiStop()
{
	_midi.terminate = true;
	SetEvent(_midi.wait_obj);
}

const HalMusicDriver _win32_music_driver = {
	Win32MidiStart,
	Win32MidiStop,
	Win32MidiPlaySong,
	Win32MidiStopSong,
	Win32MidiIsSongPlaying,
	Win32MidiSetVolume,
};

// WIN32 Sound code.

static HWAVEOUT _waveout;
static WAVEHDR _wave_hdr[2];
static int _bufsize;
static void PrepareHeader(WAVEHDR *hdr)
{
	hdr->dwBufferLength = _bufsize*4;
	hdr->dwFlags = 0;
	hdr->lpData = malloc(_bufsize*4);
	if (hdr->lpData == NULL || waveOutPrepareHeader(_waveout, hdr, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
		error("waveOutPrepareHeader failed");
}

static void FillHeaders()
{
	WAVEHDR *hdr;
	for(hdr=_wave_hdr; hdr != endof(_wave_hdr); hdr++) {
		if (!(hdr->dwFlags & WHDR_INQUEUE)) {
			MxMixSamples(_mixer, hdr->lpData, hdr->dwBufferLength >> 2);
			if (waveOutWrite(_waveout, hdr, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
				error("waveOutWrite failed");
		}
	}
}

static void CALLBACK waveOutProc(HWAVEOUT hwo, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2)
{
	switch(uMsg) {
	case WOM_DONE:
		if (_waveout)
			FillHeaders();
		break;
	}
}

static char *Win32SoundStart(char **parm)
{
	WAVEFORMATEX wfex;
	int hz;

	_bufsize = GetDriverParamInt(parm, "bufsize", 1024);
	hz = GetDriverParamInt(parm, "hz", 11025);
	wfex.wFormatTag = WAVE_FORMAT_PCM;
	wfex.nChannels = 2;
	wfex.nSamplesPerSec = hz;
	wfex.nAvgBytesPerSec = hz*2*2;
	wfex.nBlockAlign = 4;
	wfex.wBitsPerSample = 16;
	if (waveOutOpen(&_waveout, WAVE_MAPPER, &wfex, (DWORD)&waveOutProc, 0, CALLBACK_FUNCTION) != MMSYSERR_NOERROR)
		return "waveOutOpen failed";
	PrepareHeader(&_wave_hdr[0]);
	PrepareHeader(&_wave_hdr[1]);
	FillHeaders();
	return NULL;
}

static void Win32SoundStop()
{
	HWAVEOUT waveout = _waveout;
	_waveout = NULL;
	waveOutReset(waveout);
	waveOutUnprepareHeader(waveout, &_wave_hdr[0], sizeof(WAVEHDR));
	waveOutUnprepareHeader(waveout, &_wave_hdr[1], sizeof(WAVEHDR));
	waveOutClose(waveout);
}

const HalSoundDriver _win32_sound_driver = {
	Win32SoundStart,
	Win32SoundStop,
};

// Helper function needed by dynamically loading SDL
bool LoadLibraryList(void **proc, const char *dll)
{
	HMODULE lib;
	void *p;

	while (*dll) {
		lib = LoadLibrary(dll);
		if (lib == NULL)
			return false;
		while (true) {
			while(*dll++);
			if (!*dll)
				break;
			p = GetProcAddress(lib, dll);
			if (p == NULL)
				return false;
			*proc++ = p;
		}
		dll++;
	}
	return true;
}

static const char *_exception_string;
static void *_safe_esp;
static char *_crash_msg;
static bool _expanded;
static bool _did_emerg_save;
static int _ident;

void ShowOSErrorBox(const char *buf)
{
	MyShowCursor(true);
	MessageBoxA(GetActiveWindow(), buf, "Error!", MB_ICONSTOP);

// if exception tracker is enabled, we crash here to let the exception handler handle it.
#if defined(WIN32_EXCEPTION_TRACKER) && !defined(_DEBUG)
	if (*buf == '!') {
		_exception_string = buf;
		*(byte*)0 = 0;
	}
#endif

}

#ifdef _MSC_VER

typedef struct DebugFileInfo {
	uint32 size;
	uint32 crc32;
	SYSTEMTIME file_time;
} DebugFileInfo;



static uint32 *_crc_table;

static void MakeCRCTable(uint32 *table) {
	uint32 crc, poly = 0xEDB88320L;
	int	i, j;

	_crc_table = table;

	for (i=0; i!=256; i++) {
		crc = i;
		for (j=8; j!=0; j--) {
			if (crc & 1)
				crc = (crc >> 1) ^ poly;
			else
				crc>>=1;
		}
		table[i] = crc;
	}
}

static uint32 CalcCRC(byte *data, uint size, uint32 crc) {
	do {
		crc = ((crc>>8) & 0x00FFFFFF) ^ _crc_table[ (crc^(*data++)) & 0xFF ];
	} while (--size);
	return crc;
}

static void GetFileInfo(DebugFileInfo *dfi, const char *filename)
{
	memset(dfi, 0, sizeof(dfi));

	{
		HANDLE file;
		byte buffer[1024];
		DWORD numread;
		uint32 filesize = 0;
		FILETIME write_time;
		uint32 crc = (uint32)-1;

		file = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, 0);
		if (file != INVALID_HANDLE_VALUE) {
			while(true) {
				if (ReadFile(file, buffer, sizeof(buffer), &numread, NULL) == 0 || numread==0)
					break;
				filesize += numread;
				crc = CalcCRC(buffer, numread, crc);
			}
			dfi->size = filesize;
			dfi->crc32 = crc ^ (uint32)-1;

			if (GetFileTime(file, NULL, NULL, &write_time)) {
				FileTimeToSystemTime(&write_time, &dfi->file_time);
			}
			CloseHandle(file);
		}
	}
}


static char *PrintModuleInfo(char *output, HMODULE mod)
{
	char buffer[MAX_PATH];
	DebugFileInfo dfi;
	GetModuleFileName(mod, buffer, MAX_PATH);
	GetFileInfo(&dfi, buffer);
	output += sprintf(output, " %-20s handle: %.8X size: %d crc: %.8X date: %d-%.2d-%.2d %.2d:%.2d:%.2d\r\n",
		buffer,
		mod,
		dfi.size,
		dfi.crc32,
		dfi.file_time.wYear,
		dfi.file_time.wMonth,
		dfi.file_time.wDay,
		dfi.file_time.wHour,
		dfi.file_time.wMinute,
		dfi.file_time.wSecond
	);
	return output;
}

static char *PrintModuleList(char *output)
{
	BOOL (WINAPI *EnumProcessModules)(HANDLE,HMODULE*,DWORD,LPDWORD);
	HANDLE proc;
	HMODULE modules[100];
	DWORD needed;
	BOOL res;
	int count,i;

	if (LoadLibraryList((void*)&EnumProcessModules, "psapi.dll\0EnumProcessModules\0")) {
		proc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, GetCurrentProcessId());
		if (proc) {
			res = EnumProcessModules(proc, modules, sizeof(modules), &needed);
			CloseHandle(proc);
			if (res) {
				count = min(needed/sizeof(HMODULE*), sizeof(modules)/sizeof(HMODULE*));
				for(i=0; i!=count; i++)
					output = PrintModuleInfo(output, modules[i]);
				return output;
			}
		}
	}
	output = PrintModuleInfo(output, NULL);
	return output;
}

static const char _crash_desc[] =
	"A serious fault condition occured in the game. The game will shut down.\n"
	"Press \"Submit report\" to send crash information to the developers. "
	"This will greatly help debugging. The information contained in the report is "
	"displayed below.\n"
	"Press \"Emergency save\" to attempt saving the game.";

static const char _save_succeeded[] =
	"Emergency save succeeded.\nBe aware that critical parts of the internal game state "
	"may have become corrupted. The saved game is not guaranteed to work.";

bool EmergencySave();


typedef struct {
	HINTERNET (WINAPI *InternetOpenA)(LPCSTR,DWORD, LPCSTR, LPCSTR, DWORD);
	HINTERNET (WINAPI *InternetConnectA)(HINTERNET, LPCSTR, INTERNET_PORT, LPCSTR, LPCSTR, DWORD, DWORD, DWORD);
	HINTERNET (WINAPI *HttpOpenRequestA)(HINTERNET, LPCSTR, LPCSTR, LPCSTR, LPCSTR, LPCSTR *, DWORD, DWORD);
	BOOL (WINAPI *HttpSendRequestA)(HINTERNET, LPCSTR, DWORD, LPVOID, DWORD);
	BOOL (WINAPI *InternetCloseHandle)(HINTERNET);
	BOOL (WINAPI *HttpQueryInfo)(HINTERNET, DWORD, LPVOID, LPDWORD, LPDWORD);
} WinInetProcs;

#define M(x) x "\0"
static const char wininet_files[] =
	M("wininet.dll")
	M("InternetOpenA")
	M("InternetConnectA")
	M("HttpOpenRequestA")
	M("HttpSendRequestA")
	M("InternetCloseHandle")
	M("HttpQueryInfoA")
	M("");
#undef M

static WinInetProcs _wininet;


static char *SubmitCrashReport(HWND wnd, void *msg, size_t msglen, const char *arg)
{
	HINTERNET inet, conn, http;
	char *err = NULL;
	DWORD code, len;
	static char buf[100];
	char buff[100];

	if (_wininet.InternetOpen == NULL && !LoadLibraryList((void**)&_wininet, wininet_files)) return "can't load wininet.dll";

	inet = _wininet.InternetOpen("TTD", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0 );
	if (inet == NULL) { err = "internetopen failed"; goto error1; }

	conn = _wininet.InternetConnect(inet, "openttd.com", INTERNET_DEFAULT_HTTP_PORT, "", "", INTERNET_SERVICE_HTTP, 0, 0);
	if (conn == NULL) { err = "internetconnect failed"; goto error2; }

	sprintf(buff, "/crash.php?file=%s&ident=%d", arg, _ident);

	http = _wininet.HttpOpenRequest(conn, "POST", buff, NULL, NULL, NULL, INTERNET_FLAG_NO_CACHE_WRITE , 0);
	if (http == NULL) { err = "httpopenrequest failed"; goto error3; }

	if (!_wininet.HttpSendRequest(http, "Content-type: application/binary", -1, msg, msglen)) { err = "httpsendrequest failed"; goto error4; }

	len = sizeof(code);
	if (!_wininet.HttpQueryInfo(http, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &code, &len, 0)) { err = "httpqueryinfo failed"; goto error4; }

	if (code != 200) {
		int l = sprintf(buf, "Server said: %d ", code);
		len = sizeof(buf) - l;
		_wininet.HttpQueryInfo(http, HTTP_QUERY_STATUS_TEXT, buf + l, &len, 0);
		err = buf;
	}

error4:
	_wininet.InternetCloseHandle(http);
error3:
	_wininet.InternetCloseHandle(conn);
error2:
	_wininet.InternetCloseHandle(inet);
error1:
	return err;
}

static void SubmitFile(HWND wnd, const char *file)
{
	HANDLE h;
	unsigned long size;
	unsigned long read;
	void *mem;

	h = CreateFile(file, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (h == NULL) return;

	size = GetFileSize(h, NULL);
	if (size > 500000) goto error1;

	mem = malloc(size);
	if (mem == NULL) goto error1;

	if (!ReadFile(h, mem, size, &read, NULL) || read != size) goto error2;

	SubmitCrashReport(wnd, mem, size, file);

error2:
	free(mem);
error1:
	CloseHandle(h);
}

static const char * const _expand_texts[] = {"S&how report >>", "&Hide report <<" };

static void SetWndSize(HWND wnd, int mode)
{
	RECT r,r2;
	int offs;

	GetWindowRect(wnd, &r);

	SetDlgItemText(wnd, 15, _expand_texts[mode == 1]);

	if (mode >= 0) {
		GetWindowRect(GetDlgItem(wnd, 11), &r2);
		offs = r2.bottom - r2.top + 10;
		if (!mode) offs=-offs;
		SetWindowPos(wnd, HWND_TOPMOST, 0, 0, r.right - r.left, r.bottom - r.top + offs, SWP_NOMOVE | SWP_NOZORDER);
	} else {
		SetWindowPos(wnd, HWND_TOPMOST,
			(GetSystemMetrics(SM_CXSCREEN) - (r.right - r.left)) >> 1,
			(GetSystemMetrics(SM_CYSCREEN) - (r.bottom - r.top)) >> 1,
			0, 0, SWP_NOSIZE);
	}
}

static bool DoEmergencySave(HWND wnd)
{
	bool b = false;

	EnableWindow(GetDlgItem(wnd, 13), FALSE);
	_did_emerg_save = true;
	__try {
		b = EmergencySave();
	} __except (1) {}
	return b;
}

static BOOL CALLBACK CrashDialogFunc(HWND wnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
	switch(msg) {
	case WM_INITDIALOG:
		SetDlgItemText(wnd, 10, _crash_desc);
		SetDlgItemText(wnd, 11, _crash_msg);
		SendDlgItemMessage(wnd, 11, WM_SETFONT, (WPARAM)GetStockObject(ANSI_FIXED_FONT), FALSE);
		SetWndSize(wnd, -1);
		return TRUE;
	case WM_COMMAND:
		switch(wParam) {
		case 12: // Close
			ExitProcess(0);
		case 13: { // Emergency save
			if (DoEmergencySave(wnd))
				MessageBoxA(wnd, _save_succeeded, "Save successful", MB_ICONINFORMATION);
			else
				MessageBoxA(wnd, "Save failed", "Save failed", MB_ICONINFORMATION);
			break;
		}
		case 14: { // Submit crash report
			char *s;

			SetCursor(LoadCursor(NULL, IDC_WAIT));

			s = SubmitCrashReport(wnd, _crash_msg, strlen(_crash_msg), "");
			if (s) {
				MessageBoxA(wnd, s, "Error", MB_ICONSTOP);
				break;
			}

			// try to submit emergency savegame
			if (_did_emerg_save || DoEmergencySave(wnd)) {
				SubmitFile(wnd, "crash.sav");
			}
			// try to submit the autosaved game
			if (_opt.autosave) {
				char buf[40];
				sprintf(buf, "autosave%d.sav", (_autosave_ctr - 1) & 3);
				SubmitFile(wnd, buf);
			}
			EnableWindow(GetDlgItem(wnd, 14), FALSE);
			SetCursor(LoadCursor(NULL, IDC_ARROW));
			MessageBoxA(wnd, "Crash report submitted. Thank you.", "Crash Report", MB_ICONINFORMATION);
			break;
		}
		case 15: // Expand
			_expanded ^= 1;
			SetWndSize(wnd, _expanded);
			break;
		}
		return TRUE;
	case WM_CLOSE:
		ExitProcess(0);
	}

	return FALSE;
}

static void Handler2()
{
	ShowCursor(TRUE);
	ShowWindow(GetActiveWindow(), FALSE);
	DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(100), NULL, CrashDialogFunc);
}

static LONG WINAPI ExceptionHandler(EXCEPTION_POINTERS *ep)
{
	char *output;
	static bool had_exception;

	if (had_exception) { ExitProcess(0); }
	had_exception = true;

	_ident = GetTickCount(); // something pretty unique

	MakeCRCTable(alloca(256 * sizeof(uint32)));
	_crash_msg = output = LocalAlloc(LMEM_FIXED, 8192);

	{
		SYSTEMTIME time;
		GetLocalTime(&time);
		output += sprintf(output,
			"*** OpenTTD Crash Report ***\r\n"
			"Date: %d-%.2d-%.2d %.2d:%.2d:%.2d\r\n"
			"Build: %s built on " __TIMESTAMP__ "\r\n",
			time.wYear,
			time.wMonth,
			time.wDay,
			time.wHour,
			time.wMinute,
			time.wSecond,
			"???"
		);
	}

	if (_exception_string) output += sprintf(output, "Reason: %s\r\n", _exception_string);

	output += sprintf(output, "Exception %.8X at %.8X\r\n"
		"Registers:\r\n"
		" EAX: %.8X EBX: %.8X ECX: %.8X EDX: %.8X\r\n"
		" ESI: %.8X EDI: %.8X EBP: %.8X ESP: %.8X\r\n"
		" EIP: %.8X EFLAGS: %.8X\r\n"
		"\r\nBytes at CS:EIP:\r\n",
		ep->ExceptionRecord->ExceptionCode,
		ep->ExceptionRecord->ExceptionAddress,
		ep->ContextRecord->Eax,
		ep->ContextRecord->Ebx,
		ep->ContextRecord->Ecx,
		ep->ContextRecord->Edx,
		ep->ContextRecord->Esi,
		ep->ContextRecord->Edi,
		ep->ContextRecord->Ebp,
		ep->ContextRecord->Esp,
		ep->ContextRecord->Eip,
		ep->ContextRecord->EFlags
	);

	{
		byte *b = (byte*)ep->ContextRecord->Eip;
		int i;
		for(i=0; i!=24; i++) {
			if (IsBadReadPtr(b, 1)) {
				output += sprintf(output, " ??"); // OCR: WAS: , 0);
			} else {
				output += sprintf(output, " %.2X", *b);
			}
			b++;
		}
		output += sprintf(output,
			"\r\n"
			"\r\nStack trace: \r\n"
		);
	}

	{
		int i,j;
		uint32 *b = (uint32*)ep->ContextRecord->Esp;
		for(j=0; j!=24; j++) {
			for(i=0; i!=8; i++) {
				if (IsBadReadPtr(b,sizeof(uint32))) {
					output += sprintf(output, " ????????"); //OCR: WAS - , 0);
				} else {
					output += sprintf(output, " %.8X", *b);
				}
				b++;
			}
			output += sprintf(output, "\r\n");
		}
	}

	output += sprintf(output, "\r\nModule information:\r\n");
	output = PrintModuleList(output);

	{
		OSVERSIONINFO os;
		os.dwOSVersionInfoSize = sizeof(os);
		GetVersionEx(&os);
		output += sprintf(output, "\r\nSystem information:\r\n"
			" Windows version %d.%d %d %s\r\n", os.dwMajorVersion, os.dwMinorVersion, os.dwBuildNumber, os.szCSDVersion);
	}

	{
		HANDLE file = CreateFile("crash.log", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, 0);
		DWORD num_written;
		if (file != INVALID_HANDLE_VALUE) {
			WriteFile(file, _crash_msg, output - _crash_msg, &num_written, NULL);
			CloseHandle(file);
		}
	}

	if (_safe_esp) {
		ep->ContextRecord->Eip = (DWORD)Handler2;
		ep->ContextRecord->Esp = (DWORD)_safe_esp;
		return EXCEPTION_CONTINUE_EXECUTION;
	} else {
		return EXCEPTION_EXECUTE_HANDLER;
	}
}

static void Win32InitializeExceptions()
{
	_asm {
		mov _safe_esp,esp
	}

	SetUnhandledExceptionFilter(ExceptionHandler);
}
#endif

static char *_fios_path;
static char *_fios_save_path;
static char *_fios_scn_path;
static FiosItem *_fios_items;
static int _fios_count, _fios_alloc;

static FiosItem *FiosAlloc()
{
	if (_fios_count == _fios_alloc) {
		_fios_alloc += 256;
		_fios_items = realloc(_fios_items, _fios_alloc * sizeof(FiosItem));
	}
	return &_fios_items[_fios_count++];
}

static HANDLE MyFindFirstFile(char *path, char *file, WIN32_FIND_DATA *fd)
{
	char paths[MAX_PATH];

	sprintf(paths, "%s\\%s", path, file);
	return FindFirstFile(paths, fd);
}

int CDECL compare_FiosItems (const void *a, const void *b) {
	const FiosItem *da = (const FiosItem *) a;
	const FiosItem *db = (const FiosItem *) b;
	int r;

	if (_savegame_sort_order < 2) // sort by date
    r = da->mtime < db->mtime ? -1 : 1;
	else
		r = stricmp(da->title[0] ? da->title : da->name, db->title[0] ? db->title : db->name);

	if (_savegame_sort_order & 1) r = -r;
	return r;
}

// Get a list of savegames
FiosItem *FiosGetSavegameList(int *num, int mode)
{
	WIN32_FIND_DATA fd;
	HANDLE h;
	FiosItem *fios;
	int sort_start;
	char buf[MAX_PATH];

	if (_fios_save_path == NULL) {
		_fios_save_path = malloc(MAX_PATH);
		strcpy(_fios_save_path, _path.save_dir);
	}

	if (_game_mode == GM_EDITOR)
		_fios_path = _fios_scn_path;
	else
		_fios_path = _fios_save_path;

	// Parent directory, only if not of the type C:\.
	if (_fios_path[3] != 0) {
		fios = FiosAlloc();
		fios->type = FIOS_TYPE_PARENT;
		strcpy(fios->title, ".. (Parent directory)");
	}


	// Show subdirectories first
	h = MyFindFirstFile(_fios_path, "*.*", &fd);
	if (h != INVALID_HANDLE_VALUE) {
		do {
			if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY &&
					!(fd.cFileName[0]=='.' && (fd.cFileName[1]==0 || fd.cFileName[1]=='.' && fd.cFileName[2]==0))) {
				fios = FiosAlloc();
				fios->type = FIOS_TYPE_DIR;
				strcpy(fios->name, fd.cFileName);
				sprintf(fios->title, "\\%s (Directory)", fd.cFileName);
			}
		} while (FindNextFile(h, &fd));
		FindClose(h);
	}

	// this is where to start sorting
	sort_start = _fios_count;

	/*	Show savegame files
	 *	.SAV	OpenTTD saved game
	 *	.SS1	Transport Tycoon Deluxe preset game
	 *	.SV1	Transport Tycoon Deluxe (Patch) saved game
	 *	.SV2	Transport Tycoon Deluxe (Patch) saved 2-player game
	 */
	h = MyFindFirstFile(_fios_path, "*.*", &fd);
	if (h != INVALID_HANDLE_VALUE) {
		do {
			if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
				char *t = strrchr(fd.cFileName, '.');
				if (t && !stricmp(t, ".SAV")) { // OpenTTD
					fios = FiosAlloc();
					fios->mtime = *(uint64*)&fd.ftLastWriteTime;
					sprintf(buf, "%s\\%s", _fios_path, fd.cFileName);
					fios->type = FIOS_TYPE_FILE;
					fios->title[0] = 0;
					ttd_strlcpy(fios->name, fd.cFileName, strlen(fd.cFileName)-3);
				}	else if (mode == SLD_LOAD_GAME || mode == SLD_LOAD_SCENARIO) {
					int ext = 0; // start of savegame extensions in _old_extensions[]
					if (t && ((ext++, !stricmp(t, ".SS1")) || (ext++, !stricmp(t, ".SV1")) || (ext++, !stricmp(t, ".SV2"))) ) { // TTDLX(Patch)
						fios = FiosAlloc();
						fios->old_extension = ext-1;
						fios->mtime = *(uint64*)&fd.ftLastWriteTime;
						sprintf(buf, "%s\\%s", _fios_path, fd.cFileName);
						fios->type = FIOS_TYPE_OLDFILE;
						GetOldSaveGameName(fios->title, buf);
						ttd_strlcpy(fios->name, fd.cFileName, strlen(fd.cFileName)-3);
					}
				}
			}
		} while (FindNextFile(h, &fd));
		FindClose(h);
	}

	qsort(_fios_items + sort_start, _fios_count - sort_start, sizeof(FiosItem), compare_FiosItems);

	// Drives
	{
		char drives[256];
		char *s;
		GetLogicalDriveStrings(sizeof(drives), drives);
		s=drives;
		while (*s) {
			fios = FiosAlloc();
			fios->type = FIOS_TYPE_DRIVE;
			fios->title[0] = s[0];
			fios->title[1] = ':';
			fios->title[2] = 0;
			while (*s++) {}
		}
	}
	*num = _fios_count;
	return _fios_items;
}

// Get a list of scenarios
FiosItem *FiosGetScenarioList(int *num, int mode)
{
	FiosItem *fios;
	WIN32_FIND_DATA fd;
	HANDLE h;
	int sort_start;
	char buf[MAX_PATH];

	if (mode == SLD_NEW_GAME || _fios_scn_path == NULL) {
		if (_fios_scn_path == NULL)
			_fios_scn_path = malloc(MAX_PATH);
		strcpy(_fios_scn_path, _path.scenario_dir);
	}

	_fios_path = _fios_scn_path;

	// Parent directory, only if not of the type C:\.
	if (_fios_path[3] != 0 && mode != SLD_NEW_GAME) {
		fios = FiosAlloc();
		fios->type = FIOS_TYPE_PARENT;
		strcpy(fios->title, ".. (Parent directory)");
	}

	// Show subdirectories first
	h = MyFindFirstFile(_fios_scn_path, "*.*", &fd);
	if (h != INVALID_HANDLE_VALUE && mode != SLD_NEW_GAME) {
		do {
			if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY &&
					!(fd.cFileName[0]=='.' && (fd.cFileName[1]==0 || fd.cFileName[1]=='.' && fd.cFileName[2]==0))) {
				fios = FiosAlloc();
				fios->type = FIOS_TYPE_DIR;
				strcpy(fios->name, fd.cFileName);
				sprintf(fios->title, "\\%s (Directory)", fd.cFileName);
			}
		} while (FindNextFile(h, &fd));
		FindClose(h);
	}

	// this is where to start sorting
	sort_start = _fios_count;

	/*	Show scenario files
	 *	.SCN	OpenTTD style scenario file
	 *	.SV0	Transport Tycoon Deluxe (Patch) scenario
	 *	.SS0	Transport Tycoon Deluxe preset scenario
	 */
	h = MyFindFirstFile(_fios_scn_path, "*.*", &fd);
	if (h != INVALID_HANDLE_VALUE) {
		do {
			if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
				char *t = strrchr(fd.cFileName, '.');
				if (t && !stricmp(t, ".SCN")) { // OpenTTD
					fios = FiosAlloc();
					fios->mtime = *(uint64*)&fd.ftLastWriteTime;
					sprintf(buf, "%s\\%s", _fios_path, fd.cFileName);
					fios->type = FIOS_TYPE_SCENARIO;
					fios->title[0] = 0;
					ttd_strlcpy(fios->name, fd.cFileName, strlen(fd.cFileName)-3);
				}	else if (mode == SLD_LOAD_GAME || mode == SLD_LOAD_SCENARIO || mode == SLD_NEW_GAME) {
					int ext = 3; // start of scenario extensions in _old_extensions[]
					if (t && ((ext++, !stricmp(t, ".SV0")) || (ext++, !stricmp(t, ".SS0"))) ) { // TTDLX(Patch)
						fios = FiosAlloc();
						fios->old_extension = ext-1;
						fios->mtime = *(uint64*)&fd.ftLastWriteTime;
						sprintf(buf, "%s\\%s", _fios_path, fd.cFileName);
						fios->type = FIOS_TYPE_OLD_SCENARIO;
						GetOldScenarioGameName(fios->title, buf);
						ttd_strlcpy(fios->name, fd.cFileName, strlen(fd.cFileName)-3);
					}
				}
			}
		} while (FindNextFile(h, &fd));
		FindClose(h);
	}

	qsort(_fios_items + sort_start, _fios_count - sort_start, sizeof(FiosItem), compare_FiosItems);

	// Drives
	if (mode != SLD_NEW_GAME) {
		char drives[256];
		char *s;
		GetLogicalDriveStrings(sizeof(drives), drives);
		s=drives;
		while (*s) {
			fios = FiosAlloc();
			fios->type = FIOS_TYPE_DRIVE;
			fios->title[0] = s[0];
			fios->title[1] = ':';
			fios->title[2] = 0;
			while (*s++) {}
		}
	}

	*num = _fios_count;
	return _fios_items;
}

// Free the list of savegames
void FiosFreeSavegameList()
{
	free(_fios_items);
	_fios_items = NULL;
	_fios_alloc = _fios_count = 0;
}

// Browse to
char *FiosBrowseTo(const FiosItem *item)
{
	char *path = _fios_path;
	char *s;

	switch(item->type) {
	case FIOS_TYPE_DRIVE:
		sprintf(path, "%c:\\", item->title[0]);
		break;

	case FIOS_TYPE_PARENT:
		// Skip drive part
		path += 3;
		s = path;
		while (*path) {
			if (*path== '\\')
				s = path;
			path++;
		}
		*s = 0;
		break;

	case FIOS_TYPE_DIR:
		// Scan to end
		while (*++path);
		// Add backslash?
		if (path[-1] != '\\') *path++ = '\\';

		strcpy(path, item->name);
		break;

	case FIOS_TYPE_FILE:
		FiosMakeSavegameName(str_buffr, item->name);
		return str_buffr;

	case FIOS_TYPE_OLDFILE:
		sprintf(str_buffr, "%s\\%s.%s", _fios_path, item->name, _old_extensions[item->old_extension]);
		return str_buffr;

	case FIOS_TYPE_SCENARIO:
		sprintf(str_buffr, "%s\\%s.scn", path, item->name);
		return str_buffr;
	case FIOS_TYPE_OLD_SCENARIO:
		sprintf(str_buffr, "%s\\%s.%s", path, item->name, _old_extensions[item->old_extension]);
		return str_buffr;
	}

	return NULL;
}

// Get descriptive texts.
// Returns a path as well as a
//  string describing the path.
StringID FiosGetDescText(const char **path)
{
	char root[4];
	DWORD spc, bps, nfc, tnc;
	*path = _fios_path;

	root[0] = _fios_path[0];
	root[1] = ':';
	root[2] = '\\';
	root[3] = 0;
	if (GetDiskFreeSpace(root, &spc, &bps, &nfc, &tnc)) {
		uint32 tot = ((spc*bps)*(uint64)nfc) >> 20;
		SetDParam(0, tot);
		return STR_4005_BYTES_FREE;
	} else {
		return STR_4006_UNABLE_TO_READ_DRIVE;
	}
}

void FiosMakeSavegameName(char *buf, const char *name)
{
	if(_game_mode == GM_EDITOR)
		sprintf(buf, "%s\\%s.scn", _fios_path, name);
	else
		sprintf(buf, "%s\\%s.sav", _fios_path, name);
}

void FiosDelete(const char *name)
{
	char *path = str_buffr;
	FiosMakeSavegameName(path, name);
	DeleteFile(path);
}

#define Windows_2000		5
#define Windows_NT3_51	4

/* flags show the minimum required OS to use a given feature. Currently
	 only dwMajorVersion and dwMinorVersion (WindowsME) are used
														MajorVersion	MinorVersion
		Windows Server 2003					5							 2				dmusic
		Windows XP									5							 1				dmusic
		Windows 2000								5							 0				dmusic
		Windows NT 4.0							4							 0				win32
		Windows Me									4							90				dmusic
		Windows 98									4							10				win32
		Windows 95									4							 0				win32
		Windows NT 3.51							3							51				?????
*/

const DriverDesc _video_driver_descs[] = {
	{"null", "Null Video Driver",				&_null_video_driver,		0},
#if defined(WITH_SDL)
	{"sdl", "SDL Video Driver",					&_sdl_video_driver,			1},
#endif
	{"win32", "Win32 GDI Video Driver",	&_win32_video_driver,		Windows_NT3_51},
	{ "dedicated", "Dedicated Video Driver", &_dedicated_video_driver, 0},
	{NULL}
};

const DriverDesc _sound_driver_descs[] = {
	{"null", "Null Sound Driver",			&_null_sound_driver,	0},
#if defined(WITH_SDL)
	{"sdl", "SDL Sound Driver",				&_sdl_sound_driver,		1},
#endif
	{"win32", "Win32 WaveOut Driver",	&_win32_sound_driver,	Windows_NT3_51},
	{NULL}
};

const DriverDesc _music_driver_descs[] = {
	{"null", "Null Music Driver",		&_null_music_driver,				0},
#ifdef WIN32_ENABLE_DIRECTMUSIC_SUPPORT
	{"dmusic", "DirectMusic MIDI Driver",	&_dmusic_midi_driver,	Windows_2000},
#endif
	// Win32 MIDI driver has higher priority then DMusic, so this one is chosen
	{"win32", "Win32 MIDI Driver",	&_win32_music_driver,				Windows_NT3_51},
	{NULL}
};

byte GetOSVersion()
{
	OSVERSIONINFO osvi;

	ZeroMemory(&osvi, sizeof(OSVERSIONINFO));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

	if (GetVersionEx(&osvi)) {
		DEBUG(misc, 2) ("Windows Version is %d.%d", osvi.dwMajorVersion, osvi.dwMinorVersion);
		// WinME needs directmusic too (dmusic, Windows_2000 mode), all others default to OK
		if (osvi.dwMajorVersion == 4 && osvi.dwMinorVersion == 90) { return Windows_2000;} // WinME

		return osvi.dwMajorVersion;
	}

	// GetVersionEx failed, but we can safely assume at least Win95/WinNT3.51 is used
	DEBUG(misc, 0) ("Windows version retrieval failed, defaulting to level 4");
	return Windows_NT3_51;
}

bool FileExists(const char *filename)
{
	HANDLE hand = CreateFile(filename, 0, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (hand == INVALID_HANDLE_VALUE) return false;
	CloseHandle(hand);
	return true;
}

static int CDECL LanguageCompareFunc(const void *a, const void *b)
{
	return strcmp(*(const char* const *)a, *(const char* const *)b);
}

int GetLanguageList(char **languages, int max)
{
	HANDLE hand;
	int num = 0;
	char filedir[MAX_PATH];
	WIN32_FIND_DATA fd;
	sprintf(filedir, "%s*.lng", _path.lang_dir);

	hand = FindFirstFile(filedir, &fd);
	if (hand != INVALID_HANDLE_VALUE) {
		do {
			if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
				languages[num++] = strdup(fd.cFileName);
				if (num == max) break;
			}
		} while (FindNextFile(hand, &fd));
		FindClose(hand);
	}

	qsort(languages, num, sizeof(char*), LanguageCompareFunc);
	return num;
}

static int ParseCommandLine(char *line, char **argv, int max_argc)
{
	int n = 0;

	do {
		// skip whitespace
		while (*line == ' ' || *line == '\t')
			line++;

		// end?
		if (*line == 0)
			break;

		// special handling when quoted
		if (*line == '"') {
			argv[n++] = ++line;
			while (*line != '"') {
				if (*line == 0) return n;
				line++;
			}
		} else {
			argv[n++] = line;
			while (*line != ' ' && *line != '\t') {
				if (*line == 0) return n;
				line++;
			}
		}
		*line++ = 0;
	} while (n != max_argc);

	return n;
}


#if defined(_MSC_VER)
__int64 _declspec(naked) rdtsc()
{
	_asm {
		rdtsc
		ret
	}
}
#endif

void CreateConsole()
{
	HANDLE hand;
	CONSOLE_SCREEN_BUFFER_INFO coninfo;

	if (_has_console) return;

	_has_console = true;

	AllocConsole();

	hand = GetStdHandle(STD_OUTPUT_HANDLE);
	GetConsoleScreenBufferInfo(hand, &coninfo);
	coninfo.dwSize.Y = 500;
	SetConsoleScreenBufferSize(hand, coninfo.dwSize);

	// redirect unbuffered STDIN, STDOUT, STDERR to the console
#if !defined(__CYGWIN__)
	*stdout = *_fdopen( _open_osfhandle((long)hand, _O_TEXT), "w" );
	*stdin = *_fdopen(_open_osfhandle((long)GetStdHandle(STD_INPUT_HANDLE), _O_TEXT), "r" );
	*stderr = *_fdopen(_open_osfhandle((long)GetStdHandle(STD_ERROR_HANDLE), _O_TEXT), "w" );
#else
	// open_osfhandle is not in cygwin
	*stdout = *fdopen(1, "w" );
	*stdin = *fdopen(0, "r" );
	*stderr = *fdopen(2, "w" );
#endif

	setvbuf( stdin, NULL, _IONBF, 0 );
	setvbuf( stdout, NULL, _IONBF, 0 );
	setvbuf( stderr, NULL, _IONBF, 0 );
}

void ShowInfo(const char *str)
{
	if (_has_console)
		puts(str);
	else {
		bool old;

		ReleaseCapture();
		_left_button_clicked =_left_button_down = false;

		old = MyShowCursor(true);
		if (MessageBoxA(GetActiveWindow(), str, "OpenTTD", MB_ICONINFORMATION | MB_OKCANCEL) == IDCANCEL) {
			CreateConsole();
		}
		MyShowCursor(old);
	}
}


int APIENTRY WinMain(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPTSTR lpCmdLine,int nCmdShow)
{
	int argc;
	char *argv[64]; // max 64 command line arguments
	_inst = hInstance;

#if defined(_DEBUG)
	CreateConsole();
#endif

	// make sure we have an autosave folder - Done in DeterminePaths
	// CreateDirectory("autosave", NULL);

	// setup random seed to something quite random
#if defined(_MSC_VER)
	{
		uint64 seed = rdtsc();
		_random_seeds[0][0] = ((uint32*)&seed)[0];
		_random_seeds[0][1] = ((uint32*)&seed)[1];
	}
#else
	_random_seeds[0][0] = GetTickCount();
	_random_seeds[0][1] = _random_seeds[0][0] * 0x1234567;
#endif

	argc = ParseCommandLine(GetCommandLine(), argv, lengthof(argv));

#if defined(WIN32_EXCEPTION_TRACKER)
	{
		Win32InitializeExceptions();
	}
#endif

#if defined(WIN32_EXCEPTION_TRACKER_DEBUG)
	_try {
		uint32 _stdcall ExceptionHandler(void *ep);
#endif
		ttd_main(argc, argv);

#if defined(WIN32_EXCEPTION_TRACKER_DEBUG)
	} _except (ExceptionHandler(_exception_info())) {}
#endif

	return 0;
}

void DeterminePaths()
{
	char *s;
	char *cfg;

	_path.personal_dir = _path.game_data_dir = cfg = malloc(MAX_PATH);
	GetCurrentDirectory(MAX_PATH - 1, cfg);


	s = strchr(cfg, 0);
	if (s[-1] != '\\') { s[0] = '\\'; s[1] = 0; }

	_path.save_dir = str_fmt("%ssave", cfg);
	_path.autosave_dir = str_fmt("%s\\autosave", _path.save_dir);
	_path.scenario_dir = str_fmt("%sscenario", cfg);
	_path.gm_dir = str_fmt("%sgm\\", cfg);
	_path.data_dir = str_fmt("%sdata\\", cfg);
	_path.lang_dir = str_fmt("%slang\\", cfg);

	_config_file = str_fmt("%sopenttd.cfg", _path.personal_dir);
	_log_file = str_fmt("%sopenttd.log", _path.personal_dir);

	// make (auto)save and scenario folder
	CreateDirectory(_path.save_dir, NULL);
	CreateDirectory(_path.autosave_dir, NULL);
	CreateDirectory(_path.scenario_dir, NULL);
}

int snprintf(char *str, size_t size, const char *format, ...)
{
	va_list ap;
	int ret;

	va_start(ap, format);
	ret = vsnprintf(str, size, format, ap);
	va_end(ap);
	return ret;
}

int vsnprintf(char *str, size_t size, const char *format, va_list ap)
{
	int ret;
	ret = _vsnprintf(str, size, format, ap);
	if (ret < 0) str[size - 1] = '\0';
	return ret;
}
