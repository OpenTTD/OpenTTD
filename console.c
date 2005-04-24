#include "stdafx.h"
#include "ttd.h"
#include "table/strings.h"
#include "window.h"
#include "gui.h"
#include "gfx.h"
#include "player.h"
#include "variables.h"
#include "string.h"
#include "hal.h"
#include <stdarg.h>
#include <string.h>
#include "console.h"
#include "network.h"
#include "network_data.h"
#include "network_server.h"

#define ICON_BUFFER 79
#define ICON_HISTORY_SIZE 20
#define ICON_CMDLN_SIZE 255
#define ICON_LINE_HEIGHT 12
#define ICON_RIGHT_BORDERWIDTH 10
#define ICON_BOTTOM_BORDERWIDTH 12
#define ICON_MAX_ALIAS_LINES 40
#define ICON_TOKEN_COUNT 20

// ** main console ** //
static Window *_iconsole_win; // Pointer to console window
static bool _iconsole_inited;
static char* _iconsole_buffer[ICON_BUFFER + 1];
static uint16 _iconsole_cbuffer[ICON_BUFFER + 1];
static Textbuf _iconsole_cmdline;
static byte _iconsole_scroll;

// ** stdlib ** //
byte _stdlib_developer = 1;
bool _stdlib_con_developer = false;
FILE* _iconsole_output_file;

// ** main console cmd buffer
static char *_iconsole_history[ICON_HISTORY_SIZE];
static byte _iconsole_historypos;

/* *************** */
/*  end of header  */
/* *************** */

static void IConsoleClearCommand(void)
{
	memset(_iconsole_cmdline.buf, 0, ICON_CMDLN_SIZE);
	_iconsole_cmdline.length = 0;
	_iconsole_cmdline.width = 0;
	_iconsole_cmdline.caretpos = 0;
	_iconsole_cmdline.caretxoffs = 0;
	SetWindowDirty(_iconsole_win);
}

static inline void IConsoleResetHistoryPos(void) {_iconsole_historypos = ICON_HISTORY_SIZE - 1;}

// ** console window ** //
static void IConsoleWndProc(Window* w, WindowEvent* e)
{
	switch (e->event) {
		case WE_PAINT: {
			int i = _iconsole_scroll;
			int max = (w->height / ICON_LINE_HEIGHT) - 1;
			int delta = 0;
			GfxFillRect(w->left, w->top, w->width, w->height - 1, 0);
			while ((i > 0) && (i > _iconsole_scroll - max) && (_iconsole_buffer[i] != NULL)) {
				DoDrawString(_iconsole_buffer[i], 5,
					w->height - (_iconsole_scroll + 2 - i) * ICON_LINE_HEIGHT, _iconsole_cbuffer[i]);
				i--;
			}
			/* If the text is longer than the window, don't show the starting ']' */
			delta = w->width - 10 - _iconsole_cmdline.width - ICON_RIGHT_BORDERWIDTH;
			if (delta > 0) {
				DoDrawString("]", 5, w->height - ICON_LINE_HEIGHT, _iconsole_color_commands);
				delta = 0;
			}

			DoDrawString(_iconsole_cmdline.buf, 10 + delta, w->height - ICON_LINE_HEIGHT, _iconsole_color_commands);

			if (_iconsole_cmdline.caret)
				DoDrawString("_", 10 + delta + _iconsole_cmdline.caretxoffs, w->height - ICON_LINE_HEIGHT, 12);
			break;
		}
		case WE_MOUSELOOP:
			if (HandleCaret(&_iconsole_cmdline))
				SetWindowDirty(w);
			break;
		case WE_DESTROY:
			_iconsole_win = NULL;
			_iconsole_mode = ICONSOLE_CLOSED;
			break;
		case WE_KEYPRESS:
			e->keypress.cont = false;
			switch (e->keypress.keycode) {
				case WKC_UP:
					IConsoleHistoryNavigate(+1);
					SetWindowDirty(w);
					break;
				case WKC_DOWN:
					IConsoleHistoryNavigate(-1);
					SetWindowDirty(w);
					break;
				case WKC_SHIFT | WKC_PAGEUP:
					if (_iconsole_scroll - (w->height / ICON_LINE_HEIGHT) - 1 < 0)
						_iconsole_scroll = 0;
					else
						_iconsole_scroll -= (w->height / ICON_LINE_HEIGHT) - 1;
					SetWindowDirty(w);
					break;
				case WKC_SHIFT | WKC_PAGEDOWN:
					if (_iconsole_scroll + (w->height / ICON_LINE_HEIGHT) - 1 > ICON_BUFFER)
						_iconsole_scroll = ICON_BUFFER;
					else
						_iconsole_scroll += (w->height / ICON_LINE_HEIGHT) - 1;
					SetWindowDirty(w);
					break;
				case WKC_SHIFT | WKC_UP:
					if (_iconsole_scroll <= 0)
						_iconsole_scroll = 0;
					else
						--_iconsole_scroll;
					SetWindowDirty(w);
					break;
				case WKC_SHIFT | WKC_DOWN:
					if (_iconsole_scroll >= ICON_BUFFER)
						_iconsole_scroll = ICON_BUFFER;
					else
						++_iconsole_scroll;
					SetWindowDirty(w);
					break;
				case WKC_BACKQUOTE:
					IConsoleSwitch();
					break;
				case WKC_RETURN: case WKC_NUM_ENTER:
					IConsolePrintF(_iconsole_color_commands, "] %s", _iconsole_cmdline.buf);
					IConsoleHistoryAdd(_iconsole_cmdline.buf);

					IConsoleCmdExec(_iconsole_cmdline.buf);
					IConsoleClearCommand();
					break;
				case WKC_CTRL | WKC_RETURN:
					_iconsole_mode = (_iconsole_mode == ICONSOLE_FULL) ? ICONSOLE_OPENED : ICONSOLE_FULL;
					IConsoleResize();
					MarkWholeScreenDirty();
					break;
				case (WKC_CTRL | 'V'):
					if (InsertTextBufferClipboard(&_iconsole_cmdline)) {
						IConsoleResetHistoryPos();
						SetWindowDirty(w);
					}
					break;
				case WKC_BACKSPACE: case WKC_DELETE:
					if (DeleteTextBufferChar(&_iconsole_cmdline, e->keypress.keycode)) {
						IConsoleResetHistoryPos();
						SetWindowDirty(w);
					}
					break;
				case WKC_LEFT: case WKC_RIGHT: case WKC_END: case WKC_HOME:
					if (MoveTextBufferPos(&_iconsole_cmdline, e->keypress.keycode)) {
						IConsoleResetHistoryPos();
						SetWindowDirty(w);
					}
					break;
				default:
					if (IsValidAsciiChar(e->keypress.ascii)) {
						_iconsole_scroll = ICON_BUFFER;
						InsertTextBufferChar(&_iconsole_cmdline, e->keypress.ascii);
						IConsoleResetHistoryPos();
						SetWindowDirty(w);
					} else
						e->keypress.cont = true;
			break;
		}
	}
}

