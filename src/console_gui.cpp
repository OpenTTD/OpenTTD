/* $Id$ */

/** @file console_gui.cpp Handling the GUI of the in-game console. */

#include "stdafx.h"
#include "textbuf_gui.h"
#include "window_gui.h"
#include "console_gui.h"
#include "console_internal.h"
#include "window_func.h"
#include "string_func.h"
#include "gfx_func.h"
#include "core/math_func.hpp"
#include "settings_type.h"
#include "rev.h"

#include "table/strings.h"

enum {
	ICON_HISTORY_SIZE       = 20,
	ICON_LINE_HEIGHT        = 12,
	ICON_RIGHT_BORDERWIDTH  = 10,
	ICON_BOTTOM_BORDERWIDTH = 12,
};

/**
 * Container for a single line of console output
 */
struct IConsoleLine {
	static IConsoleLine *front; ///< The front of the console backlog buffer
	static int size;            ///< The amount of items in the backlog

	IConsoleLine *previous; ///< The previous console message.
	char *buffer;           ///< The data to store.
	TextColour colour;      ///< The colour of the line.
	uint16 time;            ///< The amount of time the line is in the backlog.

	/**
	 * Initialize the console line.
	 * @param buffer the data to print.
	 * @param colour the colour of the line.
	 */
	IConsoleLine(char *buffer, TextColour colour) :
			previous(IConsoleLine::front),
			buffer(buffer),
			colour(colour),
			time(0)
	{
		IConsoleLine::front = this;
		IConsoleLine::size++;
	}

	/**
	 * Clear this console line and any further ones.
	 */
	~IConsoleLine()
	{
		IConsoleLine::size--;
		free(buffer);

		delete previous;
	}

	/**
	 * Get the index-ed item in the list.
	 */
	static const IConsoleLine *Get(uint index)
	{
		const IConsoleLine *item = IConsoleLine::front;
		while (index != 0 && item != NULL) {
			index--;
			item = item->previous;
		}

		return item;
	}

	/**
	 * Truncate the list removing everything older than/more than the amount
	 * as specified in the config file.
	 * As a side effect also increase the time the other lines have been in
	 * the list.
	 * @return true if and only if items got removed.
	 */
	static bool Truncate()
	{
		IConsoleLine *cur = IConsoleLine::front;
		if (cur == NULL) return false;

		int count = 1;
		for (IConsoleLine *item = cur->previous; item != NULL; count++, cur = item, item = item->previous) {
			if (item->time > _settings_client.gui.console_backlog_timeout &&
					count > _settings_client.gui.console_backlog_length) {
				delete item;
				cur->previous = NULL;
				return true;
			}

			if (item->time != MAX_UVALUE(uint16)) item->time++;
		}

		return false;
	}

	/**
	 * Reset the complete console line backlog.
	 */
	static void Reset()
	{
		delete IConsoleLine::front;
		IConsoleLine::front = NULL;
		IConsoleLine::size = 0;
	}
};

/* static */ IConsoleLine *IConsoleLine::front = NULL;
/* static */ int IConsoleLine::size  = 0;


/* ** main console cmd buffer ** */
static Textbuf _iconsole_cmdline;
static char *_iconsole_history[ICON_HISTORY_SIZE];
static byte _iconsole_historypos;
IConsoleModes _iconsole_mode;

/* *************** *
 *  end of header  *
 * *************** */

