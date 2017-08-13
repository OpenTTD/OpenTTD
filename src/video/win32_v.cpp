/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file win32_v.cpp Implementation of the Windows (GDI) video driver. */

#include "../stdafx.h"
#include "../openttd.h"
#include "../gfx_func.h"
#include "../os/windows/win32.h"
#include "../rev.h"
#include "../blitter/factory.hpp"
#include "../network/network.h"
#include "../core/math_func.hpp"
#include "../core/random_func.hpp"
#include "../texteff.hpp"
#include "../thread/thread.h"
#include "../progress.h"
#include "../window_gui.h"
#include "../window_func.h"
#include "win32_v.h"
#include <windows.h>
#include <imm.h>

#include "../safeguards.h"

/* Missing define in MinGW headers. */
#ifndef MAPVK_VK_TO_CHAR
#define MAPVK_VK_TO_CHAR    (2)
#endif

static struct {
	HWND main_wnd;
	HBITMAP dib_sect;
	void *buffer_bits;
	HPALETTE gdi_palette;
	RECT update_rect;
	int width;
	int height;
	int width_org;
	int height_org;
	bool fullscreen;
	bool has_focus;
	bool running;
} _wnd;

bool _force_full_redraw;
bool _window_maximize;
uint _display_hz;
static Dimension _bck_resolution;
#if !defined(WINCE) || _WIN32_WCE >= 0x400
DWORD _imm_props;
#endif

/** Whether the drawing is/may be done in a separate thread. */
static bool _draw_threaded;
/** Thread used to 'draw' to the screen, i.e. push data to the screen. */
static ThreadObject *_draw_thread = NULL;
/** Mutex to keep the access to the shared memory controlled. */
static ThreadMutex *_draw_mutex = NULL;
/** Event that is signaled when the drawing thread has finished initializing. */
static HANDLE _draw_thread_initialized = NULL;
/** Should we keep continue drawing? */
static volatile bool _draw_continue;
/** Local copy of the palette for use in the drawing thread. */
static Palette _local_palette;

static void MakePalette()
{
	LOGPALETTE *pal = (LOGPALETTE*)alloca(sizeof(LOGPALETTE) + (256 - 1) * sizeof(PALETTEENTRY));

	pal->palVersion = 0x300;
	pal->palNumEntries = 256;

	for (uint i = 0; i != 256; i++) {
		pal->palPalEntry[i].peRed   = _cur_palette.palette[i].r;
		pal->palPalEntry[i].peGreen = _cur_palette.palette[i].g;
		pal->palPalEntry[i].peBlue  = _cur_palette.palette[i].b;
		pal->palPalEntry[i].peFlags = 0;

	}
	_wnd.gdi_palette = CreatePalette(pal);
	if (_wnd.gdi_palette == NULL) usererror("CreatePalette failed!\n");

	_cur_palette.first_dirty = 0;
	_cur_palette.count_dirty = 256;
	_local_palette = _cur_palette;
}

static void UpdatePalette(HDC dc, uint start, uint count)
{
	RGBQUAD rgb[256];
	uint i;

	for (i = 0; i != count; i++) {
		rgb[i].rgbRed   = _local_palette.palette[start + i].r;
		rgb[i].rgbGreen = _local_palette.palette[start + i].g;
		rgb[i].rgbBlue  = _local_palette.palette[start + i].b;
		rgb[i].rgbReserved = 0;
	}

	SetDIBColorTable(dc, start, count, rgb);
}

bool VideoDriver_Win32::ClaimMousePointer()
{
	MyShowCursor(false, true);
	return true;
}

struct VkMapping {
	byte vk_from;
	byte vk_count;
	byte map_to;
};

#define AS(x, z) {x, 0, z}
#define AM(x, y, z, w) {x, y - x, z}

