/* $Id$ */

/** @file console.cpp Handling of the in-game console. */

#include "stdafx.h"
#include "openttd.h"
#include "textbuf_gui.h"
#include "window_gui.h"
#include "console_gui.h"
#include <stdarg.h>
#include <string.h>
#include "console_internal.h"
#include "window_func.h"
#include "string_func.h"
#include "gfx_func.h"
#include "core/math_func.hpp"
#include "rev.h"

#include "table/strings.h"

#define ICON_BUFFER 79
#define ICON_HISTORY_SIZE 20
#define ICON_LINE_HEIGHT 12
#define ICON_RIGHT_BORDERWIDTH 10
#define ICON_BOTTOM_BORDERWIDTH 12
#define ICON_MAX_ALIAS_LINES 40
#define ICON_TOKEN_COUNT 20

/* console modes */
IConsoleModes _iconsole_mode;

/* ** main console ** */
static char *_iconsole_buffer[ICON_BUFFER + 1];
static uint16 _iconsole_cbuffer[ICON_BUFFER + 1];
static Textbuf _iconsole_cmdline;

/* ** main console cmd buffer ** */
static char *_iconsole_history[ICON_HISTORY_SIZE];
static byte _iconsole_historypos;

/* *************** *
 *  end of header  *
 * *************** */

static void IConsoleClearCommand()
{
	memset(_iconsole_cmdline.buf, 0, ICON_CMDLN_SIZE);
	_iconsole_cmdline.length = 0;
	_iconsole_cmdline.width = 0;
	_iconsole_cmdline.caretpos = 0;
	_iconsole_cmdline.caretxoffs = 0;
	SetWindowDirty(FindWindowById(WC_CONSOLE, 0));
}

static inline void IConsoleResetHistoryPos() {_iconsole_historypos = ICON_HISTORY_SIZE - 1;}


static void IConsoleHistoryAdd(const char *cmd);
static void IConsoleHistoryNavigate(int direction);

struct IConsoleWindow : Window
{
	static byte scroll;

	IConsoleWindow(const WindowDesc *desc) : Window(desc)
	{
		_iconsole_mode = ICONSOLE_OPENED;
		SetBit(_no_scroll, SCROLL_CON); // override cursor arrows; the gamefield will not scroll

		this->height = _screen.height / 3;
		this->width  = _screen.width;
	}

	~IConsoleWindow()
	{
		_iconsole_mode = ICONSOLE_CLOSED;
		ClrBit(_no_scroll, SCROLL_CON);
	}

	virtual void OnPaint()
	{
		int i = IConsoleWindow::scroll;
		int max = (this->height / ICON_LINE_HEIGHT) - 1;
		int delta = 0;
		GfxFillRect(this->left, this->top, this->width, this->height - 1, 0);
		while ((i > 0) && (i > IConsoleWindow::scroll - max) && (_iconsole_buffer[i] != NULL)) {
			DoDrawString(_iconsole_buffer[i], 5,
				this->height - (IConsoleWindow::scroll + 2 - i) * ICON_LINE_HEIGHT, _iconsole_cbuffer[i]);
			i--;
		}
		/* If the text is longer than the window, don't show the starting ']' */
		delta = this->width - 10 - _iconsole_cmdline.width - ICON_RIGHT_BORDERWIDTH;
		if (delta > 0) {
			DoDrawString("]", 5, this->height - ICON_LINE_HEIGHT, CC_COMMAND);
			delta = 0;
		}

		DoDrawString(_iconsole_cmdline.buf, 10 + delta, this->height - ICON_LINE_HEIGHT, CC_COMMAND);

		if (_iconsole_cmdline.caret) {
			DoDrawString("_", 10 + delta + _iconsole_cmdline.caretxoffs, this->height - ICON_LINE_HEIGHT, TC_WHITE);
		}
	}

	virtual void OnMouseLoop()
	{
		if (HandleCaret(&_iconsole_cmdline)) this->SetDirty();
	}