static const Widget _iconsole_window_widgets[] = {
	{WIDGETS_END}
};

static const WindowDesc _iconsole_window_desc = {
	0, 0, 2, 2,
	WC_CONSOLE, 0,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_iconsole_window_widgets,
	IConsoleWndProc,
};

extern const char _openttd_revision[];

void IConsoleInit(void)
{
	_iconsole_output_file = NULL;
	_iconsole_color_default = 1;
	_iconsole_color_error = 3;
	_iconsole_color_warning = 13;
	_iconsole_color_debug = 5;
	_iconsole_color_commands = 2;
	_iconsole_scroll = ICON_BUFFER;
	_iconsole_historypos = ICON_HISTORY_SIZE - 1;
	_iconsole_inited = true;
	_iconsole_mode = ICONSOLE_CLOSED;
	_iconsole_win = NULL;

#ifdef ENABLE_NETWORK /* Initialize network only variables */
	_redirect_console_to_client = 0;
#endif

	memset(_iconsole_history, 0, sizeof(_iconsole_history));
	memset(_iconsole_buffer, 0, sizeof(_iconsole_buffer));
	memset(_iconsole_cbuffer, 0, sizeof(_iconsole_cbuffer));
	_iconsole_cmdline.buf = calloc(ICON_CMDLN_SIZE, sizeof(*_iconsole_cmdline.buf)); // create buffer and zero it
	_iconsole_cmdline.maxlength = ICON_CMDLN_SIZE - 1;

	IConsoleStdLibRegister();
	IConsolePrintF(13, "OpenTTD Game Console Revision 7 - %s", _openttd_revision);
	IConsolePrint(12,  "------------------------------------");
	IConsolePrint(12,  "use \"help\" for more info");
	IConsolePrint(12,  "");
	IConsoleClearCommand();
	IConsoleHistoryAdd("");
}

void IConsoleClear(void)
{
	uint i;
	for (i = 0; i <= ICON_BUFFER; i++)
		free(_iconsole_buffer[i]);

	free(_iconsole_cmdline.buf);
}

static void IConsoleWriteToLogFile(const char* string)
{
	if (_iconsole_output_file != NULL) {
		// if there is an console output file ... also print it there
		fwrite(string, strlen(string), 1, _iconsole_output_file);
		fwrite("\n", 1, 1, _iconsole_output_file);
	}
}

bool CloseConsoleLogIfActive(void)
{
	if (_iconsole_output_file != NULL) {
		IConsolePrintF(_iconsole_color_default, "file output complete");
		fclose(_iconsole_output_file);
		_iconsole_output_file = NULL;
		return true;
	}

	return false;
}

void IConsoleFree(void)
{
	_iconsole_inited = false;
	IConsoleClear();
	CloseConsoleLogIfActive();
}

void IConsoleResize(void)
{
	_iconsole_win = FindWindowById(WC_CONSOLE, 0);

	switch (_iconsole_mode) {
		case ICONSOLE_OPENED:
			_iconsole_win->height = _screen.height / 3;
			_iconsole_win->width = _screen.width;
			break;
		case ICONSOLE_FULL:
			_iconsole_win->height = _screen.height - ICON_BOTTOM_BORDERWIDTH;
			_iconsole_win->width = _screen.width;
			break;
		default:
			NOT_REACHED();
	}

	MarkWholeScreenDirty();
}

void IConsoleSwitch(void)
{
	switch (_iconsole_mode) {
		case ICONSOLE_CLOSED:
			_iconsole_win = AllocateWindowDesc(&_iconsole_window_desc);
			_iconsole_win->height = _screen.height / 3;
			_iconsole_win->width = _screen.width;
			_iconsole_mode = ICONSOLE_OPENED;
			SETBIT(_no_scroll, SCROLL_CON); // override cursor arrows; the gamefield will not scroll
			break;
		case ICONSOLE_OPENED: case ICONSOLE_FULL:
			DeleteWindowById(WC_CONSOLE, 0);
			_iconsole_win = NULL;
			_iconsole_mode = ICONSOLE_CLOSED;
			CLRBIT(_no_scroll, SCROLL_CON);
			break;
	}

	MarkWholeScreenDirty();
}

void IConsoleClose(void)
{
	if (_iconsole_mode == ICONSOLE_OPENED) IConsoleSwitch();
}

void IConsoleOpen(void)
{
	if (_iconsole_mode == ICONSOLE_CLOSED) IConsoleSwitch();
}

/**
 * Add the entered line into the history so you can look it back
 * scroll, etc. Put it to the beginning as it is the latest text
 * @param cmd Text to be entered into the 'history'
 */
