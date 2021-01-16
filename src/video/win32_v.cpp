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
#include "../core/geometry_func.hpp"
#include "../core/math_func.hpp"
#include "../core/random_func.hpp"
#include "../texteff.hpp"
#include "../thread.h"
#include "../progress.h"
#include "../window_gui.h"
#include "../window_func.h"
#include "../framerate_type.h"
#include "win32_v.h"
#include <windows.h>
#include <imm.h>
#include <mutex>
#include <condition_variable>

#include "../safeguards.h"

/* Missing define in MinGW headers. */
#ifndef MAPVK_VK_TO_CHAR
#define MAPVK_VK_TO_CHAR    (2)
#endif

#ifndef PM_QS_INPUT
#define PM_QS_INPUT 0x20000
#endif

static struct {
	int width;            ///< Width in pixels of our display surface.
	int height;           ///< Height in pixels of our display surface.
	int width_org;        ///< Original monitor resolution width, before we changed it.
	int height_org;       ///< Original monitor resolution height, before we changed it.
	bool has_focus;       ///< Does our window have system focus?
	bool running;         ///< Is the main loop running?
} _wnd;

bool _window_maximize;
static Dimension _bck_resolution;
DWORD _imm_props;

/** Whether the drawing is/may be done in a separate thread. */
static bool _draw_threaded;
/** Mutex to keep the access to the shared memory controlled. */
static std::recursive_mutex *_draw_mutex = nullptr;
/** Signal to draw the next frame. */
static std::condition_variable_any *_draw_signal = nullptr;
/** Should we keep continue drawing? */
static volatile bool _draw_continue;
/** Local copy of the palette for use in the drawing thread. */
static Palette _local_palette;

bool VideoDriver_Win32Base::ClaimMousePointer()
{
	MyShowCursor(false, true);
	return true;
}

struct Win32VkMapping {
	byte vk_from;
	byte vk_count;
	byte map_to;
};

#define AS(x, z) {x, 0, z}
#define AM(x, y, z, w) {x, y - x, z}