	virtual EventState OnKeyPress(uint16 key, uint16 keycode)
	{
		switch (keycode) {
			case WKC_UP:
				IConsoleHistoryNavigate(+1);
				this->SetDirty();
				break;

			case WKC_DOWN:
				IConsoleHistoryNavigate(-1);
				this->SetDirty();
				break;

			case WKC_SHIFT | WKC_PAGEUP:
				if (IConsoleWindow::scroll - (this->height / ICON_LINE_HEIGHT) - 1 < 0) {
					IConsoleWindow::scroll = 0;
				} else {
					IConsoleWindow::scroll -= (this->height / ICON_LINE_HEIGHT) - 1;
				}
				this->SetDirty();
				break;

			case WKC_SHIFT | WKC_PAGEDOWN:
				if (IConsoleWindow::scroll + (this->height / ICON_LINE_HEIGHT) - 1 > ICON_BUFFER) {
					IConsoleWindow::scroll = ICON_BUFFER;
				} else {
					IConsoleWindow::scroll += (this->height / ICON_LINE_HEIGHT) - 1;
				}
				this->SetDirty();
				break;

			case WKC_SHIFT | WKC_UP:
				if (IConsoleWindow::scroll <= 0) {
					IConsoleWindow::scroll = 0;
				} else {
					--IConsoleWindow::scroll;
				}
				this->SetDirty();
				break;

			case WKC_SHIFT | WKC_DOWN:
				if (IConsoleWindow::scroll >= ICON_BUFFER) {
					IConsoleWindow::scroll = ICON_BUFFER;
				} else {
					++IConsoleWindow::scroll;
				}
				this->SetDirty();
				break;

			case WKC_BACKQUOTE:
				IConsoleSwitch();
				break;

			case WKC_RETURN: case WKC_NUM_ENTER:
				IConsolePrintF(CC_COMMAND, "] %s", _iconsole_cmdline.buf);
				IConsoleHistoryAdd(_iconsole_cmdline.buf);

				IConsoleCmdExec(_iconsole_cmdline.buf);
				IConsoleClearCommand();
				break;

			case WKC_CTRL | WKC_RETURN:
				_iconsole_mode = (_iconsole_mode == ICONSOLE_FULL) ? ICONSOLE_OPENED : ICONSOLE_FULL;
				IConsoleResize(this);
				MarkWholeScreenDirty();
				break;

			case (WKC_CTRL | 'V'):
				if (InsertTextBufferClipboard(&_iconsole_cmdline)) {
					IConsoleResetHistoryPos();
					this->SetDirty();
				}
				break;

			case (WKC_CTRL | 'L'):
				IConsoleCmdExec("clear");
				break;

			case (WKC_CTRL | 'U'):
				DeleteTextBufferAll(&_iconsole_cmdline);
				this->SetDirty();
				break;

			case WKC_BACKSPACE: case WKC_DELETE:
				if (DeleteTextBufferChar(&_iconsole_cmdline, keycode)) {
					IConsoleResetHistoryPos();
					this->SetDirty();
				}
				break;

			case WKC_LEFT: case WKC_RIGHT: case WKC_END: case WKC_HOME:
				if (MoveTextBufferPos(&_iconsole_cmdline, keycode)) {
					IConsoleResetHistoryPos();
					this->SetDirty();
				}
				break;

			default:
				if (IsValidChar(key, CS_ALPHANUMERAL)) {
					IConsoleWindow::scroll = ICON_BUFFER;
					InsertTextBufferChar(&_iconsole_cmdline, key);
					IConsoleResetHistoryPos();
					this->SetDirty();
				} else {
					return ES_NOT_HANDLED;
				}
		}
		return ES_HANDLED;
	}
};

byte IConsoleWindow::scroll = ICON_BUFFER;

static const Widget _iconsole_window_widgets[] = {
	{WIDGETS_END}
};

static const WindowDesc _iconsole_window_desc = {
	0, 0, 2, 2, 2, 2,
	WC_CONSOLE, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_iconsole_window_widgets,
};

void IConsoleGUIInit()
{
	_iconsole_historypos = ICON_HISTORY_SIZE - 1;
	_iconsole_mode = ICONSOLE_CLOSED;

	memset(_iconsole_history, 0, sizeof(_iconsole_history));
	memset(_iconsole_buffer, 0, sizeof(_iconsole_buffer));
	memset(_iconsole_cbuffer, 0, sizeof(_iconsole_cbuffer));
	_iconsole_cmdline.buf = CallocT<char>(ICON_CMDLN_SIZE); // create buffer and zero it
	_iconsole_cmdline.maxlength = ICON_CMDLN_SIZE;

	IConsolePrintF(CC_WARNING, "OpenTTD Game Console Revision 7 - %s", _openttd_revision);
	IConsolePrint(CC_WHITE,  "------------------------------------");
	IConsolePrint(CC_WHITE,  "use \"help\" for more information");
	IConsolePrint(CC_WHITE,  "");
	IConsoleClearCommand();
	IConsoleHistoryAdd("");
}

