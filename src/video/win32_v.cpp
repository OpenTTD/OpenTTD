/* $Id$ */

#include "../stdafx.h"
#include "../openttd.h"
#include "../functions.h"
#include "../gfx.h"
#include "../macros.h"
#include "../network/network.h"
#include "../variables.h"
#include "../win32.h"
#include "../window.h"
#include "../blitter/factory.hpp"
#include "win32_v.h"
#include <windows.h>
#include <tchar.h>

static struct {
	HWND main_wnd;
	HBITMAP dib_sect;
	void *buffer_bits;
	HPALETTE gdi_palette;
	int width;
	int height;
	int width_org;
	int height_org;
	bool minimized;
	bool fullscreen;
	bool has_focus;
	bool running;
} _wnd;

bool _force_full_redraw;
bool _window_maximize;
uint _display_hz;
uint _fullscreen_bpp;
static uint16 _bck_resolution[2];
#if !defined(UNICODE)
uint _codepage;
#endif

static void MakePalette()
{
	LOGPALETTE *pal;
	uint i;

	pal = (LOGPALETTE*)alloca(sizeof(LOGPALETTE) + (256 - 1) * sizeof(PALETTEENTRY));

	pal->palVersion = 0x300;
	pal->palNumEntries = 256;

	for (i = 0; i != 256; i++) {
		pal->palPalEntry[i].peRed   = _cur_palette[i].r;
		pal->palPalEntry[i].peGreen = _cur_palette[i].g;
		pal->palPalEntry[i].peBlue  = _cur_palette[i].b;
		pal->palPalEntry[i].peFlags = 0;

	}
	_wnd.gdi_palette = CreatePalette(pal);
	if (_wnd.gdi_palette == NULL) error("CreatePalette failed!\n");
}

static void UpdatePalette(HDC dc, uint start, uint count)
{
	RGBQUAD rgb[256];
	uint i;

	for (i = 0; i != count; i++) {
		rgb[i].rgbRed   = _cur_palette[start + i].r;
		rgb[i].rgbGreen = _cur_palette[start + i].g;
		rgb[i].rgbBlue  = _cur_palette[start + i].b;
		rgb[i].rgbReserved = 0;
	}

	SetDIBColorTable(dc, start, count, rgb);
}

struct VkMapping {
	byte vk_from;
	byte vk_count;
	byte map_to;
};

#define AS(x, z) {x, 0, z}
#define AM(x, y, z, w) {x, y - x, z}

static const VkMapping _vk_mapping[] = {
	// Pageup stuff + up/down
	AM(VK_PRIOR,VK_DOWN, WKC_PAGEUP, WKC_DOWN),
	// Map letters & digits
	AM('A','Z','A','Z'),
	AM('0','9','0','9'),

	AS(VK_ESCAPE,   WKC_ESC),
	AS(VK_PAUSE,    WKC_PAUSE),
	AS(VK_BACK,     WKC_BACKSPACE),
	AM(VK_INSERT,   VK_DELETE, WKC_INSERT, WKC_DELETE),

	AS(VK_SPACE,    WKC_SPACE),
	AS(VK_RETURN,   WKC_RETURN),
	AS(VK_TAB,      WKC_TAB),

	// Function keys
	AM(VK_F1, VK_F12, WKC_F1, WKC_F12),

	// Numeric part.
	// What is the virtual keycode for numeric enter??
	AM(VK_NUMPAD0, VK_NUMPAD9, WKC_NUM_0, WKC_NUM_9),
	AS(VK_DIVIDE,   WKC_NUM_DIV),
	AS(VK_MULTIPLY, WKC_NUM_MUL),
	AS(VK_SUBTRACT, WKC_NUM_MINUS),
	AS(VK_ADD,      WKC_NUM_PLUS),
	AS(VK_DECIMAL,  WKC_NUM_DECIMAL)
};