static const VkMapping _vk_mapping[] = {
	/* Pageup stuff + up/down */
	AM(VK_PRIOR, VK_DOWN, WKC_PAGEUP, WKC_DOWN),
	/* Map letters & digits */
	AM('A', 'Z', 'A', 'Z'),
	AM('0', '9', '0', '9'),

	AS(VK_ESCAPE,   WKC_ESC),
	AS(VK_PAUSE,    WKC_PAUSE),
	AS(VK_BACK,     WKC_BACKSPACE),
	AM(VK_INSERT,   VK_DELETE, WKC_INSERT, WKC_DELETE),

	AS(VK_SPACE,    WKC_SPACE),
	AS(VK_RETURN,   WKC_RETURN),
	AS(VK_TAB,      WKC_TAB),

	/* Function keys */
	AM(VK_F1, VK_F12, WKC_F1, WKC_F12),

	/* Numeric part */
	AM(VK_NUMPAD0, VK_NUMPAD9, '0', '9'),
	AS(VK_DIVIDE,   WKC_NUM_DIV),
	AS(VK_MULTIPLY, WKC_NUM_MUL),
	AS(VK_SUBTRACT, WKC_NUM_MINUS),
	AS(VK_ADD,      WKC_NUM_PLUS),
	AS(VK_DECIMAL,  WKC_NUM_DECIMAL),

	/* Other non-letter keys */
	AS(0xBF,  WKC_SLASH),
	AS(0xBA,  WKC_SEMICOLON),
	AS(0xBB,  WKC_EQUALS),
	AS(0xDB,  WKC_L_BRACKET),
	AS(0xDC,  WKC_BACKSLASH),
	AS(0xDD,  WKC_R_BRACKET),

	AS(0xDE,  WKC_SINGLEQUOTE),
	AS(0xBC,  WKC_COMMA),
	AS(0xBD,  WKC_MINUS),
	AS(0xBE,  WKC_PERIOD)
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

static bool AllocateDibSection(int w, int h, bool force = false);

static void ClientSizeChanged(int w, int h)
{
	/* allocate new dib section of the new size */
	if (AllocateDibSection(w, h)) {
		/* mark all palette colours dirty */
		_cur_palette.first_dirty = 0;
		_cur_palette.count_dirty = 256;
		_local_palette = _cur_palette;

		BlitterFactory::GetCurrentBlitter()->PostResize();

		GameSizeChanged();
	}
}

#ifdef _DEBUG
/* Keep this function here..
 * It allows you to redraw the screen from within the MSVC debugger */
int RedrawScreenDebug()
{
	HDC dc, dc2;
	static int _fooctr;
	HBITMAP old_bmp;
	HPALETTE old_palette;

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

/**
 * Instantiate a new window.
 * @param full_screen Whether to make a full screen window or not.
 * @return True if the window could be created.
 */
bool VideoDriver_Win32::MakeWindow(bool full_screen)
{
	_fullscreen = full_screen;

	/* recreate window? */
	if ((full_screen || _wnd.fullscreen) && _wnd.main_wnd) {
		DestroyWindow(_wnd.main_wnd);
		_wnd.main_wnd = 0;
	}

#if defined(WINCE)
	/* WinCE is always fullscreen */
#else
	if (full_screen) {
		DEVMODE settings;

		memset(&settings, 0, sizeof(settings));
		settings.dmSize = sizeof(settings);
		settings.dmFields =
			DM_BITSPERPEL |
			DM_PELSWIDTH |
			DM_PELSHEIGHT |
			(_display_hz != 0 ? DM_DISPLAYFREQUENCY : 0);
		settings.dmBitsPerPel = BlitterFactory::GetCurrentBlitter()->GetScreenDepth();
		settings.dmPelsWidth  = _wnd.width_org;
		settings.dmPelsHeight = _wnd.height_org;
		settings.dmDisplayFrequency = _display_hz;

		/* Check for 8 bpp support. */
		if (settings.dmBitsPerPel == 8 &&
				(_support8bpp != S8BPP_HARDWARE || ChangeDisplaySettings(&settings, CDS_FULLSCREEN | CDS_TEST) != DISP_CHANGE_SUCCESSFUL)) {
			settings.dmBitsPerPel = 32;
		}

		/* Test fullscreen with current resolution, if it fails use desktop resolution. */
		if (ChangeDisplaySettings(&settings, CDS_FULLSCREEN | CDS_TEST) != DISP_CHANGE_SUCCESSFUL) {
			RECT r;
			GetWindowRect(GetDesktopWindow(), &r);
			/* Guard against recursion. If we already failed here once, just fall through to
			 * the next ChangeDisplaySettings call which will fail and error out appropriately. */
			if ((int)settings.dmPelsWidth != r.right - r.left || (int)settings.dmPelsHeight != r.bottom - r.top) {
				return this->ChangeResolution(r.right - r.left, r.bottom - r.top);
			}
		}

		if (ChangeDisplaySettings(&settings, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL) {
			this->MakeWindow(false);  // don't care about the result
			return false;  // the request failed
		}
	} else if (_wnd.fullscreen) {
		/* restore display? */
		ChangeDisplaySettings(NULL, 0);
		/* restore the resolution */
		_wnd.width = _bck_resolution.width;
		_wnd.height = _bck_resolution.height;
	}
#endif

	{
		RECT r;
		DWORD style, showstyle;
		int w, h;

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

		if (_wnd.main_wnd != NULL) {
			if (!_window_maximize) SetWindowPos(_wnd.main_wnd, 0, 0, 0, w, h, SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_NOMOVE);
		} else {
			TCHAR Windowtitle[50];
			int x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
			int y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;

			_sntprintf(Windowtitle, lengthof(Windowtitle), _T("OpenTTD %s"), MB_TO_WIDE(_openttd_revision));

			_wnd.main_wnd = CreateWindow(_T("OTTD"), Windowtitle, style, x, y, w, h, 0, 0, GetModuleHandle(NULL), 0);
			if (_wnd.main_wnd == NULL) usererror("CreateWindow failed");
			ShowWindow(_wnd.main_wnd, showstyle);
		}
	}

	BlitterFactory::GetCurrentBlitter()->PostResize();

	GameSizeChanged(); // invalidate all windows, force redraw
	return true; // the request succeeded
}

/** Do palette animation and blit to the window. */
static void PaintWindow(HDC dc)
{
	HDC dc2 = CreateCompatibleDC(dc);
	HBITMAP old_bmp = (HBITMAP)SelectObject(dc2, _wnd.dib_sect);
	HPALETTE old_palette = SelectPalette(dc, _wnd.gdi_palette, FALSE);

	if (_cur_palette.count_dirty != 0) {
		Blitter *blitter = BlitterFactory::GetCurrentBlitter();

		switch (blitter->UsePaletteAnimation()) {
			case Blitter::PALETTE_ANIMATION_VIDEO_BACKEND:
				UpdatePalette(dc2, _local_palette.first_dirty, _local_palette.count_dirty);
				break;

			case Blitter::PALETTE_ANIMATION_BLITTER:
				blitter->PaletteAnimate(_local_palette);
				break;

			case Blitter::PALETTE_ANIMATION_NONE:
				break;

			default:
				NOT_REACHED();
		}
		_cur_palette.count_dirty = 0;
	}

	BitBlt(dc, 0, 0, _wnd.width, _wnd.height, dc2, 0, 0, SRCCOPY);
	SelectPalette(dc, old_palette, TRUE);
	SelectObject(dc2, old_bmp);
	DeleteDC(dc2);
}

static void PaintWindowThread(void *)
{
	/* First tell the main thread we're started */
	_draw_mutex->BeginCritical();
	SetEvent(_draw_thread_initialized);

	/* Now wait for the first thing to draw! */
	_draw_mutex->WaitForSignal();

	while (_draw_continue) {
		/* Convert update region from logical to device coordinates. */
		POINT pt = {0, 0};
		ClientToScreen(_wnd.main_wnd, &pt);
		OffsetRect(&_wnd.update_rect, pt.x, pt.y);

		/* Create a device context that is clipped to the region we need to draw.
		 * GetDCEx 'consumes' the update region, so we may not destroy it ourself. */
		HRGN rgn = CreateRectRgnIndirect(&_wnd.update_rect);
		HDC dc = GetDCEx(_wnd.main_wnd, rgn, DCX_CLIPSIBLINGS | DCX_CLIPCHILDREN | DCX_INTERSECTRGN);

		PaintWindow(dc);

		/* Clear update rect. */
		SetRectEmpty(&_wnd.update_rect);
		ReleaseDC(_wnd.main_wnd, dc);

		/* Flush GDI buffer to ensure drawing here doesn't conflict with any GDI usage in the main WndProc. */
		GdiFlush();

		_draw_mutex->WaitForSignal();
	}

	_draw_mutex->EndCritical();
	_draw_thread->Exit();
}

/** Forward key presses to the window system. */
static LRESULT HandleCharMsg(uint keycode, WChar charcode)
{
#if !defined(UNICODE)
	static char prev_char = 0;

	char input[2] = {(char)charcode, 0};
	int input_len = 1;

	if (prev_char != 0) {
		/* We stored a lead byte previously, combine it with this byte. */
		input[0] = prev_char;
		input[1] = (char)charcode;
		input_len = 2;
	} else if (IsDBCSLeadByte(charcode)) {
		/* We got a lead byte, store and exit. */
		prev_char = charcode;
		return 0;
	}
	prev_char = 0;

	wchar_t w[2]; // Can get up to two code points as a result.
	int len = MultiByteToWideChar(CP_ACP, 0, input, input_len, w, 2);
	switch (len) {
		case 1: // Normal unicode character.
			charcode = w[0];
			break;

		case 2: // Got an UTF-16 surrogate pair back.
			charcode = Utf16DecodeSurrogate(w[0], w[1]);
			break;

		default: // Some kind of error.
			DEBUG(driver, 1, "Invalid DBCS character sequence encountered, dropping input");
			charcode = 0;
			break;
	}
#else
	static WChar prev_char = 0;

	/* Did we get a lead surrogate? If yes, store and exit. */
	if (Utf16IsLeadSurrogate(charcode)) {
		if (prev_char != 0) DEBUG(driver, 1, "Got two UTF-16 lead surrogates, dropping the first one");
		prev_char = charcode;
		return 0;
	}

	/* Stored lead surrogate and incoming trail surrogate? Combine and forward to input handling. */
	if (prev_char != 0) {
		if (Utf16IsTrailSurrogate(charcode)) {
			charcode = Utf16DecodeSurrogate(prev_char, charcode);
		} else {
			DEBUG(driver, 1, "Got an UTF-16 lead surrogate without a trail surrogate, dropping the lead surrogate");
		}
	}
	prev_char = 0;
#endif /* UNICODE */

	HandleKeypress(keycode, charcode);

	return 0;
}

#if !defined(WINCE) || _WIN32_WCE >= 0x400
/** Should we draw the composition string ourself, i.e is this a normal IME? */
static bool DrawIMECompositionString()
{
	return (_imm_props & IME_PROP_AT_CARET) && !(_imm_props & IME_PROP_SPECIAL_UI);
}

/** Set position of the composition window to the caret position. */
static void SetCompositionPos(HWND hwnd)
{
	HIMC hIMC = ImmGetContext(hwnd);
	if (hIMC != NULL) {
		COMPOSITIONFORM cf;
		cf.dwStyle = CFS_POINT;

		if (EditBoxInGlobalFocus()) {
			/* Get caret position. */
			Point pt = _focused_window->GetCaretPosition();
			cf.ptCurrentPos.x = _focused_window->left + pt.x;
			cf.ptCurrentPos.y = _focused_window->top  + pt.y;
		} else {
			cf.ptCurrentPos.x = 0;
			cf.ptCurrentPos.y = 0;
		}
		ImmSetCompositionWindow(hIMC, &cf);
	}
	ImmReleaseContext(hwnd, hIMC);
}

/** Set the position of the candidate window. */
static void SetCandidatePos(HWND hwnd)
{
	HIMC hIMC = ImmGetContext(hwnd);
	if (hIMC != NULL) {
		CANDIDATEFORM cf;
		cf.dwIndex = 0;
		cf.dwStyle = CFS_EXCLUDE;

		if (EditBoxInGlobalFocus()) {
			Point pt = _focused_window->GetCaretPosition();
			cf.ptCurrentPos.x = _focused_window->left + pt.x;
			cf.ptCurrentPos.y = _focused_window->top  + pt.y;
			if (_focused_window->window_class == WC_CONSOLE) {
				cf.rcArea.left   = _focused_window->left;
				cf.rcArea.top    = _focused_window->top;
				cf.rcArea.right  = _focused_window->left + _focused_window->width;
				cf.rcArea.bottom = _focused_window->top  + _focused_window->height;
			} else {
				cf.rcArea.left   = _focused_window->left + _focused_window->nested_focus->pos_x;
				cf.rcArea.top    = _focused_window->top  + _focused_window->nested_focus->pos_y;
				cf.rcArea.right  = cf.rcArea.left + _focused_window->nested_focus->current_x;
				cf.rcArea.bottom = cf.rcArea.top  + _focused_window->nested_focus->current_y;
			}
		} else {
			cf.ptCurrentPos.x = 0;
			cf.ptCurrentPos.y = 0;
			SetRectEmpty(&cf.rcArea);
		}
		ImmSetCandidateWindow(hIMC, &cf);
	}
	ImmReleaseContext(hwnd, hIMC);
}

/** Handle WM_IME_COMPOSITION messages. */
static LRESULT HandleIMEComposition(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
	HIMC hIMC = ImmGetContext(hwnd);

	if (hIMC != NULL) {
		if (lParam & GCS_RESULTSTR) {
			/* Read result string from the IME. */
			LONG len = ImmGetCompositionString(hIMC, GCS_RESULTSTR, NULL, 0); // Length is always in bytes, even in UNICODE build.
			TCHAR *str = (TCHAR *)_alloca(len + sizeof(TCHAR));
			len = ImmGetCompositionString(hIMC, GCS_RESULTSTR, str, len);
			str[len / sizeof(TCHAR)] = '\0';

			/* Transmit text to windowing system. */
			if (len > 0) {
				HandleTextInput(NULL, true); // Clear marked string.
				HandleTextInput(FS2OTTD(str));
			}
			SetCompositionPos(hwnd);

			/* Don't pass the result string on to the default window proc. */
			lParam &= ~(GCS_RESULTSTR | GCS_RESULTCLAUSE | GCS_RESULTREADCLAUSE | GCS_RESULTREADSTR);
		}

		if ((lParam & GCS_COMPSTR) && DrawIMECompositionString()) {
			/* Read composition string from the IME. */
			LONG len = ImmGetCompositionString(hIMC, GCS_COMPSTR, NULL, 0); // Length is always in bytes, even in UNICODE build.
			TCHAR *str = (TCHAR *)_alloca(len + sizeof(TCHAR));
			len = ImmGetCompositionString(hIMC, GCS_COMPSTR, str, len);
			str[len / sizeof(TCHAR)] = '\0';

			if (len > 0) {
				static char utf8_buf[1024];
				convert_from_fs(str, utf8_buf, lengthof(utf8_buf));

				/* Convert caret position from bytes in the input string to a position in the UTF-8 encoded string. */
				LONG caret_bytes = ImmGetCompositionString(hIMC, GCS_CURSORPOS, NULL, 0);
				const char *caret = utf8_buf;
				for (const TCHAR *c = str; *c != '\0' && *caret != '\0' && caret_bytes > 0; c++, caret_bytes--) {
					/* Skip DBCS lead bytes or leading surrogates. */
#ifdef UNICODE
					if (Utf16IsLeadSurrogate(*c)) {
#else
					if (IsDBCSLeadByte(*c)) {
#endif
						c++;
						caret_bytes--;
					}
					Utf8Consume(&caret);
				}

				HandleTextInput(utf8_buf, true, caret);
			} else {
				HandleTextInput(NULL, true);
			}

			lParam &= ~(GCS_COMPSTR | GCS_COMPATTR | GCS_COMPCLAUSE | GCS_CURSORPOS | GCS_DELTASTART);
		}
	}
	ImmReleaseContext(hwnd, hIMC);

	return lParam != 0 ? DefWindowProc(hwnd, WM_IME_COMPOSITION, wParam, lParam) : 0;
}

/** Clear the current composition string. */
static void CancelIMEComposition(HWND hwnd)
{
	HIMC hIMC = ImmGetContext(hwnd);
	if (hIMC != NULL) ImmNotifyIME(hIMC, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
	ImmReleaseContext(hwnd, hIMC);
	/* Clear any marked string from the current edit box. */
	HandleTextInput(NULL, true);
}

#else

static bool DrawIMECompositionString() { return false; }
static void SetCompositionPos(HWND hwnd) {}
static void SetCandidatePos(HWND hwnd) {}
static void CancelIMEComposition(HWND hwnd) {}

#endif /* !defined(WINCE) || _WIN32_WCE >= 0x400 */

static LRESULT CALLBACK WndProcGdi(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static uint32 keycode = 0;
	static bool console = false;
	static bool in_sizemove = false;

	switch (msg) {
		case WM_CREATE:
			SetTimer(hwnd, TID_POLLMOUSE, MOUSE_POLL_DELAY, (TIMERPROC)TrackMouseTimerProc);
			SetCompositionPos(hwnd);
#if !defined(WINCE) || _WIN32_WCE >= 0x400
			_imm_props = ImmGetProperty(GetKeyboardLayout(0), IGP_PROPERTY);
#endif
			break;

		case WM_ENTERSIZEMOVE:
			in_sizemove = true;
			break;

		case WM_EXITSIZEMOVE:
			in_sizemove = false;
			break;

		case WM_PAINT:
			if (!in_sizemove && _draw_mutex != NULL && !HasModalProgress()) {
				/* Get the union of the old update rect and the new update rect. */
				RECT r;
				GetUpdateRect(hwnd, &r, FALSE);
				UnionRect(&_wnd.update_rect, &_wnd.update_rect, &r);

				/* Mark the window as updated, otherwise Windows would send more WM_PAINT messages. */
				ValidateRect(hwnd, NULL);
				_draw_mutex->SendSignal();
			} else {
				PAINTSTRUCT ps;

				BeginPaint(hwnd, &ps);
				PaintWindow(ps.hdc);
				EndPaint(hwnd, &ps);
			}
			return 0;

		case WM_PALETTECHANGED:
			if ((HWND)wParam == hwnd) return 0;
			FALLTHROUGH;

		case WM_QUERYNEWPALETTE: {
			HDC hDC = GetWindowDC(hwnd);
			HPALETTE hOldPalette = SelectPalette(hDC, _wnd.gdi_palette, FALSE);
			UINT nChanged = RealizePalette(hDC);

			SelectPalette(hDC, hOldPalette, TRUE);
			ReleaseDC(hwnd, hDC);
			if (nChanged != 0) InvalidateRect(hwnd, NULL, FALSE);
			return 0;
		}

		case WM_CLOSE:
			HandleExitGameRequest();
			return 0;

		case WM_DESTROY:
			if (_window_maximize) _cur_resolution = _bck_resolution;
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
			}

			if (_cursor.UpdateCursorPosition(x, y, true)) {
				pt.x = _cursor.pos.x;
				pt.y = _cursor.pos.y;
				ClientToScreen(hwnd, &pt);
				SetCursorPos(pt.x, pt.y);
			}
			MyShowCursor(false);
			HandleMouseEvents();
			return 0;
		}

#if !defined(WINCE) || _WIN32_WCE >= 0x400
		case WM_INPUTLANGCHANGE:
			_imm_props = ImmGetProperty(GetKeyboardLayout(0), IGP_PROPERTY);
			break;

		case WM_IME_SETCONTEXT:
			/* Don't show the composition window if we draw the string ourself. */
			if (DrawIMECompositionString()) lParam &= ~ISC_SHOWUICOMPOSITIONWINDOW;
			break;

		case WM_IME_STARTCOMPOSITION:
			SetCompositionPos(hwnd);
			if (DrawIMECompositionString()) return 0;
			break;

		case WM_IME_COMPOSITION:
			return HandleIMEComposition(hwnd, wParam, lParam);

		case WM_IME_ENDCOMPOSITION:
			/* Clear any pending composition string. */
			HandleTextInput(NULL, true);
			if (DrawIMECompositionString()) return 0;
			break;

		case WM_IME_NOTIFY:
			if (wParam == IMN_OPENCANDIDATE) SetCandidatePos(hwnd);
			break;

#if !defined(UNICODE)
		case WM_IME_CHAR:
			if (GB(wParam, 8, 8) != 0) {
				/* DBCS character, send lead byte first. */
				HandleCharMsg(0, GB(wParam, 8, 8));
			}
			HandleCharMsg(0, GB(wParam, 0, 8));
			return 0;
#endif
#endif

		case WM_DEADCHAR:
			console = GB(lParam, 16, 8) == 41;
			return 0;

		case WM_CHAR: {
			uint scancode = GB(lParam, 16, 8);
			uint charcode = wParam;

			/* If the console key is a dead-key, we need to press it twice to get a WM_CHAR message.
			 * But we then get two WM_CHAR messages, so ignore the first one */
			if (console && scancode == 41) {
				console = false;
				return 0;
			}

			/* IMEs and other input methods sometimes send a WM_CHAR without a WM_KEYDOWN,
			 * clear the keycode so a previous WM_KEYDOWN doesn't become 'stuck'. */
			uint cur_keycode = keycode;
			keycode = 0;

			return HandleCharMsg(cur_keycode, charcode);
		}

		case WM_KEYDOWN: {
			/* No matter the keyboard layout, we will map the '~' to the console. */
			uint scancode = GB(lParam, 16, 8);
			keycode = scancode == 41 ? (uint)WKC_BACKQUOTE : MapWindowsKey(wParam);

			/* Silently drop all messages handled by WM_CHAR. */
			MSG msg;
			if (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) {
				if ((msg.message == WM_CHAR || msg.message == WM_DEADCHAR) && GB(lParam, 16, 8) == GB(msg.lParam, 16, 8)) {
					return 0;
				}
			}

			uint charcode = MapVirtualKey(wParam, MAPVK_VK_TO_CHAR);

			/* No character translation? */
			if (charcode == 0) {
				HandleKeypress(keycode, 0);
				return 0;
			}

			/* Is the console key a dead key? If yes, ignore the first key down event. */
			if (HasBit(charcode, 31) && !console) {
				if (scancode == 41) {
					console = true;
					return 0;
				}
			}
			console = false;

			/* IMEs and other input methods sometimes send a WM_CHAR without a WM_KEYDOWN,
			 * clear the keycode so a previous WM_KEYDOWN doesn't become 'stuck'. */
			uint cur_keycode = keycode;
			keycode = 0;

			return HandleCharMsg(cur_keycode, LOWORD(charcode));
		}

		case WM_SYSKEYDOWN: // user presses F10 or Alt, both activating the title-menu
			switch (wParam) {
				case VK_RETURN:
				case 'F': // Full Screen on ALT + ENTER/F
					ToggleFullScreen(!_wnd.fullscreen);
					return 0;

				case VK_MENU: // Just ALT
					return 0; // do nothing

				case VK_F10: // F10, ignore activation of menu
					HandleKeypress(MapWindowsKey(wParam), 0);
					return 0;

				default: // ALT in combination with something else
					HandleKeypress(MapWindowsKey(wParam), 0);
					break;
			}
			break;

		case WM_SIZE:
			if (wParam != SIZE_MINIMIZED) {
				/* Set maximized flag when we maximize (obviously), but also when we
				 * switched to fullscreen from a maximized state */
				_window_maximize = (wParam == SIZE_MAXIMIZED || (_window_maximize && _fullscreen));
				if (_window_maximize || _fullscreen) _bck_resolution = _cur_resolution;
				ClientSizeChanged(LOWORD(lParam), HIWORD(lParam));
			}
			return 0;

#if !defined(WINCE)
		case WM_SIZING: {
			RECT *r = (RECT*)lParam;
			RECT r2;
			int w, h;

			SetRect(&r2, 0, 0, 0, 0);
			AdjustWindowRect(&r2, GetWindowLong(hwnd, GWL_STYLE), FALSE);

			w = r->right - r->left - (r2.right - r2.left);
			h = r->bottom - r->top - (r2.bottom - r2.top);
			w = max(w, 64);
			h = max(h, 64);
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

/* needed for wheel */
#if !defined(WM_MOUSEWHEEL)
# define WM_MOUSEWHEEL 0x020A
#endif  /* WM_MOUSEWHEEL */
#if !defined(GET_WHEEL_DELTA_WPARAM)
# define GET_WHEEL_DELTA_WPARAM(wparam) ((short)HIWORD(wparam))
#endif  /* GET_WHEEL_DELTA_WPARAM */

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

		case WM_SETFOCUS:
			_wnd.has_focus = true;
			SetCompositionPos(hwnd);
			break;

		case WM_KILLFOCUS:
			_wnd.has_focus = false;
			break;

#if !defined(WINCE)
		case WM_ACTIVATE: {
			/* Don't do anything if we are closing openttd */
			if (_exit_game) break;

			bool active = (LOWORD(wParam) != WA_INACTIVE);
			bool minimized = (HIWORD(wParam) != 0);
			if (_wnd.fullscreen) {
				if (active && minimized) {
					/* Restore the game window */
					ShowWindow(hwnd, SW_RESTORE);
					static_cast<VideoDriver_Win32 *>(VideoDriver::GetInstance())->MakeWindow(true);
				} else if (!active && !minimized) {
					/* Minimise the window and restore desktop */
					ShowWindow(hwnd, SW_MINIMIZE);
					ChangeDisplaySettings(NULL, 0);
				}
			}
			break;
		}
#endif
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

static void RegisterWndClass()
{
	static bool registered = false;

	if (!registered) {
		HINSTANCE hinst = GetModuleHandle(NULL);
		WNDCLASS wnd = {
			CS_OWNDC,
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
		if (!RegisterClass(&wnd)) usererror("RegisterClass failed");
	}
}

static bool AllocateDibSection(int w, int h, bool force)
{
	BITMAPINFO *bi;
	HDC dc;
	uint bpp = BlitterFactory::GetCurrentBlitter()->GetScreenDepth();

	w = max(w, 64);
	h = max(h, 64);

	if (bpp == 0) usererror("Can't use a blitter that blits 0 bpp for normal visuals");

	if (!force && w == _screen.width && h == _screen.height) return false;

	bi = (BITMAPINFO*)alloca(sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * 256);
	memset(bi, 0, sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * 256);
	bi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);

	bi->bmiHeader.biWidth = _wnd.width = w;
	bi->bmiHeader.biHeight = -(_wnd.height = h);

	bi->bmiHeader.biPlanes = 1;
	bi->bmiHeader.biBitCount = BlitterFactory::GetCurrentBlitter()->GetScreenDepth();
	bi->bmiHeader.biCompression = BI_RGB;

	if (_wnd.dib_sect) DeleteObject(_wnd.dib_sect);

	dc = GetDC(0);
	_wnd.dib_sect = CreateDIBSection(dc, bi, DIB_RGB_COLORS, (VOID**)&_wnd.buffer_bits, NULL, 0);
	if (_wnd.dib_sect == NULL) usererror("CreateDIBSection failed");
	ReleaseDC(0, dc);

	_screen.width = w;
	_screen.pitch = (bpp == 8) ? Align(w, 4) : w;
	_screen.height = h;
	_screen.dst_ptr = _wnd.buffer_bits;

	return true;
}

static const Dimension default_resolutions[] = {
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
	/* EnumDisplaySettingsW is only supported in CE 4.2+
	 * XXX -- One might argue that we assume 4.2+ on every system. Then we can use this function safely */
#else
	uint i;
	DEVMODEA dm;

	/* Check modes for the relevant fullscreen bpp */
	uint bpp = _support8bpp != S8BPP_HARDWARE ? 32 : BlitterFactory::GetCurrentBlitter()->GetScreenDepth();

	/* XXX - EnumDisplaySettingsW crashes with unicows.dll on Windows95
	 * Doesn't really matter since we don't pass a string anyways, but still
	 * a letdown */
	for (i = 0; EnumDisplaySettingsA(NULL, i, &dm) != 0; i++) {
		if (dm.dmBitsPerPel == bpp &&
				dm.dmPelsWidth >= 640 && dm.dmPelsHeight >= 480) {
			uint j;

			for (j = 0; j < n; j++) {
				if (_resolutions[j].width == dm.dmPelsWidth && _resolutions[j].height == dm.dmPelsHeight) break;
			}

			/* In the previous loop we have checked already existing/added resolutions if
			 * they are the same as the new ones. If this is not the case (j == n); we have
			 * looped all and found none, add the new one to the list. If we have reached the
			 * maximum amount of resolutions, then quit querying the display */
			if (j == n) {
				_resolutions[j].width  = dm.dmPelsWidth;
				_resolutions[j].height = dm.dmPelsHeight;
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

static FVideoDriver_Win32 iFVideoDriver_Win32;

const char *VideoDriver_Win32::Start(const char * const *parm)
{
	memset(&_wnd, 0, sizeof(_wnd));

	RegisterWndClass();

	MakePalette();

	FindResolutions();

	DEBUG(driver, 2, "Resolution for display: %ux%u", _cur_resolution.width, _cur_resolution.height);

	/* fullscreen uses those */
	_wnd.width_org  = _cur_resolution.width;
	_wnd.height_org = _cur_resolution.height;

	AllocateDibSection(_cur_resolution.width, _cur_resolution.height);
	this->MakeWindow(_fullscreen);

	MarkWholeScreenDirty();

	_draw_threaded = GetDriverParam(parm, "no_threads") == NULL && GetDriverParam(parm, "no_thread") == NULL && GetCPUCoreCount() > 1;

	return NULL;
}

void VideoDriver_Win32::Stop()
{
	DeleteObject(_wnd.gdi_palette);
	DeleteObject(_wnd.dib_sect);
	DestroyWindow(_wnd.main_wnd);

#if !defined(WINCE)
	if (_wnd.fullscreen) ChangeDisplaySettings(NULL, 0);
#endif
	MyShowCursor(true);
}

void VideoDriver_Win32::MakeDirty(int left, int top, int width, int height)
{
	RECT r = { left, top, left + width, top + height };

	InvalidateRect(_wnd.main_wnd, &r, FALSE);
}

static void CheckPaletteAnim()
{
	if (_cur_palette.count_dirty == 0) return;

	_local_palette = _cur_palette;
	InvalidateRect(_wnd.main_wnd, NULL, FALSE);
}

void VideoDriver_Win32::MainLoop()
{
	MSG mesg;
	uint32 cur_ticks = GetTickCount();
	uint32 last_cur_ticks = cur_ticks;
	uint32 next_tick = cur_ticks + MILLISECONDS_PER_TICK;

	if (_draw_threaded) {
		/* Initialise the mutex first, because that's the thing we *need*
		 * directly in the newly created thread. */
		_draw_mutex = ThreadMutex::New();
		_draw_thread_initialized = CreateEvent(NULL, FALSE, FALSE, NULL);
		if (_draw_mutex == NULL || _draw_thread_initialized == NULL) {
			_draw_threaded = false;
		} else {
			_draw_continue = true;
			_draw_threaded = ThreadObject::New(&PaintWindowThread, NULL, &_draw_thread, "ottd:draw-win32");

			/* Free the mutex if we won't be able to use it. */
			if (!_draw_threaded) {
				delete _draw_mutex;
				_draw_mutex = NULL;
				CloseHandle(_draw_thread_initialized);
				_draw_thread_initialized = NULL;
			} else {
				DEBUG(driver, 1, "Threaded drawing enabled");
				/* Wait till the draw thread has started itself. */
				WaitForSingleObject(_draw_thread_initialized, INFINITE);
				_draw_mutex->BeginCritical();
			}
		}
	}

	_wnd.running = true;

	CheckPaletteAnim();
	for (;;) {
		uint32 prev_cur_ticks = cur_ticks; // to check for wrapping

		while (PeekMessage(&mesg, NULL, 0, 0, PM_REMOVE)) {
			InteractiveRandom(); // randomness
			/* Convert key messages to char messages if we want text input. */
			if (EditBoxInGlobalFocus()) TranslateMessage(&mesg);
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
		if (cur_ticks >= next_tick || (_fast_forward && !_pause_mode) || cur_ticks < prev_cur_ticks) {
			_realtime_tick += cur_ticks - last_cur_ticks;
			last_cur_ticks = cur_ticks;
			next_tick = cur_ticks + MILLISECONDS_PER_TICK;

			bool old_ctrl_pressed = _ctrl_pressed;

			_ctrl_pressed = _wnd.has_focus && GetAsyncKeyState(VK_CONTROL)<0;
			_shift_pressed = _wnd.has_focus && GetAsyncKeyState(VK_SHIFT)<0;

			/* determine which directional keys are down */
			if (_wnd.has_focus) {
				_dirkeys =
					(GetAsyncKeyState(VK_LEFT) < 0 ? 1 : 0) +
					(GetAsyncKeyState(VK_UP) < 0 ? 2 : 0) +
					(GetAsyncKeyState(VK_RIGHT) < 0 ? 4 : 0) +
					(GetAsyncKeyState(VK_DOWN) < 0 ? 8 : 0);
			} else {
				_dirkeys = 0;
			}

			if (old_ctrl_pressed != _ctrl_pressed) HandleCtrlChanged();

#if !defined(WINCE)
			/* Flush GDI buffer to ensure we don't conflict with the drawing thread. */
			GdiFlush();
#endif

			/* The game loop is the part that can run asynchronously.
			 * The rest except sleeping can't. */
			if (_draw_threaded) _draw_mutex->EndCritical();
			GameLoop();
			if (_draw_threaded) _draw_mutex->BeginCritical();

			if (_force_full_redraw) MarkWholeScreenDirty();

			UpdateWindows();
			CheckPaletteAnim();
		} else {
#if !defined(WINCE)
			/* Flush GDI buffer to ensure we don't conflict with the drawing thread. */
			GdiFlush();
#endif

			/* Release the thread while sleeping */
			if (_draw_threaded) _draw_mutex->EndCritical();
			Sleep(1);
			if (_draw_threaded) _draw_mutex->BeginCritical();

			NetworkDrawChatMessage();
			DrawMouseCursor();
		}
	}

	if (_draw_threaded) {
		_draw_continue = false;
		/* Sending signal if there is no thread blocked
		 * is very valid and results in noop */
		_draw_mutex->SendSignal();
		_draw_mutex->EndCritical();
		_draw_thread->Join();

		CloseHandle(_draw_thread_initialized);
		delete _draw_mutex;
		delete _draw_thread;
	}
}

bool VideoDriver_Win32::ChangeResolution(int w, int h)
{
	if (_draw_mutex != NULL) _draw_mutex->BeginCritical(true);
	if (_window_maximize) ShowWindow(_wnd.main_wnd, SW_SHOWNORMAL);

	_wnd.width = _wnd.width_org = w;
	_wnd.height = _wnd.height_org = h;

	bool ret = this->MakeWindow(_fullscreen); // _wnd.fullscreen screws up ingame resolution switching
	if (_draw_mutex != NULL) _draw_mutex->EndCritical(true);
	return ret;
}

bool VideoDriver_Win32::ToggleFullscreen(bool full_screen)
{
	if (_draw_mutex != NULL) _draw_mutex->BeginCritical(true);
	bool ret = this->MakeWindow(full_screen);
	if (_draw_mutex != NULL) _draw_mutex->EndCritical(true);
	return ret;
}

bool VideoDriver_Win32::AfterBlitterChange()
{
	return AllocateDibSection(_screen.width, _screen.height, true) && this->MakeWindow(_fullscreen);
}

void VideoDriver_Win32::AcquireBlitterLock()
{
	if (_draw_mutex != NULL) _draw_mutex->BeginCritical(true);
}

void VideoDriver_Win32::ReleaseBlitterLock()
{
	if (_draw_mutex != NULL) _draw_mutex->EndCritical(true);
}

void VideoDriver_Win32::EditBoxLostFocus()
{
	if (_draw_mutex != NULL) _draw_mutex->BeginCritical(true);
	CancelIMEComposition(_wnd.main_wnd);
	SetCompositionPos(_wnd.main_wnd);
	SetCandidatePos(_wnd.main_wnd);
	if (_draw_mutex != NULL) _draw_mutex->EndCritical(true);
}