void IConsoleHistoryAdd(const char *cmd)
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
void IConsoleHistoryNavigate(signed char direction)
{
	int i = _iconsole_historypos + direction;

	// watch out for overflows, just wrap around
	if (i < 0) i = ICON_HISTORY_SIZE - 1;
	if (i >= ICON_HISTORY_SIZE) i = 0;

	if (direction > 0)
		if (_iconsole_history[i] == NULL) i = 0;

	if (direction < 0) {
		while (i > 0 && _iconsole_history[i] == NULL) i--;
	}

	_iconsole_historypos = i;
	IConsoleClearCommand();
	// copy history to 'command prompt / bash'
	assert(_iconsole_history[i] != NULL && IS_INT_INSIDE(i, 0, ICON_HISTORY_SIZE));
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
void IConsolePrint(uint16 color_code, const char* string)
{
	char *i;

#ifdef ENABLE_NETWORK
	if (_redirect_console_to_client != 0) {
		/* Redirect the string to the client */
		SEND_COMMAND(PACKET_SERVER_RCON)(NetworkFindClientStateFromIndex(_redirect_console_to_client), color_code, string);
		return;
	}
#endif

	if (_network_dedicated) {
		printf("%s\n", string);
		IConsoleWriteToLogFile(string);
		return;
	}

	if (!_iconsole_inited) return;

	/* move up all the strings in the buffer one place and do the same for colour
	 * to accomodate for the new command/message */
	free(_iconsole_buffer[0]);
	memmove(&_iconsole_buffer[0], &_iconsole_buffer[1], sizeof(_iconsole_buffer[0]) * ICON_BUFFER);
	_iconsole_buffer[ICON_BUFFER] = strdup(string);

	// filter out unprintable characters
	for (i = _iconsole_buffer[ICON_BUFFER]; *i != '\0'; i++)
		if (!IsValidAsciiChar((byte)*i)) *i = ' ';

	memmove(&_iconsole_cbuffer[0], &_iconsole_cbuffer[1], sizeof(_iconsole_cbuffer[0]) * ICON_BUFFER);
	_iconsole_cbuffer[ICON_BUFFER] = color_code;

	IConsoleWriteToLogFile(string);

	if (_iconsole_win != NULL) SetWindowDirty(_iconsole_win);
}

/**
 * Handle the printing of text entered into the console or redirected there
 * by any other means. Uses printf() style format, for more information look
 * at @IConsolePrint()
 */
void CDECL IConsolePrintF(uint16 color_code, const char* s, ...)
{
	va_list va;
	char buf[ICON_MAX_STREAMSIZE];

	va_start(va, s);
	vsnprintf(buf, sizeof(buf), s, va);
	va_end(va);

	IConsolePrint(color_code, buf);
}

/**
 * It is possible to print debugging information to the console,
 * which is achieved by using this function. Can only be used by
 * @debug() in debug.c. You need at least a level 2 (developer) for debugging
 * messages to show up
 */
void IConsoleDebug(const char* string)
{
	if (_stdlib_developer > 1)
		IConsolePrintF(_iconsole_color_debug, "dbg: %s", string);
}

/**
 * It is possible to print warnings to the console. These are mostly
 * errors or mishaps, but non-fatal. You need at least a level 1 (developer) for
 * debugging messages to show up
 */
void IConsoleWarning(const char* string)
{
	if (_stdlib_developer > 0)
		IConsolePrintF(_iconsole_color_warning, "WARNING: %s", string);
}

/**
 * It is possible to print error information to the console. This can include
 * game errors, or errors in general you would want the user to notice
 */
void IConsoleError(const char* string)
{
	IConsolePrintF(_iconsole_color_error, "ERROR: %s", string);
}

/**
 * Register a new command to be used in the console
 * @param name name of the command that will be used
 * @param addr function that will be called upon execution of command
 */
void IConsoleCmdRegister(const char *name, IConsoleCmdAddr addr)
{
	IConsoleCmd *item, *item_before;
	char *new_cmd = strdup(name);
	IConsoleCmd *item_new = malloc(sizeof(IConsoleCmd));

	item_new->_next = NULL;
	item_new->addr = addr;
	item_new->name = new_cmd;

	item_new->hook_access = NULL;
	item_new->hook_after_exec = NULL;
	item_new->hook_before_exec = NULL;

	// first command
	if (_iconsole_cmds == NULL) {
		_iconsole_cmds = item_new;
		return;
	}

	item_before = NULL;
	item = _iconsole_cmds;

	/* BEGIN - Alphabetically insert the commands into the linked list */
	while (item != NULL) {
		int i = strcmp(item->name, item_new->name);
		if (i == 0) {
			IConsoleError("a command with this name already exists; insertion aborted");
			free(item_new);
			return;
		}

		if (i > 0) break; // insert at this position

		item_before = item;
		item = item->_next;
	}

	if (item_before == NULL) {
		_iconsole_cmds = item_new;
	} else
		item_before->_next = item_new;

	item_new->_next = item;
	/* END - Alphabetical insert */
}

/**
 * Find the command pointed to by its string
 * @param name command to be found
 * @return return Cmdstruct of the found command, or NULL on failure
 */
IConsoleCmd *IConsoleCmdGet(const char *name)
{
	IConsoleCmd *item;

	for (item = _iconsole_cmds; item != NULL; item = item->_next) {
		if (strcmp(item->name, name) == 0) return item;
	}
	return NULL;
}

/**
 * Register a an alias for an already existing command in the console
 * @param name name of the alias that will be used
 * @param cmd name of the command that 'name' will be alias of
 */
void IConsoleAliasRegister(const char *name, const char *cmd)
{
	IConsoleAlias *item, *item_before;
	char *new_alias = strdup(name);
	char *cmd_aliased = strdup(cmd);
	IConsoleAlias *item_new = malloc(sizeof(IConsoleAlias));

	item_new->_next = NULL;
	item_new->cmdline = cmd_aliased;
	item_new->name = new_alias;

	item_before = NULL;
	item = _iconsole_aliases;

	// first command
	if (_iconsole_aliases == NULL) {
		_iconsole_aliases = item_new;
		return;
	}

	item_before = NULL;
	item = _iconsole_aliases;

	/* BEGIN - Alphabetically insert the commands into the linked list */
	while (item != NULL) {
		int i = strcmp(item->name, item_new->name);
		if (i == 0) {
			IConsoleError("an alias with this name already exists; insertion aborted");
			free(item_new);
			return;
		}

		if (i > 0) break; // insert at this position

		item_before = item;
		item = item->_next;
	}

	if (item_before == NULL) {
		_iconsole_aliases = item_new;
	} else
		item_before->_next = item_new;

	item_new->_next = item;
	/* END - Alphabetical insert */
}

/**
 * Find the alias pointed to by its string
 * @param name alias to be found
 * @return return Aliasstruct of the found alias, or NULL on failure
 */
IConsoleAlias *IConsoleAliasGet(const char *name)
{
	IConsoleAlias* item;

	for (item = _iconsole_aliases; item != NULL; item = item->_next) {
		if (strcmp(item->name, name) == 0) return item;
	}

	return NULL;
}

static inline int IConsoleCopyInParams(char *dst, const char *src, uint bufpos)
{
	int len = min(ICON_MAX_STREAMSIZE - bufpos, strlen(src));
	strncpy(dst, src, len);

	return len;
}

/**
 * An alias is just another name for a command, or for more commands
 * Execute it as well.
 * @param cmdstr is the alias of the command
 * @param tokens are the parameters given to the original command (0 is the first param)
 * @param tokencount the number of parameters passed
 */
static void IConsoleAliasExec(const char *cmdstr, char *tokens[ICON_TOKEN_COUNT], int tokencount)
{
	const char *cmdptr;
	char *aliases[ICON_MAX_ALIAS_LINES], aliasstream[ICON_MAX_STREAMSIZE];
	int i;
	uint a_index, astream_i;

	memset(&aliases, 0, sizeof(aliases));
	memset(&aliasstream, 0, sizeof(aliasstream));

	aliases[0] = aliasstream;
	for (cmdptr = cmdstr, a_index = 0, astream_i = 0; *cmdptr != '\0'; *cmdptr++) {
		if (a_index >= lengthof(aliases) || astream_i >= lengthof(aliasstream)) break;

		switch (*cmdptr) {
		case '\'': /* ' will double for : */
			aliasstream[astream_i++] = '"';
			break;
		case ';': /* Cmd seperator, start new line */
			aliasstream[astream_i] = '\0';
			aliases[++a_index] = &aliasstream[++astream_i];
			*cmdptr++;
			break;
		case '%': /* One specific parameter: %A = [param 1] %B = [param 2] ... */
			*cmdptr++;
			switch (*cmdptr) {
			case '+': { /* All parameters seperated: "[param 1]" "[param 2]" */
				for (i = 0; i != tokencount; i++) {
					aliasstream[astream_i++] = '"';
					astream_i += IConsoleCopyInParams(&aliasstream[astream_i], tokens[i], astream_i);
					aliasstream[astream_i++] = '"';
					aliasstream[astream_i++] = ' ';
				}
			} break;
			case '!': { /* Merge the parameters to one: "[param 1] [param 2] [param 3...]" */
				aliasstream[astream_i++] = '"';
				for (i = 0; i != tokencount; i++) {
					astream_i += IConsoleCopyInParams(&aliasstream[astream_i], tokens[i], astream_i);
					aliasstream[astream_i++] = ' ';
				}
				aliasstream[astream_i++] = '"';

			} break;
				default: {
				int param = *cmdptr - 'A';

				if (param < 0 || param >= tokencount) {
					IConsoleError("too many or wrong amount of parameters passed to alias, aborting");
					return;
				}

				aliasstream[astream_i++] = '"';
				astream_i += IConsoleCopyInParams(&aliasstream[astream_i], tokens[param], astream_i);
				aliasstream[astream_i++] = '"';
			} break;
			} break;

		default:
			aliasstream[astream_i++] = *cmdptr;
			break;
		}
	}

	for (i = 0; i <= (int)a_index; i++) IConsoleCmdExec(aliases[i]);
}

#if 0
static void IConsoleAliasExec(const char* cmdline, char* tokens[TOKEN_COUNT], byte tokentypes[TOKEN_COUNT])
{
	char *lines[ICON_MAX_ALIAS_LINES];
	char *linestream, *linestream_s;

	int c;
	int i;
	int l;
	int x;
	byte t;

	memset(lines, 0, ICON_MAX_ALIAS_LINES); // clear buffer

	linestream_s = linestream = malloc(1024 * ICON_MAX_ALIAS_LINES);
	memset(linestream, 0, 1024 * ICON_MAX_ALIAS_LINES);

	//** parsing **//

	l = strlen(cmdline);
	i = 0;
	c = 0;
	x = 0;
	t = 0;
	lines[c] = linestream;

	while (i < l && c < ICON_MAX_ALIAS_LINES - 1) {
		if (cmdline[i] == '%') {
			i++;
			if (cmdline[i] == '+') {
				// all params seperated: "[param 1]" "[param 2]"
				t=1;
				while ((tokens[t]!=NULL) && (t<20) &&
						((tokentypes[t] == ICONSOLE_VAR_STRING) || (tokentypes[t] == ICONSOLE_VAR_UNKNOWN))) {
					int l2 = strlen(tokens[t]);
					*linestream = '"';
					linestream++;
					memcpy(linestream,tokens[t],l2);
					linestream += l2;
					*linestream = '"';
					linestream++;
					*linestream = ' ';
					linestream++;
					x += l2+3;
					t++;
				}
			} else if (cmdline[i] == '!') {
				// merge the params to one: "[param 1] [param 2] [param 3...]"
				t=1;
				*linestream = '"';
				linestream++;
				while ((tokens[t]!=NULL) && (t<20) &&
						((tokentypes[t] == ICONSOLE_VAR_STRING) || (tokentypes[t] == ICONSOLE_VAR_UNKNOWN))) {
					int l2 = strlen(tokens[t]);
					memcpy(linestream,tokens[t],l2);
					linestream += l2;
					*linestream = ' ';
					linestream++;
					x += l2+1;
					t++;
				}
				linestream--;
				*linestream = '"';
				linestream++;
				x += 1;
			} else {
				// one specific parameter: %A = [param 1] %B = [param 2] ...
				int l2;
				t = ((byte)cmdline[i]) - 64;
				if ((t<20) && (tokens[t]!=NULL) &&
						((tokentypes[t] == ICONSOLE_VAR_STRING) || (tokentypes[t] == ICONSOLE_VAR_UNKNOWN))) {
					l2 = strlen(tokens[t]);
					*linestream = '"';
					linestream++;
					memcpy(linestream,tokens[t],l2);
					linestream += l2;
					*linestream = '"';
					linestream++;
					x += l2+2;
				}
			}
		} else if (cmdline[i] == '\\') {
			// \\ = \       \' = '      \% = %
			i++;
			if (cmdline[i] == '\\') {
				*linestream = '\\';
				linestream++;
			} else if (cmdline[i] == '\'') {
				*linestream = '\'';
				linestream++;
			} else if (cmdline[i] == '%') {
				*linestream = '%';
				linestream++;
			}
		} else if (cmdline[i] == '\'') {
			// ' = "
			*linestream = '"';
			linestream++;
		} else if (cmdline[i] == ';') {
			// ; = start a new line
			c++;
			*linestream = '\0';
			linestream += 1024 - (x % 1024);
			x += 1024 - (x % 1024);
			lines[c] = linestream;
		} else {
			*linestream = cmdline[i];
			linestream++;
			x++;
		}
		i++;
	}

	linestream--;
	if (*linestream != '\0') {
		c++;
		linestream++;
		*linestream = '\0';
	}

	for (i = 0; i < c; i++)	IConsoleCmdExec(lines[i]);

	free(linestream_s);
}
#endif

static void IConsoleVarExec(char *token[ICON_TOKEN_COUNT]) {}

void IConsoleVarInsert(_iconsole_var* item_new, const char* name)
{
	_iconsole_var* item;
	_iconsole_var* item_before;

	item_new->_next = NULL;

	item_new->name = malloc(strlen(name) + 2); /* XXX unchecked malloc */
	sprintf(item_new->name, "%s", name);

	item_before = NULL;
	item = _iconsole_vars;

	if (item == NULL) {
		_iconsole_vars = item_new;
	} else {
		while ((item->_next != NULL) && (strcmp(item->name,item_new->name)<=0)) {
			item_before = item;
			item = item->_next;
			}
// insertion sort
		if (item_before==NULL) {
			if (strcmp(item->name,item_new->name)<=0) {
				// appending
				item ->_next = item_new;
			} else {
				// inserting as startitem
				_iconsole_vars = item_new;
				item_new ->_next = item;
			}
		} else {
			if (strcmp(item->name,item_new->name)<=0) {
				// appending
				item ->_next = item_new;
			} else {
				// inserting
				item_new ->_next = item_before->_next;
				item_before ->_next = item_new;
			}
		}
// insertion sort end
	}
}

void IConsoleVarRegister(const char* name, void* addr, _iconsole_var_types type)
{
	_iconsole_var* item_new;

	item_new = malloc(sizeof(_iconsole_var)); /* XXX unchecked malloc */

	item_new->_next = NULL;

	switch (type) {
		case ICONSOLE_VAR_BOOLEAN:
			item_new->data.bool_ = addr;
			break;
		case ICONSOLE_VAR_BYTE:
		case ICONSOLE_VAR_UINT8:
			item_new->data.byte_ = addr;
			break;
		case ICONSOLE_VAR_UINT16:
			item_new->data.uint16_ = addr;
			break;
		case ICONSOLE_VAR_UINT32:
			item_new->data.uint32_ = addr;
			break;
		case ICONSOLE_VAR_INT16:
			item_new->data.int16_ = addr;
			break;
		case ICONSOLE_VAR_INT32:
			item_new->data.int32_ = addr;
			break;
		case ICONSOLE_VAR_STRING:
			item_new->data.string_ = addr;
			break;
		default:
			error("unknown console variable type");
			break;
	}

	IConsoleVarInsert(item_new, name);

	item_new->type = type;
	item_new->_malloc = false;

	item_new->hook_access = NULL;
	item_new->hook_after_change = NULL;
	item_new->hook_before_change = NULL;

}

void IConsoleVarMemRegister(const char* name, _iconsole_var_types type)
{
	_iconsole_var* item;
	item = IConsoleVarAlloc(type);
	IConsoleVarInsert(item, name);
}

_iconsole_var* IConsoleVarGet(const char* name)
{
	_iconsole_var* item;
	for (item = _iconsole_vars; item != NULL; item = item->_next) {
		if (strcmp(item->name, name) == 0) return item;
	}

	return NULL;
}

_iconsole_var* IConsoleVarAlloc(_iconsole_var_types type)
{
	_iconsole_var* item = malloc(sizeof(_iconsole_var)); /* XXX unchecked malloc */
	item->_next = NULL;
	item->name = NULL;
	item->type = type;
	switch (item->type) {
		case ICONSOLE_VAR_BOOLEAN:
			item->data.bool_ = malloc(sizeof(*item->data.bool_));
			*item->data.bool_ = false;
			item->_malloc = true;
			break;
		case ICONSOLE_VAR_BYTE:
		case ICONSOLE_VAR_UINT8:
			item->data.byte_ = malloc(sizeof(*item->data.byte_));
			*item->data.byte_ = 0;
			item->_malloc = true;
			break;
		case ICONSOLE_VAR_UINT16:
			item->data.uint16_ = malloc(sizeof(*item->data.uint16_));
			*item->data.uint16_ = 0;
			item->_malloc = true;
			break;
		case ICONSOLE_VAR_UINT32:
			item->data.uint32_ = malloc(sizeof(*item->data.uint32_));
			*item->data.uint32_ = 0;
			item->_malloc = true;
			break;
		case ICONSOLE_VAR_INT16:
			item->data.int16_ = malloc(sizeof(*item->data.int16_));
			*item->data.int16_ = 0;
			item->_malloc = true;
			break;
		case ICONSOLE_VAR_INT32:
			item->data.int32_ = malloc(sizeof(*item->data.int32_));
			*item->data.int32_ = 0;
			item->_malloc = true;
			break;
		case ICONSOLE_VAR_POINTER:
		case ICONSOLE_VAR_STRING:
			// needs no memory ... it gets memory when it is set to an value
			item->data.addr = NULL;
			item->_malloc = false;
			break;
		default:
			error("unknown console variable type");
			break;
	}

	item->hook_access = NULL;
	item->hook_after_change = NULL;
	item->hook_before_change = NULL;
	return item;
}


void IConsoleVarFree(_iconsole_var* var)
{
	if (var->_malloc)
		free(var->data.addr);
	free(var->name);
	free(var);
}

void IConsoleVarSetString(_iconsole_var* var, const char* string)
{
	if (string == NULL) return;

	if (var->_malloc)
		free(var->data.string_);

	var->data.string_ = strdup(string);
	var->_malloc = true;
}

void IConsoleVarSetValue(_iconsole_var* var, int value) {
	switch (var->type) {
		case ICONSOLE_VAR_BOOLEAN:
			*var->data.bool_ = (value != 0);
			break;
		case ICONSOLE_VAR_BYTE:
		case ICONSOLE_VAR_UINT8:
			*var->data.byte_ = value;
			break;
		case ICONSOLE_VAR_UINT16:
			*var->data.uint16_ = value;
			break;
		case ICONSOLE_VAR_UINT32:
			*var->data.uint32_ = value;
			break;
		case ICONSOLE_VAR_INT16:
			*var->data.int16_ = value;
			break;
		case ICONSOLE_VAR_INT32:
			*var->data.int32_ = value;
			break;
		default:
			assert(0);
			break;
	}
}

void IConsoleVarDump(const _iconsole_var* var, const char* dump_desc)
{
	if (var == NULL) return;
	if (dump_desc == NULL) dump_desc = var->name;

	switch (var->type) {
		case ICONSOLE_VAR_BOOLEAN:
			IConsolePrintF(_iconsole_color_default, "%s = %s",
				dump_desc, *var->data.bool_ ? "true" : "false");
			break;
		case ICONSOLE_VAR_BYTE:
		case ICONSOLE_VAR_UINT8:
			IConsolePrintF(_iconsole_color_default, "%s = %u",
				dump_desc, *var->data.byte_);
			break;
		case ICONSOLE_VAR_UINT16:
			IConsolePrintF(_iconsole_color_default, "%s = %u",
				dump_desc, *var->data.uint16_);
			break;
		case ICONSOLE_VAR_UINT32:
			IConsolePrintF(_iconsole_color_default, "%s = %u",
				dump_desc, *var->data.uint32_);
			break;
		case ICONSOLE_VAR_INT16:
			IConsolePrintF(_iconsole_color_default, "%s = %i",
				dump_desc, *var->data.int16_);
			break;
		case ICONSOLE_VAR_INT32:
			IConsolePrintF(_iconsole_color_default, "%s = %i",
				dump_desc, *var->data.int32_);
			break;
		case ICONSOLE_VAR_STRING:
			IConsolePrintF(_iconsole_color_default, "%s = %s",
				dump_desc, var->data.string_);
			break;
		case ICONSOLE_VAR_REFERENCE:
			IConsolePrintF(_iconsole_color_default, "%s = @%s",
				dump_desc, var->data.reference_);
		case ICONSOLE_VAR_UNKNOWN:
		case ICONSOLE_VAR_POINTER:
			IConsolePrintF(_iconsole_color_default, "%s = @%p",
				dump_desc, var->data.addr);
			break;
		case ICONSOLE_VAR_NONE:
			IConsolePrintF(_iconsole_color_default, "%s = [nothing]",
				dump_desc);
			break;
	}
}

// * ************************* * //
// * hooking code              * //
// * ************************* * //

void IConsoleVarHook(const char* name, _iconsole_hook_types type, iconsole_var_hook proc)
{
	_iconsole_var* hook_var = IConsoleVarGet(name);
	if (hook_var == NULL) return;
	switch (type) {
		case ICONSOLE_HOOK_BEFORE_CHANGE:
			hook_var->hook_before_change = proc;
			break;
		case ICONSOLE_HOOK_AFTER_CHANGE:
			hook_var->hook_after_change = proc;
			break;
		case ICONSOLE_HOOK_ACCESS:
			hook_var->hook_access = proc;
			break;
		case ICONSOLE_HOOK_BEFORE_EXEC:
		case ICONSOLE_HOOK_AFTER_EXEC:
			assert(0);
			break;
	}
}

bool IConsoleVarHookHandle(_iconsole_var* hook_var, _iconsole_hook_types type)
{
	iconsole_var_hook proc;
	if (hook_var == NULL) return false;

	proc = NULL;
	switch (type) {
		case ICONSOLE_HOOK_BEFORE_CHANGE:
			proc = hook_var->hook_before_change;
			break;
		case ICONSOLE_HOOK_AFTER_CHANGE:
			proc = hook_var->hook_after_change;
			break;
		case ICONSOLE_HOOK_ACCESS:
			proc = hook_var->hook_access;
			break;
		case ICONSOLE_HOOK_BEFORE_EXEC:
		case ICONSOLE_HOOK_AFTER_EXEC:
			assert(0);
			break;
	}
	return proc == NULL ? true : proc(hook_var);
}

void IConsoleCmdHook(const char* name, _iconsole_hook_types type, iconsole_cmd_hook proc)
{
	IConsoleCmd* hook_cmd = IConsoleCmdGet(name);
	if (hook_cmd == NULL) return;
	switch (type) {
		case ICONSOLE_HOOK_AFTER_EXEC:
			hook_cmd->hook_after_exec = proc;
			break;
		case ICONSOLE_HOOK_BEFORE_EXEC:
			hook_cmd->hook_before_exec = proc;
			break;
		case ICONSOLE_HOOK_ACCESS:
			hook_cmd->hook_access = proc;
			break;
		case ICONSOLE_HOOK_BEFORE_CHANGE:
		case ICONSOLE_HOOK_AFTER_CHANGE:
			assert(0);
			break;
	}
}

bool IConsoleCmdHookHandle(IConsoleCmd* hook_cmd, _iconsole_hook_types type)
{
	iconsole_cmd_hook proc = NULL;
	switch (type) {
		case ICONSOLE_HOOK_AFTER_EXEC:
			proc = hook_cmd->hook_after_exec;
			break;
		case ICONSOLE_HOOK_BEFORE_EXEC:
			proc = hook_cmd->hook_before_exec;
			break;
		case ICONSOLE_HOOK_ACCESS:
			proc = hook_cmd->hook_access;
			break;
		case ICONSOLE_HOOK_BEFORE_CHANGE:
		case ICONSOLE_HOOK_AFTER_CHANGE:
			assert(0);
			break;
	}
	return proc == NULL ? true : proc(hook_cmd);
}

/**
 * Execute a given command passed to us. First chop it up into
 * individual tokens, then execute it if possible
 * @param cmdstr string to be parsed and executed
 */
void IConsoleCmdExec(const char *cmdstr)
{
	IConsoleCmd   *cmd    = NULL;
	IConsoleAlias *alias  = NULL;
	_iconsole_var *var    = NULL;

	const char *cmdptr;
	char *tokens[ICON_TOKEN_COUNT], tokenstream[ICON_MAX_STREAMSIZE];
	uint t_index, tstream_i;

	bool longtoken = false;

	for (cmdptr = cmdstr; *cmdptr != '\0'; *cmdptr++) {
		if (!IsValidAsciiChar(*cmdptr)) {
			IConsoleError("command contains malformed characters, aborting");
			return;
		}
	}

	if (_stdlib_con_developer)
		IConsolePrintF(_iconsole_color_debug, "condbg: executing cmdline: %s", cmdstr);

	memset(&tokens, 0, sizeof(tokens));
	memset(&tokenstream, 0, sizeof(tokenstream));

	/* 1. Split up commandline into tokens, seperated by spaces, commands
	 * enclosed in "" are taken as one token. We can only go as far as the amount
	 * of characters in our stream or the max amount of tokens we can handle */
	tokens[0] = tokenstream;
	for (cmdptr = cmdstr, t_index = 0, tstream_i = 0; *cmdptr != '\0'; *cmdptr++) {
		if (t_index >= lengthof(tokens) || tstream_i >= lengthof(tokenstream)) break;

		switch (*cmdptr) {
		case ' ': /* Token seperator */
			if (!longtoken) {
				tokenstream[tstream_i] = '\0';
				tokens[++t_index] = &tokenstream[tstream_i + 1];
			} else
				tokenstream[tstream_i] = *cmdptr;

			tstream_i++;
			break;
		case '"': /* Tokens enclosed in "" are one token */
			longtoken = !longtoken;
			break;
		default: /* Normal character */
			tokenstream[tstream_i++] = *cmdptr;
			break;
		}
	}

	t_index++; // index was 0

	/* 2. Determine type of command (cmd, alias or variable) and execute
	 * First try commands, then aliases, and finally variables
	 */
	 cmd = IConsoleCmdGet(tokens[0]);
	 if (cmd != NULL) {
		if (IConsoleCmdHookHandle(cmd, ICONSOLE_HOOK_ACCESS)) {
			IConsoleCmdHookHandle(cmd, ICONSOLE_HOOK_BEFORE_EXEC);
			cmd->addr(t_index, tokens, NULL);
			IConsoleCmdHookHandle(cmd, ICONSOLE_HOOK_AFTER_EXEC);
		}
	 	return;
	 }

	 alias = IConsoleAliasGet(tokens[0]);
	 if (alias != NULL) {
	 	IConsoleAliasExec(alias->cmdline, &tokens[1], t_index - 1);
	 	return;
	 }

	 var = IConsoleVarGet(tokens[0]);
	 if (var != NULL) {
	 	IConsoleVarExec(tokens);
	 	return;
	 }

	 IConsoleError("command or variable not found");
}

#if 0
void IConsoleCmdExec(const char* cmdstr)
{
	IConsoleCmdAddr function;
	char* tokens[20];
	byte  tokentypes[20];
	char* tokenstream;
	char* tokenstream_s;
	byte  execution_mode;
	_iconsole_var* var     = NULL;
	_iconsole_var* result  = NULL;
	IConsoleCmd* cmd     = NULL;
	IConsoleAlias* alias = NULL;

	bool longtoken;
	bool valid_token;
	bool skip_lt_change;

	uint c;
	uint i;
	uint l;

	for (; strchr("\n\r \t", *cmdstr) != NULL; cmdstr++) {
		switch (*cmdstr) {
			case '\0': case '#': return;
			default: break;
		}
	}

	if (_stdlib_con_developer)
		IConsolePrintF(_iconsole_color_debug, "CONDEBUG: execution_cmdline: %s", cmdstr);

	//** clearing buffer **//

	for (i = 0; i < 20; i++) {
		tokens[i] = NULL;
		tokentypes[i] = ICONSOLE_VAR_NONE;
	}
	tokenstream_s = tokenstream = malloc(1024);
	memset(tokenstream, 0, 1024);

	//** parsing **//

	longtoken = false;
	valid_token = false;
	skip_lt_change = false;
	l = strlen(cmdstr);
	i = 0;
	c = 0;
	tokens[c] = tokenstream;
	tokentypes[c] = ICONSOLE_VAR_UNKNOWN;
	while (i < l && c < lengthof(tokens) - 1) {
		if (cmdstr[i] == '"') {
			if (longtoken) {
				if (cmdstr[i + 1] == '"') {
					i++;
					*tokenstream = '"';
					tokenstream++;
					skip_lt_change = true;
				} else {
					longtoken = !longtoken;
					tokentypes[c] = ICONSOLE_VAR_STRING;
				}
			} else {
				longtoken = !longtoken;
				tokentypes[c] = ICONSOLE_VAR_STRING;
			}
			if (!skip_lt_change) {
				if (!longtoken) {
					if (valid_token) {
						c++;
						*tokenstream = '\0';
						tokenstream++;
						tokens[c] = tokenstream;
						tokentypes[c] = ICONSOLE_VAR_UNKNOWN;
						valid_token = false;
					}
				}
				skip_lt_change=false;
			}
		} else if (!longtoken && cmdstr[i] == ' ') {
			if (valid_token) {
				c++;
				*tokenstream = '\0';
				tokenstream++;
				tokens[c] = tokenstream;
				tokentypes[c] = ICONSOLE_VAR_UNKNOWN;
				valid_token = false;
			}
		} else {
			valid_token = true;
			*tokenstream = cmdstr[i];
			tokenstream++;
		}
		i++;
	}

	tokenstream--;
	if (*tokenstream != '\0') {
		c++;
		tokenstream++;
		*tokenstream = '\0';
	}

	//** interpreting **//

	for (i = 0; i < c; i++) {
		if (tokens[i] != NULL && i > 0 && strlen(tokens[i]) > 0) {
			if (IConsoleVarGet((char *)tokens[i]) != NULL) {
				// change the variable to an pointer if execution_mode != 4 is
				// being prepared. execution_mode 4 is used to assign
				// one variables data to another one
				// [token 0 and 2]
				if (!((i == 2) && (tokentypes[1] == ICONSOLE_VAR_UNKNOWN) &&
					(strcmp(tokens[1], "<<") == 0))) {
					// only look for another variable if it isnt an longtoken == string with ""
					var = NULL;
					if (tokentypes[i]!=ICONSOLE_VAR_STRING) var = IConsoleVarGet(tokens[i]);
					if (var != NULL) {
						// pointer to the data --> token
						tokens[i] = (char *) var->data.addr; /* XXX: maybe someone finds an cleaner way to do this */
						tokentypes[i] = var->type;
					}
				}
			}
			if (tokens[i] != NULL && tokens[i][0] == '@' && (IConsoleVarGet(tokens[i]+1) != NULL)) {
				var = IConsoleVarGet(tokens[i]+1);
				if (var != NULL) {
					// pointer to the _iconsole_var struct --> token
					tokens[i] = (char *) var; /* XXX: maybe someone finds an cleaner way to do this */
					tokentypes[i] = ICONSOLE_VAR_REFERENCE;
				}
			}
		}
	}

	execution_mode=0;

	function = NULL;
	cmd = IConsoleCmdGet(tokens[0]);
	if (cmd != NULL) {
		function = cmd->addr;
	} else {
		alias = IConsoleAliasGet(tokens[0]);
		if (alias != NULL) execution_mode = 5; // alias handling
	}

	if (function != NULL) {
		execution_mode = 1; // this is a command
	} else {
		var = IConsoleVarGet(tokens[0]);
		if (var != NULL) {
			execution_mode = 2; // this is a variable
			if (c > 2 && strcmp(tokens[1], "<<") == 0) {
				// this is command to variable mode [normal]

				function = NULL;
				cmd = IConsoleCmdGet(tokens[2]);
				if (cmd != NULL) function = cmd->addr;

				if (function != NULL) {
					execution_mode = 3;
				} else {
					result = IConsoleVarGet(tokens[2]);
					if (result != NULL)
						execution_mode = 4;
				}
			}
		}
	}

	//** executing **//
	if (_stdlib_con_developer)
		IConsolePrintF(_iconsole_color_debug, "CONDEBUG: execution_mode: %i",
			execution_mode);
	switch (execution_mode) {
		case 0:
			// not found
			IConsoleError("command or variable not found");
			break;
		case 1:
			if (IConsoleCmdHookHandle(cmd, ICONSOLE_HOOK_ACCESS)) {
				// execution with command syntax
				IConsoleCmdHookHandle(cmd, ICONSOLE_HOOK_BEFORE_EXEC);
				result = function(c, tokens, tokentypes);
				if (result != NULL) {
					IConsoleVarDump(result, "result");
					IConsoleVarFree(result);
				}
				IConsoleCmdHookHandle(cmd, ICONSOLE_HOOK_AFTER_EXEC);
				break;
			}
		case 2:
		{
			// execution with variable syntax
			if (IConsoleVarHookHandle(var, ICONSOLE_HOOK_ACCESS) && (c == 2 || c == 3)) {
				// ** variable modifications ** //
				IConsoleVarHookHandle(var, ICONSOLE_HOOK_BEFORE_CHANGE);
				switch (var->type) {
					case ICONSOLE_VAR_BOOLEAN:
					{
						if (strcmp(tokens[1], "=") == 0) {
							if (c == 3) {
								*var->data.bool_ = (atoi(tokens[2]) != 0);
							} else {
								*var->data.bool_ = false;
							}
							IConsoleVarDump(var, NULL);
						} else if (strcmp(tokens[1], "++") == 0) {
							*var->data.bool_ = true;
							IConsoleVarDump(var, NULL);
						} else if (strcmp(tokens[1], "--") == 0) {
							*var->data.bool_ = false;
							IConsoleVarDump(var, NULL);
						}
						else
							IConsoleError("operation not supported");
						break;
					}
					case ICONSOLE_VAR_BYTE:
					case ICONSOLE_VAR_UINT8:
					{
						if (strcmp(tokens[1], "=") == 0) {
							if (c == 3)
								*var->data.byte_ = atoi(tokens[2]);
							else
								*var->data.byte_ = 0;
							IConsoleVarDump(var, NULL);
						} else if (strcmp(tokens[1], "++") == 0) {
							++*var->data.byte_;
							IConsoleVarDump(var, NULL);
						} else if (strcmp(tokens[1], "--")==0) {
							--*var->data.byte_;
							IConsoleVarDump(var, NULL);
						}
						else
							IConsoleError("operation not supported");
						break;
					}
					case ICONSOLE_VAR_UINT16:
					{
						if (strcmp(tokens[1], "=") == 0) {
							if (c == 3)
								*var->data.uint16_ = atoi(tokens[2]);
							else
								*var->data.uint16_ = 0;
							IConsoleVarDump(var, NULL);
						} else if (strcmp(tokens[1], "++") == 0) {
							++*var->data.uint16_;
							IConsoleVarDump(var, NULL);
						} else if (strcmp(tokens[1], "--") == 0) {
							--*var->data.uint16_;
							IConsoleVarDump(var, NULL);
						}
						else
							IConsoleError("operation not supported");
						break;
					}
					case ICONSOLE_VAR_UINT32:
					{
						if (strcmp(tokens[1], "=") == 0) {
							if (c == 3)
								*var->data.uint32_ = atoi(tokens[2]);
							else
								*var->data.uint32_ = 0;
							IConsoleVarDump(var, NULL);
						} else if (strcmp(tokens[1], "++") == 0) {
							++*var->data.uint32_;
							IConsoleVarDump(var, NULL);
						} else if (strcmp(tokens[1], "--") == 0) {
							--*var->data.uint32_;
							IConsoleVarDump(var, NULL);
						}
						else
							IConsoleError("operation not supported");
						break;
					}
					case ICONSOLE_VAR_INT16:
					{
						if (strcmp(tokens[1], "=") == 0) {
							if (c == 3)
								*var->data.int16_ = atoi(tokens[2]);
							else
								*var->data.int16_ = 0;
							IConsoleVarDump(var, NULL);
						} else if (strcmp(tokens[1], "++") == 0) {
							++*var->data.int16_;
							IConsoleVarDump(var, NULL);
						} else if (strcmp(tokens[1], "--") == 0) {
							--*var->data.int16_;
							IConsoleVarDump(var, NULL);
						}
						else
							IConsoleError("operation not supported");
						break;
					}
					case ICONSOLE_VAR_INT32:
					{
						if (strcmp(tokens[1], "=") == 0) {
							if (c == 3)
								*var->data.int32_ = atoi(tokens[2]);
							else
								*var->data.int32_ = 0;
							IConsoleVarDump(var, NULL);
						} else if (strcmp(tokens[1], "++") == 0) {
							++*var->data.int32_;
							IConsoleVarDump(var, NULL);
						} else if (strcmp(tokens[1], "--") == 0) {
							--*var->data.int32_;
							IConsoleVarDump(var, NULL);
						}
						else { IConsoleError("operation not supported"); }
						break;
					}
					case ICONSOLE_VAR_STRING:
					{
						if (strcmp(tokens[1], "=") == 0) {
							if (c == 3)
								IConsoleVarSetString(var, tokens[2]);
							else
								IConsoleVarSetString(var, "");
							IConsoleVarDump(var, NULL);
						}
						else
							IConsoleError("operation not supported");
						break;
					}
					case ICONSOLE_VAR_POINTER:
					{
						if (strcmp(tokens[1], "=") == 0) {
							if (c == 3) {
								if (tokentypes[2] == ICONSOLE_VAR_UNKNOWN)
									var->data.addr = (void*)atoi(tokens[2]); /* direct access on memory [by address] */
								else
									var->data.addr = (void*)tokens[2]; /* direct acces on memory [by variable] */
							} else
								var->data.addr = NULL;
							IConsoleVarDump(var, NULL);
						} else if (strcmp(tokens[1], "++") == 0) {
							++*(char*)&var->data.addr; /* change the address + 1 */
							IConsoleVarDump(var, NULL);
						} else if (strcmp(tokens[1], "--") == 0) {
							--*(char*)&var->data.addr; /* change the address - 1 */
							IConsoleVarDump(var, NULL);
						}
						else
							IConsoleError("operation not supported");
						break;
					}
					case ICONSOLE_VAR_NONE:
					case ICONSOLE_VAR_REFERENCE:
					case ICONSOLE_VAR_UNKNOWN:
						IConsoleError("operation not supported");
						break;
				}
				IConsoleVarHookHandle(var, ICONSOLE_HOOK_AFTER_CHANGE);
			}
			if (c == 1) // ** variable output ** //
				IConsoleVarDump(var, NULL);
			break;
		}
		case 3:
		case 4:
		{
			// execute command with result or assign a variable
			if (execution_mode == 3) {
				if (IConsoleCmdHookHandle(cmd, ICONSOLE_HOOK_ACCESS)) {
					int i;
					int diff;
					void* temp;
					byte temp2;

					// tokenshifting
					for (diff = 0; diff < 2; diff++) {
						temp = tokens[0];
						temp2 = tokentypes[0];
						for (i = 0; i < 19; i++) {
							tokens[i] = tokens[i + 1];
							tokentypes[i] = tokentypes[i + 1];
						}
						tokens[19] = temp;
						tokentypes[19] = temp2;
					}
					IConsoleCmdHookHandle(cmd, ICONSOLE_HOOK_BEFORE_EXEC);
					result = function(c, tokens, tokentypes);
					IConsoleCmdHookHandle(cmd, ICONSOLE_HOOK_AFTER_EXEC);
				} else
					execution_mode = 255;
			}

			if (IConsoleVarHookHandle(var, ICONSOLE_HOOK_ACCESS) && result != NULL) {
				if (result->type != var->type) {
					IConsoleError("variable type missmatch");
				} else {
					IConsoleVarHookHandle(var, ICONSOLE_HOOK_BEFORE_CHANGE);
					switch (result->type) {
						case ICONSOLE_VAR_BOOLEAN:
							*var->data.bool_ = *result->data.bool_;
							IConsoleVarDump(var, NULL);
							break;
						case ICONSOLE_VAR_BYTE:
						case ICONSOLE_VAR_UINT8:
							*var->data.byte_ = *result->data.byte_;
							IConsoleVarDump(var, NULL);
							break;
						case ICONSOLE_VAR_UINT16:
							*var->data.uint16_ = *result->data.uint16_;
							IConsoleVarDump(var, NULL);
							break;
						case ICONSOLE_VAR_UINT32:
							*var->data.uint32_ = *result->data.uint32_;
							IConsoleVarDump(var, NULL);
							break;
						case ICONSOLE_VAR_INT16:
							*var->data.int16_ = *result->data.int16_;
							IConsoleVarDump(var, NULL);
							break;
						case ICONSOLE_VAR_INT32:
							*var->data.int32_ = *result->data.int32_;
							IConsoleVarDump(var, NULL);
							break;
						case ICONSOLE_VAR_POINTER:
							var->data.addr = result->data.addr;
							IConsoleVarDump(var, NULL);
							break;
						case ICONSOLE_VAR_STRING:
							IConsoleVarSetString(var, result->data.string_);
							IConsoleVarDump(var, NULL);
							break;
						default:
							IConsoleError("variable type missmatch");
							break;
					}
					IConsoleVarHookHandle(var, ICONSOLE_HOOK_AFTER_CHANGE);
				}

				if (execution_mode == 3) {
					IConsoleVarFree(result);
				}
			}
			break;
		}
		case 5: {
			// execute an alias
			IConsoleAliasExec(alias->cmdline, tokens,tokentypes);
			}
			break;
		default:
			// execution mode invalid
			IConsoleError("invalid execution mode");
			break;
	}

	//** freeing the tokenstream **//
	free(tokenstream_s);
}
#endif