static uint MapWindowsKey(uint sym)
{
	const VkMapping *map;
	uint key = 0;

	for (map = _vk_mapping; map != endof(_vk_mapping); ++map) {
		if ((uint)(sym - map->vk_from) <= map->vk_count) {
			key = sym - map->vk_from + map->map_to;
			break;
		}
	}

	if (GetAsyncKeyState(VK_SHIFT)   < 0) key |= WKC_SHIFT;
	if (GetAsyncKeyState(VK_CONTROL) < 0) key |= WKC_CTRL;
	if (GetAsyncKeyState(VK_MENU)    < 0) key |= WKC_ALT;
	return key;
}

static bool AllocateDibSection(int w, int h);

static void ClientSizeChanged(int w, int h)
{
	// allocate new dib section of the new size
	if (AllocateDibSection(w, h)) {
		// mark all palette colors dirty
		_pal_first_dirty = 0;
		_pal_count_dirty = 255;
		GameSizeChanged();

		// redraw screen
		if (_wnd.running) {
			_screen.dst_ptr = _wnd.buffer_bits;
			UpdateWindows();
		}
	}
}

#ifdef _DEBUG
// Keep this function here..
// It allows you to redraw the screen from within the MSVC debugger
int RedrawScreenDebug()
{
	HDC dc,dc2;
	static int _fooctr;
	HBITMAP old_bmp;
	HPALETTE old_palette;

	_screen.dst_ptr = _wnd.buffer_bits;
	UpdateWindows();

	dc = GetDC(_wnd.main_wnd);
	dc2 = CreateCompatibleDC(dc);

	old_bmp = (HBITMAP)SelectObject(dc2, _wnd.dib_sect);
	old_palette = SelectPalette(dc, _wnd.gdi_palette, FALSE);
	BitBlt(dc, 0, 0, _wnd.width, _wnd.height, dc2, 0, 0, SRCCOPY);
	SelectPalette(dc, old_palette, TRUE);
	SelectObject(dc2, old_bmp);
	DeleteDC(dc2);
	ReleaseDC(_wnd.main_wnd, dc);

	return _fooctr++;
}
#endif

/* Windows 95 will not have a WM_MOUSELEAVE message, so define it if needed */
#if !defined(WM_MOUSELEAVE)
#define WM_MOUSELEAVE 0x02A3
#endif
#define TID_POLLMOUSE 1
#define MOUSE_POLL_DELAY 75

static void CALLBACK TrackMouseTimerProc(HWND hwnd, UINT msg, UINT event, DWORD time)
{
	RECT rc;
	POINT pt;

	/* Get the rectangle of our window and translate it to screen coordinates.
	 * Compare this with the current screen coordinates of the mouse and if it
	 * falls outside of the area or our window we have left the window. */
	GetClientRect(hwnd, &rc);
	MapWindowPoints(hwnd, HWND_DESKTOP, (LPPOINT)(LPRECT)&rc, 2);
	GetCursorPos(&pt);

	if (!PtInRect(&rc, pt) || (WindowFromPoint(pt) != hwnd)) {
		KillTimer(hwnd, event);
		PostMessage(hwnd, WM_MOUSELEAVE, 0, 0L);
	}
}