static const Win32VkMapping _vk_mapping[] = {
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
	const Win32VkMapping *map;
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

/** Colour depth to use for fullscreen display modes. */
uint8 VideoDriver_Win32Base::GetFullscreenBpp()
{
	/* Check modes for the relevant fullscreen bpp */
	return _support8bpp != S8BPP_HARDWARE ? 32 : BlitterFactory::GetCurrentBlitter()->GetScreenDepth();
}

/**
 * Instantiate a new window.
 * @param full_screen Whether to make a full screen window or not.
 * @return True if the window could be created.
 */
bool VideoDriver_Win32Base::MakeWindow(bool full_screen)
{
	/* full_screen is whether the new window should be fullscreen,
	 * _wnd.fullscreen is whether the current window is. */
	_fullscreen = full_screen;

	/* recreate window? */
	if ((full_screen || this->fullscreen) && this->main_wnd) {
		DestroyWindow(this->main_wnd);
		this->main_wnd = 0;
	}

	if (full_screen) {
		DEVMODE settings;

		memset(&settings, 0, sizeof(settings));
		settings.dmSize = sizeof(settings);
		settings.dmFields =
			DM_BITSPERPEL |
			DM_PELSWIDTH |
			DM_PELSHEIGHT;
		settings.dmBitsPerPel = this->GetFullscreenBpp();
		settings.dmPelsWidth  = _wnd.width_org;
		settings.dmPelsHeight = _wnd.height_org;

		/* Check for 8 bpp support. */
		if (settings.dmBitsPerPel == 8 && ChangeDisplaySettings(&settings, CDS_FULLSCREEN | CDS_TEST) != DISP_CHANGE_SUCCESSFUL) {
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
	} else if (this->fullscreen) {
		/* restore display? */
		ChangeDisplaySettings(nullptr, 0);
		/* restore the resolution */
		_wnd.width = _bck_resolution.width;
		_wnd.height = _bck_resolution.height;
	}

	{
		RECT r;
		DWORD style, showstyle;
		int w, h;

		showstyle = SW_SHOWNORMAL;
		this->fullscreen = full_screen;
		if (this->fullscreen) {
			style = WS_POPUP;
			SetRect(&r, 0, 0, _wnd.width_org, _wnd.height_org);
		} else {
			style = WS_OVERLAPPEDWINDOW;
			/* On window creation, check if we were in maximize mode before */
			if (_window_maximize) showstyle = SW_SHOWMAXIMIZED;
			SetRect(&r, 0, 0, _wnd.width, _wnd.height);
		}

		AdjustWindowRect(&r, style, FALSE);
		w = r.right - r.left;
		h = r.bottom - r.top;

		if (this->main_wnd != nullptr) {
			if (!_window_maximize) SetWindowPos(this->main_wnd, 0, 0, 0, w, h, SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_NOMOVE);
		} else {
			int x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
			int y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;

			char window_title[64];
			seprintf(window_title, lastof(window_title), "OpenTTD %s", _openttd_revision);

			this->main_wnd = CreateWindow(_T("OTTD"), MB_TO_WIDE(window_title), style, x, y, w, h, 0, 0, GetModuleHandle(nullptr), this);
			if (this->main_wnd == nullptr) usererror("CreateWindow failed");
			ShowWindow(this->main_wnd, showstyle);
		}
	}

	BlitterFactory::GetCurrentBlitter()->PostResize();

	GameSizeChanged();
	return true;
}

/* static */ void VideoDriver_Win32Base::PaintThreadThunk(VideoDriver_Win32Base *drv)
{
	drv->PaintThread();
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
			LONG len = ImmGetCompositionString(hIMC, GCS_RESULTSTR, nullptr, 0); // Length is always in bytes, even in UNICODE build.
			TCHAR *str = (TCHAR *)_alloca(len + sizeof(TCHAR));
			len = ImmGetCompositionString(hIMC, GCS_RESULTSTR, str, len);
			str[len / sizeof(TCHAR)] = '\0';

			/* Transmit text to windowing system. */
			if (len > 0) {
				HandleTextInput(nullptr, true); // Clear marked string.
				HandleTextInput(FS2OTTD(str));
			}
			SetCompositionPos(hwnd);

			/* Don't pass the result string on to the default window proc. */
			lParam &= ~(GCS_RESULTSTR | GCS_RESULTCLAUSE | GCS_RESULTREADCLAUSE | GCS_RESULTREADSTR);
		}

		if ((lParam & GCS_COMPSTR) && DrawIMECompositionString()) {
			/* Read composition string from the IME. */
			LONG len = ImmGetCompositionString(hIMC, GCS_COMPSTR, nullptr, 0); // Length is always in bytes, even in UNICODE build.
			TCHAR *str = (TCHAR *)_alloca(len + sizeof(TCHAR));
			len = ImmGetCompositionString(hIMC, GCS_COMPSTR, str, len);
			str[len / sizeof(TCHAR)] = '\0';

			if (len > 0) {
				static char utf8_buf[1024];
				convert_from_fs(str, utf8_buf, lengthof(utf8_buf));

				/* Convert caret position from bytes in the input string to a position in the UTF-8 encoded string. */
				LONG caret_bytes = ImmGetCompositionString(hIMC, GCS_CURSORPOS, nullptr, 0);
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
				HandleTextInput(nullptr, true);
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
	HandleTextInput(nullptr, true);
}

LRESULT CALLBACK WndProcGdi(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static uint32 keycode = 0;
	static bool console = false;

	VideoDriver_Win32Base *video_driver = (VideoDriver_Win32Base *)GetWindowLongPtr(hwnd, GWLP_USERDATA);

	switch (msg) {
		case WM_CREATE:
			SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)((LPCREATESTRUCT)lParam)->lpCreateParams);
			_cursor.in_window = false; // Win32 has mouse tracking.
			SetCompositionPos(hwnd);
			_imm_props = ImmGetProperty(GetKeyboardLayout(0), IGP_PROPERTY);
			break;

		case WM_PAINT: {
			RECT r;
			GetUpdateRect(hwnd, &r, FALSE);
			video_driver->MakeDirty(r.left, r.top, r.right - r.left, r.bottom - r.top);

			ValidateRect(hwnd, nullptr);
			return 0;
		}

		case WM_PALETTECHANGED:
			if ((HWND)wParam == hwnd) return 0;
			FALLTHROUGH;

		case WM_QUERYNEWPALETTE:
			video_driver->PaletteChanged(hwnd);
			return 0;

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

			/* If the mouse was not in the window and it has moved it means it has
			 * come into the window, so start drawing the mouse. Also start
			 * tracking the mouse for exiting the window */
			if (!_cursor.in_window) {
				_cursor.in_window = true;
				TRACKMOUSEEVENT tme;
				tme.cbSize = sizeof(tme);
				tme.dwFlags = TME_LEAVE;
				tme.hwndTrack = hwnd;

				TrackMouseEvent(&tme);
			}

			if (_cursor.fix_at) {
				/* Get all queued mouse events now in case we have to warp the cursor. In the
				 * end, we only care about the current mouse position and not bygone events. */
				MSG m;
				while (PeekMessage(&m, hwnd, WM_MOUSEMOVE, WM_MOUSEMOVE, PM_REMOVE | PM_NOYIELD | PM_QS_INPUT)) {
					x = (int16)LOWORD(m.lParam);
					y = (int16)HIWORD(m.lParam);
				}
			}

			if (_cursor.UpdateCursorPosition(x, y, false)) {
				POINT pt;
				pt.x = _cursor.pos.x;
				pt.y = _cursor.pos.y;
				ClientToScreen(hwnd, &pt);
				SetCursorPos(pt.x, pt.y);
			}
			MyShowCursor(false);
			HandleMouseEvents();
			return 0;
		}

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
			HandleTextInput(nullptr, true);
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
			if (PeekMessage(&msg, nullptr, 0, 0, PM_NOREMOVE)) {
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
					ToggleFullScreen(!video_driver->fullscreen);
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
				video_driver->ClientSizeChanged(LOWORD(lParam), HIWORD(lParam));
			}
			return 0;

		case WM_SIZING: {
			RECT *r = (RECT*)lParam;
			RECT r2;
			int w, h;

			SetRect(&r2, 0, 0, 0, 0);
			AdjustWindowRect(&r2, GetWindowLong(hwnd, GWL_STYLE), FALSE);

			w = r->right - r->left - (r2.right - r2.left);
			h = r->bottom - r->top - (r2.bottom - r2.top);
			w = std::max(w, 64);
			h = std::max(h, 64);
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

		case WM_ACTIVATE: {
			/* Don't do anything if we are closing openttd */
			if (_exit_game) break;

			bool active = (LOWORD(wParam) != WA_INACTIVE);
			bool minimized = (HIWORD(wParam) != 0);
			if (video_driver->fullscreen) {
				if (active && minimized) {
					/* Restore the game window */
					ShowWindow(hwnd, SW_RESTORE);
					video_driver->MakeWindow(true);
				} else if (!active && !minimized) {
					/* Minimise the window and restore desktop */
					ShowWindow(hwnd, SW_MINIMIZE);
					ChangeDisplaySettings(nullptr, 0);
				}
			}
			break;
		}
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

static void RegisterWndClass()
{
	static bool registered = false;

	if (registered) return;

	HINSTANCE hinst = GetModuleHandle(nullptr);
	WNDCLASS wnd = {
		CS_OWNDC,
		WndProcGdi,
		0,
		0,
		hinst,
		LoadIcon(hinst, MAKEINTRESOURCE(100)),
		LoadCursor(nullptr, IDC_ARROW),
		0,
		0,
		_T("OTTD")
	};

	registered = true;
	if (!RegisterClass(&wnd)) usererror("RegisterClass failed");
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

static void FindResolutions(uint8 bpp)
{
	_resolutions.clear();

	DEVMODE dm;
	for (uint i = 0; EnumDisplaySettings(nullptr, i, &dm) != 0; i++) {
		if (dm.dmBitsPerPel != bpp || dm.dmPelsWidth < 640 || dm.dmPelsHeight < 480) continue;
		if (std::find(_resolutions.begin(), _resolutions.end(), Dimension(dm.dmPelsWidth, dm.dmPelsHeight)) != _resolutions.end()) continue;
		_resolutions.emplace_back(dm.dmPelsWidth, dm.dmPelsHeight);
	}

	/* We have found no resolutions, show the default list */
	if (_resolutions.empty()) {
		_resolutions.assign(std::begin(default_resolutions), std::end(default_resolutions));
	}

	SortResolutions();
}

void VideoDriver_Win32Base::Initialize()
{
	this->UpdateAutoResolution();

	memset(&_wnd, 0, sizeof(_wnd));

	RegisterWndClass();
	FindResolutions(this->GetFullscreenBpp());

	/* fullscreen uses those */
	_wnd.width  = _wnd.width_org  = _cur_resolution.width;
	_wnd.height = _wnd.height_org = _cur_resolution.height;

	DEBUG(driver, 2, "Resolution for display: %ux%u", _cur_resolution.width, _cur_resolution.height);
}

void VideoDriver_Win32Base::Stop()
{
	DestroyWindow(this->main_wnd);

	if (this->fullscreen) ChangeDisplaySettings(nullptr, 0);
	MyShowCursor(true);
}
void VideoDriver_Win32Base::MakeDirty(int left, int top, int width, int height)
{
	Rect r = {left, top, left + width, top + height};
	this->dirty_rect = BoundingRect(this->dirty_rect, r);
}

void VideoDriver_Win32Base::CheckPaletteAnim()
{
	if (_cur_palette.count_dirty == 0) return;

	_local_palette = _cur_palette;
	this->MakeDirty(0, 0, _screen.width, _screen.height);
}

void VideoDriver_Win32Base::InputLoop()
{
	bool old_ctrl_pressed = _ctrl_pressed;

	_ctrl_pressed = _wnd.has_focus && GetAsyncKeyState(VK_CONTROL) < 0;
	_shift_pressed = _wnd.has_focus && GetAsyncKeyState(VK_SHIFT) < 0;

#if defined(_DEBUG)
	if (_shift_pressed)
#else
	/* Speedup when pressing tab, except when using ALT+TAB
	 * to switch to another application. */
	if (_wnd.has_focus && GetAsyncKeyState(VK_TAB) < 0 && GetAsyncKeyState(VK_MENU) >= 0)
#endif
	{
		if (!_networking && _game_mode != GM_MENU) _fast_forward |= 2;
	} else if (_fast_forward & 2) {
		_fast_forward = 0;
	}

	/* Determine which directional keys are down. */
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
}

void VideoDriver_Win32Base::MainLoop()
{
	MSG mesg;

	std::thread draw_thread;

	if (_draw_threaded) {
		/* Initialise the mutex first, because that's the thing we *need*
		 * directly in the newly created thread. */
		try {
			_draw_signal = new std::condition_variable_any();
			_draw_mutex = new std::recursive_mutex();
		} catch (...) {
			_draw_threaded = false;
		}

		if (_draw_threaded) {
			this->draw_lock = std::unique_lock<std::recursive_mutex>(*_draw_mutex);

			_draw_continue = true;
			_draw_threaded = StartNewThread(&draw_thread, "ottd:draw-win32", &VideoDriver_Win32Base::PaintThreadThunk, this);

			/* Free the mutex if we won't be able to use it. */
			if (!_draw_threaded) {
				this->draw_lock.unlock();
				this->draw_lock.release();
				delete _draw_mutex;
				delete _draw_signal;
				_draw_mutex = nullptr;
				_draw_signal = nullptr;
			} else {
				DEBUG(driver, 1, "Threaded drawing enabled");
				/* Wait till the draw thread has started itself. */
				_draw_signal->wait(*_draw_mutex);
			}
		}
	}

	_wnd.running = true;

	for (;;) {
		InteractiveRandom(); // randomness

		while (PeekMessage(&mesg, nullptr, 0, 0, PM_REMOVE)) {
			/* Convert key messages to char messages if we want text input. */
			if (EditBoxInGlobalFocus()) TranslateMessage(&mesg);
			DispatchMessage(&mesg);
		}
		if (_exit_game) break;

		/* Flush GDI buffer to ensure we don't conflict with the drawing thread. */
		GdiFlush();

		if (this->Tick()) {
			if (_draw_mutex != nullptr && !HasModalProgress()) {
				_draw_signal->notify_one();
			} else {
				this->Paint();
			}
		}
		this->SleepTillNextTick();
	}

	if (_draw_threaded) {
		_draw_continue = false;
		/* Sending signal if there is no thread blocked
		 * is very valid and results in noop */
		_draw_signal->notify_all();
		if (this->draw_lock.owns_lock()) this->draw_lock.unlock();
		this->draw_lock.release();
		draw_thread.join();

		delete _draw_mutex;
		delete _draw_signal;

		_draw_mutex = nullptr;
	}
}

void VideoDriver_Win32Base::ClientSizeChanged(int w, int h)
{
	/* Allocate backing store of the new size. */
	if (this->AllocateBackingStore(w, h)) {
		/* Mark all palette colours dirty. */
		_cur_palette.first_dirty = 0;
		_cur_palette.count_dirty = 256;
		_local_palette = _cur_palette;

		BlitterFactory::GetCurrentBlitter()->PostResize();

		GameSizeChanged();
	}
}

bool VideoDriver_Win32Base::ChangeResolution(int w, int h)
{
	std::unique_lock<std::recursive_mutex> lock;
	if (_draw_mutex != nullptr) lock = std::unique_lock<std::recursive_mutex>(*_draw_mutex);

	if (_window_maximize) ShowWindow(this->main_wnd, SW_SHOWNORMAL);

	_wnd.width = _wnd.width_org = w;
	_wnd.height = _wnd.height_org = h;

	return this->MakeWindow(_fullscreen); // _wnd.fullscreen screws up ingame resolution switching
}

bool VideoDriver_Win32Base::ToggleFullscreen(bool full_screen)
{
	std::unique_lock<std::recursive_mutex> lock;
	if (_draw_mutex != nullptr) lock = std::unique_lock<std::recursive_mutex>(*_draw_mutex);

	return this->MakeWindow(full_screen);
}

void VideoDriver_Win32Base::AcquireBlitterLock()
{
	if (_draw_mutex != nullptr) _draw_mutex->lock();
}

void VideoDriver_Win32Base::ReleaseBlitterLock()
{
	if (_draw_mutex != nullptr) _draw_mutex->unlock();
}

void VideoDriver_Win32Base::EditBoxLostFocus()
{
	std::unique_lock<std::recursive_mutex> lock;
	if (_draw_mutex != nullptr) lock = std::unique_lock<std::recursive_mutex>(*_draw_mutex);

	CancelIMEComposition(this->main_wnd);
	SetCompositionPos(this->main_wnd);
	SetCandidatePos(this->main_wnd);
}

Dimension VideoDriver_Win32Base::GetScreenSize() const
{
	return { static_cast<uint>(GetSystemMetrics(SM_CXSCREEN)), static_cast<uint>(GetSystemMetrics(SM_CYSCREEN)) };
}

float VideoDriver_Win32Base::GetDPIScale()
{
	typedef UINT (WINAPI *PFNGETDPIFORWINDOW)(HWND hwnd);
	typedef UINT (WINAPI *PFNGETDPIFORSYSTEM)(VOID);
	typedef HRESULT (WINAPI *PFNGETDPIFORMONITOR)(HMONITOR hMonitor, int dpiType, UINT *dpiX, UINT *dpiY);

	static PFNGETDPIFORWINDOW _GetDpiForWindow = nullptr;
	static PFNGETDPIFORSYSTEM _GetDpiForSystem = nullptr;
	static PFNGETDPIFORMONITOR _GetDpiForMonitor = nullptr;

	static bool init_done = false;
	if (!init_done) {
		init_done = true;

		_GetDpiForWindow = (PFNGETDPIFORWINDOW)GetProcAddress(GetModuleHandle(_T("User32")), "GetDpiForWindow");
		_GetDpiForSystem = (PFNGETDPIFORSYSTEM)GetProcAddress(GetModuleHandle(_T("User32")), "GetDpiForSystem");
		_GetDpiForMonitor = (PFNGETDPIFORMONITOR)GetProcAddress(LoadLibrary(_T("Shcore.dll")), "GetDpiForMonitor");
	}

	UINT cur_dpi = 0;

	if (cur_dpi == 0 && _GetDpiForWindow != nullptr && this->main_wnd != nullptr) {
		/* Per window DPI is supported since Windows 10 Ver 1607. */
		cur_dpi = _GetDpiForWindow(this->main_wnd);
	}
	if (cur_dpi == 0 && _GetDpiForMonitor != nullptr && this->main_wnd != nullptr) {
		/* Per monitor is supported since Windows 8.1. */
		UINT dpiX, dpiY;
		if (SUCCEEDED(_GetDpiForMonitor(MonitorFromWindow(this->main_wnd, MONITOR_DEFAULTTOPRIMARY), 0 /* MDT_EFFECTIVE_DPI */, &dpiX, &dpiY))) {
			cur_dpi = dpiX; // X and Y are always identical.
		}
	}
	if (cur_dpi == 0 && _GetDpiForSystem != nullptr) {
		/* Fall back to system DPI. */
		cur_dpi = _GetDpiForSystem();
	}

	return cur_dpi > 0 ? cur_dpi / 96.0f : 1.0f; // Default Windows DPI value is 96.
}

bool VideoDriver_Win32Base::LockVideoBuffer()
{
	if (_draw_threaded) this->draw_lock.lock();

	_screen.dst_ptr = this->GetVideoPointer();

	return true;
}

void VideoDriver_Win32Base::UnlockVideoBuffer()
{
	if (_draw_threaded) this->draw_lock.unlock();
}


static FVideoDriver_Win32GDI iFVideoDriver_Win32GDI;

const char *VideoDriver_Win32GDI::Start(const StringList &param)
{
	if (BlitterFactory::GetCurrentBlitter()->GetScreenDepth() == 0) return "Only real blitters supported";

	this->Initialize();

	this->MakePalette();
	this->AllocateBackingStore(_cur_resolution.width, _cur_resolution.height);
	this->MakeWindow(_fullscreen);

	MarkWholeScreenDirty();

	_draw_threaded = !GetDriverParam(param, "no_threads") && !GetDriverParam(param, "no_thread") && std::thread::hardware_concurrency() > 1;

	return nullptr;
}

void VideoDriver_Win32GDI::Stop()
{
	DeleteObject(this->gdi_palette);
	DeleteObject(this->dib_sect);

	this->VideoDriver_Win32Base::Stop();
}

bool VideoDriver_Win32GDI::AllocateBackingStore(int w, int h, bool force)
{
	uint bpp = BlitterFactory::GetCurrentBlitter()->GetScreenDepth();

	w = std::max(w, 64);
	h = std::max(h, 64);

	if (!force && w == _screen.width && h == _screen.height) return false;

	BITMAPINFO *bi = (BITMAPINFO *)alloca(sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * 256);
	memset(bi, 0, sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * 256);
	bi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);

	bi->bmiHeader.biWidth = _wnd.width = w;
	bi->bmiHeader.biHeight = -(_wnd.height = h);

	bi->bmiHeader.biPlanes = 1;
	bi->bmiHeader.biBitCount = bpp;
	bi->bmiHeader.biCompression = BI_RGB;

	if (this->dib_sect) DeleteObject(this->dib_sect);

	HDC dc = GetDC(0);
	this->dib_sect = CreateDIBSection(dc, bi, DIB_RGB_COLORS, (VOID **)&this->buffer_bits, nullptr, 0);
	if (this->dib_sect == nullptr) usererror("CreateDIBSection failed");
	ReleaseDC(0, dc);

	_screen.width = w;
	_screen.pitch = (bpp == 8) ? Align(w, 4) : w;
	_screen.height = h;
	_screen.dst_ptr = this->GetVideoPointer();

	return true;
}

bool VideoDriver_Win32GDI::AfterBlitterChange()
{
	assert(BlitterFactory::GetCurrentBlitter()->GetScreenDepth() != 0);
	return this->AllocateBackingStore(_screen.width, _screen.height, true) && this->MakeWindow(_fullscreen);
}

void VideoDriver_Win32GDI::MakePalette()
{
	_cur_palette.first_dirty = 0;
	_cur_palette.count_dirty = 256;
	_local_palette = _cur_palette;

	LOGPALETTE *pal = (LOGPALETTE*)alloca(sizeof(LOGPALETTE) + (256 - 1) * sizeof(PALETTEENTRY));

	pal->palVersion = 0x300;
	pal->palNumEntries = 256;

	for (uint i = 0; i != 256; i++) {
		pal->palPalEntry[i].peRed   = _local_palette.palette[i].r;
		pal->palPalEntry[i].peGreen = _local_palette.palette[i].g;
		pal->palPalEntry[i].peBlue  = _local_palette.palette[i].b;
		pal->palPalEntry[i].peFlags = 0;

	}
	this->gdi_palette = CreatePalette(pal);
	if (this->gdi_palette == nullptr) usererror("CreatePalette failed!\n");
}

void VideoDriver_Win32GDI::UpdatePalette(HDC dc, uint start, uint count)
{
	RGBQUAD rgb[256];

	for (uint i = 0; i != count; i++) {
		rgb[i].rgbRed   = _local_palette.palette[start + i].r;
		rgb[i].rgbGreen = _local_palette.palette[start + i].g;
		rgb[i].rgbBlue  = _local_palette.palette[start + i].b;
		rgb[i].rgbReserved = 0;
	}

	SetDIBColorTable(dc, start, count, rgb);
}

void VideoDriver_Win32GDI::PaletteChanged(HWND hWnd)
{
	HDC hDC = GetWindowDC(hWnd);
	HPALETTE hOldPalette = SelectPalette(hDC, this->gdi_palette, FALSE);
	UINT nChanged = RealizePalette(hDC);

	SelectPalette(hDC, hOldPalette, TRUE);
	ReleaseDC(hWnd, hDC);
	if (nChanged != 0) this->MakeDirty(0, 0, _screen.width, _screen.height);
}

void VideoDriver_Win32GDI::Paint()
{
	PerformanceMeasurer framerate(PFE_VIDEO);

	if (IsEmptyRect(this->dirty_rect)) return;

	HDC dc = GetDC(this->main_wnd);
	HDC dc2 = CreateCompatibleDC(dc);

	HBITMAP old_bmp = (HBITMAP)SelectObject(dc2, this->dib_sect);
	HPALETTE old_palette = SelectPalette(dc, this->gdi_palette, FALSE);

	if (_cur_palette.count_dirty != 0) {
		Blitter *blitter = BlitterFactory::GetCurrentBlitter();

		switch (blitter->UsePaletteAnimation()) {
			case Blitter::PALETTE_ANIMATION_VIDEO_BACKEND:
				this->UpdatePalette(dc2, _local_palette.first_dirty, _local_palette.count_dirty);
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

	ReleaseDC(this->main_wnd, dc);

	this->dirty_rect = {};
}

void VideoDriver_Win32GDI::PaintThread()
{
	/* First tell the main thread we're started */
	std::unique_lock<std::recursive_mutex> lock(*_draw_mutex);
	_draw_signal->notify_one();

	/* Now wait for the first thing to draw! */
	_draw_signal->wait(*_draw_mutex);

	while (_draw_continue) {
		this->Paint();

		/* Flush GDI buffer to ensure drawing here doesn't conflict with any GDI usage in the main WndProc. */
		GdiFlush();

		_draw_signal->wait(*_draw_mutex);
	}
}


#ifdef _DEBUG
/* Keep this function here..
 * It allows you to redraw the screen from within the MSVC debugger */
/* static */ int VideoDriver_Win32GDI::RedrawScreenDebug()
{
	static int _fooctr;

	VideoDriver_Win32GDI *drv = static_cast<VideoDriver_Win32GDI *>(VideoDriver::GetInstance());

	_screen.dst_ptr = drv->GetVideoPointer();
	UpdateWindows();

	drv->Paint();
	GdiFlush();

	return _fooctr++;
}
#endif

#ifdef WITH_OPENGL

#include <GL/gl.h>
#include "../3rdparty/opengl/glext.h"
#include "opengl.h"

#ifndef PFD_SUPPORT_COMPOSITION
#	define PFD_SUPPORT_COMPOSITION 0x00008000
#endif

/** Platform-specific callback to get an OpenGL funtion pointer. */
static OGLProc GetOGLProcAddressCallback(const char *proc)
{
	return reinterpret_cast<OGLProc>(wglGetProcAddress(proc));
}

static FVideoDriver_Win32OpenGL iFVideoDriver_Win32OpenGL;

const char *VideoDriver_Win32OpenGL::Start(const StringList &param)
{
	if (BlitterFactory::GetCurrentBlitter()->GetScreenDepth() != 32) return "Only 32bpp blitters supported";

	Dimension old_res = _cur_resolution; // Save current screen resolution in case of errors, as MakeWindow invalidates it.

	this->Initialize();
	this->MakeWindow(_fullscreen);

	/* Create and initialize OpenGL context. */
	const char *err = this->AllocateContext();
	if (err != nullptr) {
		this->Stop();
		_cur_resolution = old_res;
		return err;
	}

	this->ClientSizeChanged(_wnd.width, _wnd.height);

	_draw_threaded = false;
	MarkWholeScreenDirty();

	return nullptr;
}

void VideoDriver_Win32OpenGL::Stop()
{
	this->DestroyContext();
	this->VideoDriver_Win32Base::Stop();
}

void VideoDriver_Win32OpenGL::DestroyContext()
{
	OpenGLBackend::Destroy();

	wglMakeCurrent(nullptr, nullptr);
	if (this->gl_rc != nullptr) {
		wglDeleteContext(this->gl_rc);
		this->gl_rc = nullptr;
	}
	if (this->dc != nullptr) {
		ReleaseDC(this->main_wnd, this->dc);
		this->dc = nullptr;
	}
}

const char *VideoDriver_Win32OpenGL::AllocateContext()
{
	PIXELFORMATDESCRIPTOR pfd = {
		sizeof(PIXELFORMATDESCRIPTOR), // Size of this struct.
		1,                             // Version of this struct.
		PFD_DRAW_TO_WINDOW |           // Require window support.
		PFD_SUPPORT_OPENGL |           // Require OpenGL support.
		PFD_DOUBLEBUFFER   |           // Use double buffering.
		PFD_DEPTH_DONTCARE,
		PFD_TYPE_RGBA,                 // Request RGBA format.
		24,                            // 24 bpp (excluding alpha).
		0, 0, 0, 0, 0, 0, 0, 0,        // Colour bits and shift ignored.
		0, 0, 0, 0, 0,                 // No accumulation buffer.
		0, 0,                          // No depth/stencil buffer.
		0,                             // No aux buffers.
		PFD_MAIN_PLANE,                // Main layer.
		0, 0, 0, 0                     // Ignored/reserved.
	};

	if (IsWindowsVistaOrGreater()) pfd.dwFlags |= PFD_SUPPORT_COMPOSITION; // Make OpenTTD compatible with Aero.

	this->dc = GetDC(this->main_wnd);

	/* Choose a suitable pixel format. */
	int format = ChoosePixelFormat(this->dc, &pfd);
	if (format == 0) return "No suitable pixel format found";
	if (!SetPixelFormat(this->dc, format, &pfd)) return "Can't set pixel format";

	/* Create OpenGL device context. */
	this->gl_rc = wglCreateContext(this->dc);
	if (this->gl_rc == 0) return "Can't create OpenGL context";
	if (!wglMakeCurrent(this->dc, this->gl_rc)) return "Can't active GL context";

	return OpenGLBackend::Create(&GetOGLProcAddressCallback);
}

bool VideoDriver_Win32OpenGL::ToggleFullscreen(bool full_screen)
{
	this->DestroyContext();
	bool res = this->VideoDriver_Win32Base::ToggleFullscreen(full_screen);
	res &= this->AllocateContext() == nullptr;
	this->ClientSizeChanged(_wnd.width, _wnd.height);
	return res;
}

bool VideoDriver_Win32OpenGL::AfterBlitterChange()
{
	assert(BlitterFactory::GetCurrentBlitter()->GetScreenDepth() != 0);
	this->ClientSizeChanged(_wnd.width, _wnd.height);
	return true;
}

bool VideoDriver_Win32OpenGL::AllocateBackingStore(int w, int h, bool force)
{
	if (!force && w == _screen.width && h == _screen.height) return false;

	_wnd.width = w = std::max(w, 64);
	_wnd.height = h = std::max(h, 64);

	if (this->gl_rc == nullptr) return false;

	this->dirty_rect = {};
	return OpenGLBackend::Get()->Resize(w, h, force);
}

void *VideoDriver_Win32OpenGL::GetVideoPointer()
{
	return OpenGLBackend::Get()->GetVideoBuffer();
}

void VideoDriver_Win32OpenGL::Paint()
{
	PerformanceMeasurer framerate(PFE_VIDEO);

	if (IsEmptyRect(this->dirty_rect)) return;

	if (_cur_palette.count_dirty != 0) {
		Blitter *blitter = BlitterFactory::GetCurrentBlitter();

		switch (blitter->UsePaletteAnimation()) {
			case Blitter::PALETTE_ANIMATION_BLITTER:
				blitter->PaletteAnimate(_local_palette);
				break;

			case Blitter::PALETTE_ANIMATION_VIDEO_BACKEND:
			case Blitter::PALETTE_ANIMATION_NONE:
				break;

			default:
				NOT_REACHED();
		}
		_cur_palette.count_dirty = 0;
	}

	OpenGLBackend::Get()->Paint(this->dirty_rect);
	SwapBuffers(this->dc);

	this->dirty_rect = {};
}

#endif /* WITH_OPENGL */