static void IConsoleClearCommand()
{
	memset(_iconsole_cmdline.buf, 0, ICON_CMDLN_SIZE);
	_iconsole_cmdline.size = 1; // only terminating zero
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
	static int scroll;

	IConsoleWindow(const WindowDesc *desc) : Window(desc)
	{
		_iconsole_mode = ICONSOLE_OPENED;

		this->height = _screen.height / 3;
		this->width  = _screen.width;
	}

	~IConsoleWindow()
	{
		_iconsole_mode = ICONSOLE_CLOSED;
	}

	virtual void OnPaint()
	{
		int max = (this->height / ICON_LINE_HEIGHT) - 1;
		const IConsoleLine *print = IConsoleLine::Get(IConsoleWindow::scroll);
		GfxFillRect(this->left, this->top, this->width, this->height - 1, 0);
		for (int i = 0; i < max && print != NULL; i++, print = print->previous) {
			DoDrawString(print->buffer, 5,
				this->height - (2 + i) * ICON_LINE_HEIGHT, print->colour);
		}
		/* If the text is longer than the window, don't show the starting ']' */
		int delta = this->width - 10 - _iconsole_cmdline.width - ICON_RIGHT_BORDERWIDTH;
		if (delta > 0) {
			DoDrawString("]", 5, this->height - ICON_LINE_HEIGHT, (TextColour)CC_COMMAND);
			delta = 0;
		}

		DoDrawString(_iconsole_cmdline.buf, 10 + delta, this->height - ICON_LINE_HEIGHT, (TextColour)CC_COMMAND);

		if (_focused_window == this && _iconsole_cmdline.caret) {
			DoDrawString("_", 10 + delta + _iconsole_cmdline.caretxoffs, this->height - ICON_LINE_HEIGHT, TC_WHITE);
		}
	}

	virtual void OnHundredthTick()
	{
		if (IConsoleLine::Truncate() &&
				(IConsoleWindow::scroll > IConsoleLine::size)) {
			IConsoleWindow::scroll = max(0, IConsoleLine::size - (this->height / ICON_LINE_HEIGHT) + 1);
			this->SetDirty();
		}
	}

	virtual void OnMouseLoop()
	{
		if (HandleCaret(&_iconsole_cmdline)) this->SetDirty();
	}

	virtual EventState OnKeyPress(uint16 key, uint16 keycode)
	{
		if (_focused_window != this) return ES_NOT_HANDLED;

		const int scroll_height = (this->height / ICON_LINE_HEIGHT) - 1;
		switch (keycode) {
			case WKC_UP:
				IConsoleHistoryNavigate(1);
				this->SetDirty();
				break;

			case WKC_DOWN:
				IConsoleHistoryNavigate(-1);
				this->SetDirty();
				break;

			case WKC_SHIFT | WKC_PAGEDOWN:
				if (IConsoleWindow::scroll - scroll_height < 0) {
					IConsoleWindow::scroll = 0;
				} else {
					IConsoleWindow::scroll -= scroll_height;
				}
				this->SetDirty();
				break;

			case WKC_SHIFT | WKC_PAGEUP:
				if (IConsoleWindow::scroll + scroll_height > IConsoleLine::size - scroll_height) {
					IConsoleWindow::scroll = IConsoleLine::size - scroll_height;
				} else {
					IConsoleWindow::scroll += scroll_height;
				}
				this->SetDirty();
				break;

			case WKC_SHIFT | WKC_DOWN:
				if (IConsoleWindow::scroll <= 0) {
					IConsoleWindow::scroll = 0;
				} else {
					--IConsoleWindow::scroll;
				}
				this->SetDirty();
				break;

			case WKC_SHIFT | WKC_UP:
				if (IConsoleWindow::scroll >= IConsoleLine::size) {
					IConsoleWindow::scroll = IConsoleLine::size;
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
					IConsoleWindow::scroll = 0;
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

int IConsoleWindow::scroll = 0;

static const Widget _iconsole_window_widgets[] = {
	{WIDGETS_END}
};

static const WindowDesc _iconsole_window_desc(
	0, 0, 2, 2, 2, 2,
	WC_CONSOLE, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_iconsole_window_widgets
);

void IConsoleGUIInit()
{
	_iconsole_historypos = ICON_HISTORY_SIZE - 1;
	_iconsole_mode = ICONSOLE_CLOSED;

	IConsoleLine::Reset();
	memset(_iconsole_history, 0, sizeof(_iconsole_history));

	_iconsole_cmdline.buf = CallocT<char>(ICON_CMDLN_SIZE); // create buffer and zero it
	_iconsole_cmdline.maxsize = ICON_CMDLN_SIZE;

	IConsolePrintF(CC_WARNING, "OpenTTD Game Console Revision 7 - %s", _openttd_revision);
	IConsolePrint(CC_WHITE,  "------------------------------------");
	IConsolePrint(CC_WHITE,  "use \"help\" for more information");
	IConsolePrint(CC_WHITE,  "");
	IConsoleClearCommand();
	IConsoleHistoryAdd("");
}

void IConsoleClearBuffer()
{
	IConsoleLine::Reset();
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
	ttd_strlcpy(_iconsole_cmdline.buf, _iconsole_history[i], _iconsole_cmdline.maxsize);
	UpdateTextBufferSize(&_iconsole_cmdline);
}

/**
 * Handle the printing of text entered into the console or redirected there
 * by any other means. Text can be redirected to other clients in a network game
 * as well as to a logfile. If the network server is a dedicated server, all activities
 * are also logged. All lines to print are added to a temporary buffer which can be
 * used as a history to print them onscreen
 * @param colour_code the colour of the command. Red in case of errors, etc.
 * @param string the message entered or output on the console (notice, error, etc.)
 */
void IConsoleGUIPrint(ConsoleColour colour_code, char *str)
{
	new IConsoleLine(str, (TextColour)colour_code);
	SetWindowDirty(FindWindowById(WC_CONSOLE, 0));
}