static void MakeWindow(bool full_screen)
{
	_fullscreen = full_screen;

	// recreate window?
	if ((full_screen || _wnd.fullscreen) && _wnd.main_wnd) {
		DestroyWindow(_wnd.main_wnd);
		_wnd.main_wnd = 0;
	}

#if defined(WINCE)
	/* WinCE is always fullscreen */
#else
	if (full_screen) {
		DEVMODE settings;

		/* Make sure we are always at least the screen-depth of the blitter */
		if (_fullscreen_bpp < BlitterFactoryBase::GetCurrentBlitter()->GetScreenDepth()) _fullscreen_bpp = BlitterFactoryBase::GetCurrentBlitter()->GetScreenDepth();

		memset(&settings, 0, sizeof(settings));
		settings.dmSize = sizeof(settings);
		settings.dmFields =
			(_fullscreen_bpp != 0 ? DM_BITSPERPEL : 0) |
			DM_PELSWIDTH |
			DM_PELSHEIGHT |
			(_display_hz != 0 ? DM_DISPLAYFREQUENCY : 0);
		settings.dmBitsPerPel = _fullscreen_bpp;
		settings.dmPelsWidth  = _wnd.width_org;
		settings.dmPelsHeight = _wnd.height_org;
		settings.dmDisplayFrequency = _display_hz;

		if (ChangeDisplaySettings(&settings, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL) {
			MakeWindow(false);
			return;
		}
	} else if (_wnd.fullscreen) {
		// restore display?
		ChangeDisplaySettings(NULL, 0);
	}
#endif

	{
		RECT r;
		DWORD style, showstyle;
		int x, y, w, h;

		showstyle = SW_SHOWNORMAL;
		_wnd.fullscreen = full_screen;
		if (_wnd.fullscreen) {
			style = WS_POPUP;
			SetRect(&r, 0, 0, _wnd.width_org, _wnd.height_org);
		} else {
			style = WS_OVERLAPPEDWINDOW;
			/* On window creation, check if we were in maximize mode before */
			if (_window_maximize) showstyle = SW_SHOWMAXIMIZED;
			SetRect(&r, 0, 0, _wnd.width, _wnd.height);
		}

#if !defined(WINCE)
		AdjustWindowRect(&r, style, FALSE);
#endif
		w = r.right - r.left;
		h = r.bottom - r.top;
		x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
		y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;

		if (_wnd.main_wnd) {
			ShowWindow(_wnd.main_wnd, SW_SHOWNORMAL); // remove maximize-flag
			SetWindowPos(_wnd.main_wnd, 0, x, y, w, h, SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER);
		} else {
			extern const char _openttd_revision[];
			TCHAR Windowtitle[50];

			_sntprintf(Windowtitle, sizeof(Windowtitle), _T("OpenTTD %s"), MB_TO_WIDE(_openttd_revision));

			_wnd.main_wnd = CreateWindow(_T("OTTD"), Windowtitle, style, x, y, w, h, 0, 0, GetModuleHandle(NULL), 0);
			if (_wnd.main_wnd == NULL) error("CreateWindow failed");
			ShowWindow(_wnd.main_wnd, showstyle);
		}
	}
	GameSizeChanged(); // invalidate all windows, force redraw
}

static LRESULT CALLBACK WndProcGdi(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static uint32 keycode = 0;
	static bool console = false;

	switch (msg) {
		case WM_CREATE:
			SetTimer(hwnd, TID_POLLMOUSE, MOUSE_POLL_DELAY, (TIMERPROC)TrackMouseTimerProc);
			break;

		case WM_PAINT: {
			PAINTSTRUCT ps;
			HDC dc,dc2;
			HBITMAP old_bmp;
			HPALETTE old_palette;

			BeginPaint(hwnd, &ps);
			dc = ps.hdc;
			dc2 = CreateCompatibleDC(dc);
			old_bmp = (HBITMAP)SelectObject(dc2, _wnd.dib_sect);
			old_palette = SelectPalette(dc, _wnd.gdi_palette, FALSE);

			if (_pal_count_dirty != 0) {
				Blitter *blitter = BlitterFactoryBase::GetCurrentBlitter();

				switch (blitter->UsePaletteAnimation()) {
					case Blitter::PALETTE_ANIMATION_VIDEO_BACKEND:
						UpdatePalette(dc2, _pal_first_dirty, _pal_count_dirty);
						break;

					case Blitter::PALETTE_ANIMATION_BLITTER:
						blitter->PaletteAnimate(_pal_first_dirty, _pal_count_dirty);
						break;

					case Blitter::PALETTE_ANIMATION_NONE:
						break;

					default:
						NOT_REACHED();
				}
				_pal_count_dirty = 0;
			}

			BitBlt(dc, 0, 0, _wnd.width, _wnd.height, dc2, 0, 0, SRCCOPY);
			SelectPalette(dc, old_palette, TRUE);
			SelectObject(dc2, old_bmp);
			DeleteDC(dc2);
			EndPaint(hwnd, &ps);
			return 0;
		}

		case WM_PALETTECHANGED:
			if ((HWND)wParam == hwnd) return 0;
			/* FALLTHROUGH */

		case WM_QUERYNEWPALETTE: {
			HDC hDC = GetWindowDC(hwnd);
			HPALETTE hOldPalette = SelectPalette(hDC, _wnd.gdi_palette, FALSE);
			UINT nChanged = RealizePalette(hDC);

			SelectPalette(hDC, hOldPalette, TRUE);
			ReleaseDC(hwnd, hDC);
			if (nChanged) InvalidateRect(hwnd, NULL, FALSE);
			return 0;
		}

		case WM_CLOSE:
			HandleExitGameRequest();
			return 0;

		case WM_DESTROY:
			if (_window_maximize) {
				_cur_resolution[0] = _bck_resolution[0];
				_cur_resolution[1] = _bck_resolution[1];
			}
			return 0;

		case WM_LBUTTONDOWN:
			SetCapture(hwnd);
			_left_button_down = true;
			HandleMouseEvents();
			return 0;

		case WM_LBUTTONUP:
			ReleaseCapture();
			_left_button_down = false;
			_left_button_clicked = false;
			HandleMouseEvents();
			return 0;

		case WM_RBUTTONDOWN:
			SetCapture(hwnd);
			_right_button_down = true;
			_right_button_clicked = true;
			HandleMouseEvents();
			return 0;

		case WM_RBUTTONUP:
			ReleaseCapture();
			_right_button_down = false;
			HandleMouseEvents();
			return 0;

		case WM_MOUSELEAVE:
			UndrawMouseCursor();
			_cursor.in_window = false;

			if (!_left_button_down && !_right_button_down) MyShowCursor(true);
			HandleMouseEvents();
			return 0;

		case WM_MOUSEMOVE: {
			int x = (int16)LOWORD(lParam);
			int y = (int16)HIWORD(lParam);
			POINT pt;

			/* If the mouse was not in the window and it has moved it means it has
			 * come into the window, so start drawing the mouse. Also start
			 * tracking the mouse for exiting the window */
			if (!_cursor.in_window) {
				_cursor.in_window = true;
				SetTimer(hwnd, TID_POLLMOUSE, MOUSE_POLL_DELAY, (TIMERPROC)TrackMouseTimerProc);

				DrawMouseCursor();
			}

			if (_cursor.fix_at) {
				int dx = x - _cursor.pos.x;
				int dy = y - _cursor.pos.y;
				if (dx != 0 || dy != 0) {
					_cursor.delta.x += dx;
					_cursor.delta.y += dy;

					pt.x = _cursor.pos.x;
					pt.y = _cursor.pos.y;

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
			HandleMouseEvents();
			return 0;
		}

#if !defined(UNICODE)
		case WM_INPUTLANGCHANGE: {
			TCHAR locale[6];
			LCID lcid = GB(lParam, 0, 16);

			int len = GetLocaleInfo(lcid, LOCALE_IDEFAULTANSICODEPAGE, locale, lengthof(locale));
			if (len != 0) _codepage = _ttoi(locale);
			return 1;
		}
#endif /* UNICODE */

		case WM_DEADCHAR:
			console = GB(lParam, 16, 8) == 41;
			return 0;

		case WM_CHAR: {
			/* Silently drop all non-text messages as those were handled by WM_KEYDOWN */
			if (wParam < VK_SPACE) return 0;
			uint scancode = GB(lParam, 16, 8);
			uint charcode = wParam;

			/* If the console key is a dead-key, we need to press it twice to get a WM_CHAR message.
			 * But we then get two WM_CHAR messages, so ignore the first one */
			if (console && scancode == 41) {
				console = false;
				return 0;
			}

#if !defined(UNICODE)
			wchar_t w;
			int len = MultiByteToWideChar(_codepage, 0, (char*)&charcode, 1, &w, 1);
			charcode = len == 1 ? w : 0;
#endif /* UNICODE */

			/* No matter the keyboard layout, we will map the '~' to the console */
			scancode = scancode == 41 ? (int)WKC_BACKQUOTE : keycode;
			HandleKeypress(GB(charcode, 0, 16) | (scancode << 16));
			return 0;
		}

		case WM_KEYDOWN: {
			keycode = MapWindowsKey(wParam);

			/* Silently drop all text messages as those will be handled by WM_CHAR
			 * WM_KEYDOWN only handles CTRL+ commands and special keys like VK_LEFT, etc. */
			if (keycode == 0 || (keycode > WKC_PAUSE && GB(keycode, 13, 4) == 0)) return 0;

			HandleKeypress(0 | (keycode << 16));
			return 0;
		}

		case WM_SYSKEYDOWN: /* user presses F10 or Alt, both activating the title-menu */
			switch (wParam) {
				case VK_RETURN:
				case 'F': /* Full Screen on ALT + ENTER/F */
					ToggleFullScreen(!_wnd.fullscreen);
					return 0;

				case VK_MENU: /* Just ALT */
					return 0; // do nothing

				case VK_F10: /* F10, ignore activation of menu */
					HandleKeypress(MapWindowsKey(wParam) << 16);
					return 0;

				default: /* ALT in combination with something else */
					HandleKeypress(MapWindowsKey(wParam) << 16);
					break;
			}
			break;

		case WM_SIZE:
			_wnd.minimized = (wParam == SIZE_MINIMIZED);
			if (!_wnd.minimized) {
				/* Set maximized flag when we maximize (obviously), but also when we
				 * switched to fullscreen from a maximized state */
				_window_maximize = (wParam == SIZE_MAXIMIZED || (_window_maximize && _fullscreen));
				if (_window_maximize) {
					_bck_resolution[0] = _cur_resolution[0];
					_bck_resolution[1] = _cur_resolution[1];
				}
				ClientSizeChanged(LOWORD(lParam), HIWORD(lParam));
			}
			return 0;

#if !defined(WINCE)
		case WM_SIZING: {
			RECT* r = (RECT*)lParam;
			RECT r2;
			int w, h;

			SetRect(&r2, 0, 0, 0, 0);
			AdjustWindowRect(&r2, GetWindowLong(hwnd, GWL_STYLE), FALSE);

			w = r->right - r->left - (r2.right - r2.left);
			h = r->bottom - r->top - (r2.bottom - r2.top);
			w = clamp(w, 64, MAX_SCREEN_WIDTH);
			h = clamp(h, 64, MAX_SCREEN_HEIGHT);
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
#endif

// needed for wheel
#if !defined(WM_MOUSEWHEEL)
# define WM_MOUSEWHEEL 0x020A
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
			HandleMouseEvents();
			return 0;
		}

		case WM_ACTIVATEAPP:
			_wnd.has_focus = (wParam != 0);
#if !defined(WINCE)
			if (_wnd.fullscreen) {
				if (_wnd.has_focus && _wnd.minimized) {
					/* Restore the game window */
					ShowWindow(hwnd, SW_RESTORE);
					MakeWindow(true);
				} else if (!_wnd.has_focus && !_wnd.minimized) {
					/* Minimise the window and restore desktop */
					ShowWindow(hwnd, SW_MINIMIZE);
					ChangeDisplaySettings(NULL, 0);
				}
			}
#endif
			break;
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

static void RegisterWndClass()
{
	static bool registered = false;

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
			_T("OTTD")
		};

		registered = true;
		if (!RegisterClass(&wnd)) error("RegisterClass failed");
	}
}

static bool AllocateDibSection(int w, int h)
{
	BITMAPINFO *bi;
	HDC dc;
	int bpp = BlitterFactoryBase::GetCurrentBlitter()->GetScreenDepth();

	w = clamp(w, 64, MAX_SCREEN_WIDTH);
	h = clamp(h, 64, MAX_SCREEN_HEIGHT);

	if (bpp == 0) error("Can't use a blitter that blits 0 bpp for normal visuals");

	if (w == _screen.width && h == _screen.height)
		return false;

	_screen.width = w;
	_screen.pitch = (bpp == 8) ? ALIGN(w, 4) : w;
	_screen.height = h;
	bi = (BITMAPINFO*)alloca(sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * 256);
	memset(bi, 0, sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * 256);
	bi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);

	bi->bmiHeader.biWidth = _wnd.width = w;
	bi->bmiHeader.biHeight = -(_wnd.height = h);

	bi->bmiHeader.biPlanes = 1;
	bi->bmiHeader.biBitCount = BlitterFactoryBase::GetCurrentBlitter()->GetScreenDepth();
	bi->bmiHeader.biCompression = BI_RGB;

	if (_wnd.dib_sect) DeleteObject(_wnd.dib_sect);

	dc = GetDC(0);
	_wnd.dib_sect = CreateDIBSection(dc, bi, DIB_RGB_COLORS, (VOID**)&_wnd.buffer_bits, NULL, 0);
	if (_wnd.dib_sect == NULL) error("CreateDIBSection failed");
	ReleaseDC(0, dc);

	return true;
}

static const uint16 default_resolutions[][2] = {
	{  640,  480 },
	{  800,  600 },
	{ 1024,  768 },
	{ 1152,  864 },
	{ 1280,  800 },
	{ 1280,  960 },
	{ 1280, 1024 },
	{ 1400, 1050 },
	{ 1600, 1200 },
	{ 1680, 1050 },
	{ 1920, 1200 }
};

static void FindResolutions()
{
	uint n = 0;
#if defined(WINCE)
	/* EnumDisplaySettingsW is only supported in CE 4.2+ */
	/* XXX -- One might argue that we assume 4.2+ on every system. Then we can use this function safely */
#else
	uint i;
	DEVMODEA dm;

	/* XXX - EnumDisplaySettingsW crashes with unicows.dll on Windows95
	 * Doesn't really matter since we don't pass a string anyways, but still
	 * a letdown */
	for (i = 0; EnumDisplaySettingsA(NULL, i, &dm) != 0; i++) {
		if (dm.dmBitsPerPel == BlitterFactoryBase::GetCurrentBlitter()->GetScreenDepth() && IS_INT_INSIDE(dm.dmPelsWidth, 640, MAX_SCREEN_WIDTH + 1) &&
				IS_INT_INSIDE(dm.dmPelsHeight, 480, MAX_SCREEN_HEIGHT + 1)) {
			uint j;

			for (j = 0; j < n; j++) {
				if (_resolutions[j][0] == dm.dmPelsWidth && _resolutions[j][1] == dm.dmPelsHeight) break;
			}

			/* In the previous loop we have checked already existing/added resolutions if
			 * they are the same as the new ones. If this is not the case (j == n); we have
			 * looped all and found none, add the new one to the list. If we have reached the
			 * maximum amount of resolutions, then quit querying the display */
			if (j == n) {
				_resolutions[j][0] = dm.dmPelsWidth;
				_resolutions[j][1] = dm.dmPelsHeight;
				if (++n == lengthof(_resolutions)) break;
			}
		}
	}
#endif

	/* We have found no resolutions, show the default list */
	if (n == 0) {
		memcpy(_resolutions, default_resolutions, sizeof(default_resolutions));
		n = lengthof(default_resolutions);
	}

	_num_resolutions = n;
	SortResolutions(_num_resolutions);
}


static const char *Win32GdiStart(const char * const *parm)
{
	memset(&_wnd, 0, sizeof(_wnd));

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
	DeleteObject(_wnd.gdi_palette);
	DeleteObject(_wnd.dib_sect);
	DestroyWindow(_wnd.main_wnd);

#if !defined(WINCE)
	if (_wnd.fullscreen) ChangeDisplaySettings(NULL, 0);
#endif
	MyShowCursor(true);
}

static void Win32GdiMakeDirty(int left, int top, int width, int height)
{
	RECT r = { left, top, left + width, top + height };

	InvalidateRect(_wnd.main_wnd, &r, FALSE);
}

static void CheckPaletteAnim()
{
	if (_pal_count_dirty == 0)
		return;
	InvalidateRect(_wnd.main_wnd, NULL, FALSE);
}

static void Win32GdiMainLoop()
{
	MSG mesg;
	uint32 cur_ticks = GetTickCount();
	uint32 next_tick = cur_ticks + 30;

	_wnd.running = true;

	for (;;) {
		uint32 prev_cur_ticks = cur_ticks; // to check for wrapping

		while (PeekMessage(&mesg, NULL, 0, 0, PM_REMOVE)) {
			InteractiveRandom(); // randomness
			TranslateMessage(&mesg);
			DispatchMessage(&mesg);
		}
		if (_exit_game) return;

#if defined(_DEBUG)
		if (_wnd.has_focus && GetAsyncKeyState(VK_SHIFT) < 0 &&
#else
		/* Speed up using TAB, but disable for ALT+TAB of course */
		if (_wnd.has_focus && GetAsyncKeyState(VK_TAB) < 0 && GetAsyncKeyState(VK_MENU) >= 0 &&
#endif
			  !_networking && _game_mode != GM_MENU) {
			_fast_forward |= 2;
		} else if (_fast_forward & 2) {
			_fast_forward = 0;
		}

		cur_ticks = GetTickCount();
		if (cur_ticks >= next_tick || (_fast_forward && !_pause_game) || cur_ticks < prev_cur_ticks) {
			next_tick = cur_ticks + 30;
			_ctrl_pressed = _wnd.has_focus && GetAsyncKeyState(VK_CONTROL)<0;
			_shift_pressed = _wnd.has_focus && GetAsyncKeyState(VK_SHIFT)<0;
#ifdef _DEBUG
			_dbg_screen_rect = _wnd.has_focus && GetAsyncKeyState(VK_CAPITAL)<0;
#endif

			// determine which directional keys are down
			if (_wnd.has_focus) {
				_dirkeys =
					(GetAsyncKeyState(VK_LEFT) < 0 ? 1 : 0) +
					(GetAsyncKeyState(VK_UP) < 0 ? 2 : 0) +
					(GetAsyncKeyState(VK_RIGHT) < 0 ? 4 : 0) +
					(GetAsyncKeyState(VK_DOWN) < 0 ? 8 : 0);
			} else {
				_dirkeys = 0;
			}

			GameLoop();
			_cursor.delta.x = _cursor.delta.y = 0;

			if (_force_full_redraw) MarkWholeScreenDirty();

#if !defined(WINCE)
			GdiFlush();
#endif
			_screen.dst_ptr = _wnd.buffer_bits;
			UpdateWindows();
			CheckPaletteAnim();
		} else {
			Sleep(1);
#if !defined(WINCE)
			GdiFlush();
#endif
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

static void Win32GdiFullScreen(bool full_screen)
{
	MakeWindow(full_screen);
}

const HalVideoDriver _win32_video_driver = {
	Win32GdiStart,
	Win32GdiStop,
	Win32GdiMakeDirty,
	Win32GdiMainLoop,
	Win32GdiChangeRes,
	Win32GdiFullScreen,
};