void IConsoleClearBuffer()
{
	uint i;
	for (i = 0; i <= ICON_BUFFER; i++) {
		free(_iconsole_buffer[i]);
		_iconsole_buffer[i] = NULL;
	}
}

void IConsoleGUIFree()
{
	free(_iconsole_cmdline.buf);
	IConsoleClearBuffer();
}

void IConsoleResize(Window *w)
{
	switch (_iconsole_mode) {
		case ICONSOLE_OPENED:
			w->height = _screen.height / 3;
			w->width = _screen.width;
			break;
		case ICONSOLE_FULL:
			w->height = _screen.height - ICON_BOTTOM_BORDERWIDTH;
			w->width = _screen.width;
			break;
		default: return;
	}

	MarkWholeScreenDirty();
}

void IConsoleSwitch()
{
	switch (_iconsole_mode) {
		case ICONSOLE_CLOSED:
			new IConsoleWindow(&_iconsole_window_desc);
			break;

		case ICONSOLE_OPENED: case ICONSOLE_FULL:
			DeleteWindowById(WC_CONSOLE, 0);
			break;
	}

	MarkWholeScreenDirty();
}

void IConsoleClose() {if (_iconsole_mode == ICONSOLE_OPENED) IConsoleSwitch();}
void IConsoleOpen()  {if (_iconsole_mode == ICONSOLE_CLOSED) IConsoleSwitch();}

/**
 * Add the entered line into the history so you can look it back
 * scroll, etc. Put it to the beginning as it is the latest text
 * @param cmd Text to be entered into the 'history'
 */
static void IConsoleHistoryAdd(const char *cmd)
{
	free(_iconsole_history[ICON_HISTORY_SIZE - 1]);

	memmove(&_iconsole_history[1], &_iconsole_history[0], sizeof(_iconsole_history[0]) * (ICON_HISTORY_SIZE - 1));
	_iconsole_history[0] = strdup(cmd);
	IConsoleResetHistoryPos();
}

/**
 * Navigate Up/Down in the history of typed commands
 * @param direction Go further back in history (+1), go to recently typed commands (-1)
 */
static void IConsoleHistoryNavigate(int direction)
{
	int i = _iconsole_historypos + direction;

	/* watch out for overflows, just wrap around */
	if (i < 0) i = ICON_HISTORY_SIZE - 1;
	if (i >= ICON_HISTORY_SIZE) i = 0;

	if (direction > 0) {
		if (_iconsole_history[i] == NULL) i = 0;
	}

	if (direction < 0) {
		while (i > 0 && _iconsole_history[i] == NULL) i--;
	}

	_iconsole_historypos = i;
	IConsoleClearCommand();
	/* copy history to 'command prompt / bash' */
	assert(_iconsole_history[i] != NULL && IsInsideMM(i, 0, ICON_HISTORY_SIZE));
	ttd_strlcpy(_iconsole_cmdline.buf, _iconsole_history[i], _iconsole_cmdline.maxlength);
	UpdateTextBufferSize(&_iconsole_cmdline);
}

/**
 * Handle the printing of text entered into the console or redirected there
 * by any other means. Text can be redirected to other players in a network game
 * as well as to a logfile. If the network server is a dedicated server, all activities
 * are also logged. All lines to print are added to a temporary buffer which can be
 * used as a history to print them onscreen
 * @param color_code the colour of the command. Red in case of errors, etc.
 * @param string the message entered or output on the console (notice, error, etc.)
 */
void IConsoleGUIPrint(ConsoleColour color_code, char *str)
{
	/* move up all the strings in the buffer one place and do the same for colour
	 * to accomodate for the new command/message */
	free(_iconsole_buffer[0]);
	memmove(&_iconsole_buffer[0], &_iconsole_buffer[1], sizeof(_iconsole_buffer[0]) * ICON_BUFFER);
	_iconsole_buffer[ICON_BUFFER] = str;

	memmove(&_iconsole_cbuffer[0], &_iconsole_cbuffer[1], sizeof(_iconsole_cbuffer[0]) * ICON_BUFFER);
	_iconsole_cbuffer[ICON_BUFFER] = color_code;

	SetWindowDirty(FindWindowById(WC_CONSOLE, 0));
}
