/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "table/strings.h"
#include "functions.h"
#include "window.h"
#include "gui.h"
#include "gfx.h"
#include "player.h"
#include "variables.h"
#include "string.h"
#include <stdarg.h>
#include <string.h>
#include "console.h"
#include "network.h"
#include "network_data.h"
#include "network_server.h"

#define ICON_BUFFER 79
#define ICON_HISTORY_SIZE 20
#define ICON_LINE_HEIGHT 12
#define ICON_RIGHT_BORDERWIDTH 10
#define ICON_BOTTOM_BORDERWIDTH 12
#define ICON_MAX_ALIAS_LINES 40
#define ICON_TOKEN_COUNT 20

// ** main console ** //
static char *_iconsole_buffer[ICON_BUFFER + 1];
static uint16 _iconsole_cbuffer[ICON_BUFFER + 1];
static Textbuf _iconsole_cmdline;

// ** stdlib ** //
byte _stdlib_developer = 1;
bool _stdlib_con_developer = false;
FILE *_iconsole_output_file;

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
	SetWindowDirty(FindWindowById(WC_CONSOLE, 0));
}

static inline void IConsoleResetHistoryPos(void) {_iconsole_historypos = ICON_HISTORY_SIZE - 1;}


static void IConsoleHistoryAdd(const char *cmd);
static void IConsoleHistoryNavigate(int direction);

// ** console window ** //
static void IConsoleWndProc(Window *w, WindowEvent *e)
{
	static byte iconsole_scroll = ICON_BUFFER;

	switch (e->event) {
		case WE_PAINT: {
			int i = iconsole_scroll;
			int max = (w->height / ICON_LINE_HEIGHT) - 1;
			int delta = 0;
			GfxFillRect(w->left, w->top, w->width, w->height - 1, 0);
			while ((i > 0) && (i > iconsole_scroll - max) && (_iconsole_buffer[i] != NULL)) {
				DoDrawString(_iconsole_buffer[i], 5,
					w->height - (iconsole_scroll + 2 - i) * ICON_LINE_HEIGHT, _iconsole_cbuffer[i]);
				i--;
			}
			/* If the text is longer than the window, don't show the starting ']' */
			delta = w->width - 10 - _iconsole_cmdline.width - ICON_RIGHT_BORDERWIDTH;
			if (delta > 0) {
				DoDrawString("]", 5, w->height - ICON_LINE_HEIGHT, _icolour_cmd);
				delta = 0;
			}

			DoDrawString(_iconsole_cmdline.buf, 10 + delta, w->height - ICON_LINE_HEIGHT, _icolour_cmd);

			if (_iconsole_cmdline.caret)
				DoDrawString("_", 10 + delta + _iconsole_cmdline.caretxoffs, w->height - ICON_LINE_HEIGHT, 12);
			break;
		}
		case WE_MOUSELOOP:
			if (HandleCaret(&_iconsole_cmdline))
				SetWindowDirty(w);
			break;
		case WE_DESTROY:
			_iconsole_mode = ICONSOLE_CLOSED;
			break;
		case WE_KEYPRESS:
			e->we.keypress.cont = false;
			switch (e->we.keypress.keycode) {
				case WKC_UP:
					IConsoleHistoryNavigate(+1);
					SetWindowDirty(w);
					break;
				case WKC_DOWN:
					IConsoleHistoryNavigate(-1);
					SetWindowDirty(w);
					break;
				case WKC_SHIFT | WKC_PAGEUP:
					if (iconsole_scroll - (w->height / ICON_LINE_HEIGHT) - 1 < 0) {
						iconsole_scroll = 0;
					} else {
						iconsole_scroll -= (w->height / ICON_LINE_HEIGHT) - 1;
					}
					SetWindowDirty(w);
					break;
				case WKC_SHIFT | WKC_PAGEDOWN:
					if (iconsole_scroll + (w->height / ICON_LINE_HEIGHT) - 1 > ICON_BUFFER) {
						iconsole_scroll = ICON_BUFFER;
					} else {
						iconsole_scroll += (w->height / ICON_LINE_HEIGHT) - 1;
					}
					SetWindowDirty(w);
					break;
				case WKC_SHIFT | WKC_UP:
					if (iconsole_scroll <= 0) {
						iconsole_scroll = 0;
					} else {
						--iconsole_scroll;
					}
					SetWindowDirty(w);
					break;
				case WKC_SHIFT | WKC_DOWN:
					if (iconsole_scroll >= ICON_BUFFER) {
						iconsole_scroll = ICON_BUFFER;
					} else {
						++iconsole_scroll;
					}
					SetWindowDirty(w);
					break;
				case WKC_BACKQUOTE:
					IConsoleSwitch();
					break;
				case WKC_RETURN: case WKC_NUM_ENTER:
					IConsolePrintF(_icolour_cmd, "] %s", _iconsole_cmdline.buf);
					IConsoleHistoryAdd(_iconsole_cmdline.buf);

					IConsoleCmdExec(_iconsole_cmdline.buf);
					IConsoleClearCommand();
					break;
				case WKC_CTRL | WKC_RETURN:
					_iconsole_mode = (_iconsole_mode == ICONSOLE_FULL) ? ICONSOLE_OPENED : ICONSOLE_FULL;
					IConsoleResize(w);
					MarkWholeScreenDirty();
					break;
				case (WKC_CTRL | 'V'):
					if (InsertTextBufferClipboard(&_iconsole_cmdline)) {
						IConsoleResetHistoryPos();
						SetWindowDirty(w);
					}
					break;
				case (WKC_CTRL | 'L'):
					IConsoleCmdExec("clear");
					break;
				case (WKC_CTRL | 'U'):
					DeleteTextBufferAll(&_iconsole_cmdline);
					SetWindowDirty(w);
					break;
				case WKC_BACKSPACE: case WKC_DELETE:
					if (DeleteTextBufferChar(&_iconsole_cmdline, e->we.keypress.keycode)) {
						IConsoleResetHistoryPos();
						SetWindowDirty(w);
					}
					break;
				case WKC_LEFT: case WKC_RIGHT: case WKC_END: case WKC_HOME:
					if (MoveTextBufferPos(&_iconsole_cmdline, e->we.keypress.keycode)) {
						IConsoleResetHistoryPos();
						SetWindowDirty(w);
					}
					break;
				default:
					if (IsValidChar(e->we.keypress.key, CS_ALPHANUMERAL)) {
						iconsole_scroll = ICON_BUFFER;
						InsertTextBufferChar(&_iconsole_cmdline, e->we.keypress.key);
						IConsoleResetHistoryPos();
						SetWindowDirty(w);
					} else {
						e->we.keypress.cont = true;
					}
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

void IConsoleInit(void)
{
	extern const char _openttd_revision[];
	_iconsole_output_file = NULL;
	_icolour_def  =  1;
	_icolour_err  =  3;
	_icolour_warn = 13;
	_icolour_dbg  =  5;
	_icolour_cmd  =  2;
	_iconsole_historypos = ICON_HISTORY_SIZE - 1;
	_iconsole_mode = ICONSOLE_CLOSED;

#ifdef ENABLE_NETWORK /* Initialize network only variables */
	_redirect_console_to_client = 0;
#endif

	memset(_iconsole_history, 0, sizeof(_iconsole_history));
	memset(_iconsole_buffer, 0, sizeof(_iconsole_buffer));
	memset(_iconsole_cbuffer, 0, sizeof(_iconsole_cbuffer));
	_iconsole_cmdline.buf = calloc(ICON_CMDLN_SIZE, sizeof(*_iconsole_cmdline.buf)); // create buffer and zero it
	_iconsole_cmdline.maxlength = ICON_CMDLN_SIZE;

	IConsolePrintF(13, "OpenTTD Game Console Revision 7 - %s", _openttd_revision);
	IConsolePrint(12,  "------------------------------------");
	IConsolePrint(12,  "use \"help\" for more information");
	IConsolePrint(12,  "");
	IConsoleStdLibRegister();
	IConsoleClearCommand();
	IConsoleHistoryAdd("");
}

void IConsoleClearBuffer(void)
{
	uint i;
	for (i = 0; i <= ICON_BUFFER; i++) {
		free(_iconsole_buffer[i]);
		_iconsole_buffer[i] = NULL;
	}
}

static void IConsoleClear(void)
{
	free(_iconsole_cmdline.buf);
	IConsoleClearBuffer();
}

static void IConsoleWriteToLogFile(const char *string)
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
		IConsolePrintF(_icolour_def, "file output complete");
		fclose(_iconsole_output_file);
		_iconsole_output_file = NULL;
		return true;
	}

	return false;
}

void IConsoleFree(void)
{
	IConsoleClear();
	CloseConsoleLogIfActive();
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

void IConsoleSwitch(void)
{
	switch (_iconsole_mode) {
		case ICONSOLE_CLOSED: {
			Window *w = AllocateWindowDesc(&_iconsole_window_desc);
			w->height = _screen.height / 3;
			w->width = _screen.width;
			_iconsole_mode = ICONSOLE_OPENED;
			SETBIT(_no_scroll, SCROLL_CON); // override cursor arrows; the gamefield will not scroll
		} break;
		case ICONSOLE_OPENED: case ICONSOLE_FULL:
			DeleteWindowById(WC_CONSOLE, 0);
			_iconsole_mode = ICONSOLE_CLOSED;
			CLRBIT(_no_scroll, SCROLL_CON);
			break;
	}

	MarkWholeScreenDirty();
}

void IConsoleClose(void) {if (_iconsole_mode == ICONSOLE_OPENED) IConsoleSwitch();}
void IConsoleOpen(void)  {if (_iconsole_mode == ICONSOLE_CLOSED) IConsoleSwitch();}

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
void IConsolePrint(uint16 color_code, const char *string)
{
	char *str;
#ifdef ENABLE_NETWORK
	if (_redirect_console_to_client != 0) {
		/* Redirect the string to the client */
		SEND_COMMAND(PACKET_SERVER_RCON)(NetworkFindClientStateFromIndex(_redirect_console_to_client), color_code, string);
		return;
	}
#endif

	/* Create a copy of the string, strip if of colours and invalid
	 * characters and (when applicable) assign it to the console buffer */
	str = strdup(string);
	str_strip_colours(str);
	str_validate(str);

	if (_network_dedicated) {
		printf("%s\n", str);
		IConsoleWriteToLogFile(str);
		free(str); // free duplicated string since it's not used anymore
		return;
	}

	/* move up all the strings in the buffer one place and do the same for colour
	 * to accomodate for the new command/message */
	free(_iconsole_buffer[0]);
	memmove(&_iconsole_buffer[0], &_iconsole_buffer[1], sizeof(_iconsole_buffer[0]) * ICON_BUFFER);
	_iconsole_buffer[ICON_BUFFER] = str;

	memmove(&_iconsole_cbuffer[0], &_iconsole_cbuffer[1], sizeof(_iconsole_cbuffer[0]) * ICON_BUFFER);
	_iconsole_cbuffer[ICON_BUFFER] = color_code;

	IConsoleWriteToLogFile(_iconsole_buffer[ICON_BUFFER]);

	SetWindowDirty(FindWindowById(WC_CONSOLE, 0));
}

/**
 * Handle the printing of text entered into the console or redirected there
 * by any other means. Uses printf() style format, for more information look
 * at @IConsolePrint()
 */
void CDECL IConsolePrintF(uint16 color_code, const char *s, ...)
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
 * @param dbg debugging category
 * @param string debugging message
 */
void IConsoleDebug(const char *dbg, const char *string)
{
	if (_stdlib_developer > 1)
		IConsolePrintF(_icolour_dbg, "dbg: [%s] %s", dbg, string);
}

/**
 * It is possible to print warnings to the console. These are mostly
 * errors or mishaps, but non-fatal. You need at least a level 1 (developer) for
 * debugging messages to show up
 */
void IConsoleWarning(const char *string)
{
	if (_stdlib_developer > 0)
		IConsolePrintF(_icolour_warn, "WARNING: %s", string);
}

/**
 * It is possible to print error information to the console. This can include
 * game errors, or errors in general you would want the user to notice
 */
void IConsoleError(const char *string)
{
	IConsolePrintF(_icolour_err, "ERROR: %s", string);
}

/**
 * Change a string into its number representation. Supports
 * decimal and hexadecimal numbers as well as 'on'/'off' 'true'/'false'
 * @param *value the variable a successfull conversion will be put in
 * @param *arg the string to be converted
 * @return Return true on success or false on failure
 */
bool GetArgumentInteger(uint32 *value, const char *arg)
{
	char *endptr;

	if (strcmp(arg, "on") == 0 || strcmp(arg, "true") == 0) {
		*value = 1;
		return true;
	}
	if (strcmp(arg, "off") == 0 || strcmp(arg, "false") == 0) {
		*value = 0;
		return true;
	}

	*value = strtoul(arg, &endptr, 0);
	return arg != endptr;
}

// * ************************* * //
// * hooking code              * //
// * ************************* * //
/**
 * General internal hooking code that is the same for both commands and variables
 * @param hooks @IConsoleHooks structure that will be set according to
 * @param type type access trigger
 * @param proc function called when the hook criteria is met
 */
static void IConsoleHookAdd(IConsoleHooks *hooks, IConsoleHookTypes type, IConsoleHook *proc)
{
	if (hooks == NULL || proc == NULL) return;

	switch (type) {
		case ICONSOLE_HOOK_ACCESS:
			hooks->access = proc;
			break;
		case ICONSOLE_HOOK_PRE_ACTION:
			hooks->pre = proc;
			break;
		case ICONSOLE_HOOK_POST_ACTION:
			hooks->post = proc;
			break;
		default: NOT_REACHED();
	}
}

/**
 * Handle any special hook triggers. If the hook type is met check if
 * there is a function associated with that and if so, execute it
 * @param hooks @IConsoleHooks structure that will be checked
 * @param type type of hook, trigger that needs to be activated
 * @return true on a successfull execution of the hook command or if there
 * is no hook/trigger present at all. False otherwise
 */
static bool IConsoleHookHandle(const IConsoleHooks *hooks, IConsoleHookTypes type)
{
	IConsoleHook *proc = NULL;
	if (hooks == NULL) return false;

	switch (type) {
		case ICONSOLE_HOOK_ACCESS:
			proc = hooks->access;
			break;
		case ICONSOLE_HOOK_PRE_ACTION:
			proc = hooks->pre;
			break;
		case ICONSOLE_HOOK_POST_ACTION:
			proc = hooks->post;
			break;
		default: NOT_REACHED();
	}

	return (proc == NULL) ? true : proc();
}

/**
 * Add a hook to a command that will be triggered at certain points
 * @param name name of the command that the hook is added to
 * @param type type of hook that is added (ACCESS, BEFORE and AFTER change)
 * @param proc function called when the hook criteria is met
 */
void IConsoleCmdHookAdd(const char *name, IConsoleHookTypes type, IConsoleHook *proc)
{
	IConsoleCmd *cmd = IConsoleCmdGet(name);
	if (cmd == NULL) return;
	IConsoleHookAdd(&cmd->hook, type, proc);
}

/**
 * Add a hook to a variable that will be triggered at certain points
 * @param name name of the variable that the hook is added to
 * @param type type of hook that is added (ACCESS, BEFORE and AFTER change)
 * @param proc function called when the hook criteria is met
 */
void IConsoleVarHookAdd(const char *name, IConsoleHookTypes type, IConsoleHook *proc)
{
	IConsoleVar *var = IConsoleVarGet(name);
	if (var == NULL) return;
	IConsoleHookAdd(&var->hook, type, proc);
}

/**
 * Perhaps ugly macro, but this saves us the trouble of writing the same function
 * three types, just with different variables. Yes, templates would be handy. It was
 * either this define or an even more ugly void* magic function
 */
#define IConsoleAddSorted(_base, item_new, IConsoleType, type)                 \
{                                                                              \
	IConsoleType *item, *item_before;                                            \
	/* first command */                                                          \
	if (_base == NULL) {                                                         \
		_base = item_new;                                                          \
		return;                                                                    \
	}                                                                            \
                                                                               \
	item_before = NULL;                                                          \
	item = _base;                                                                \
                                                                               \
	/* BEGIN - Alphabetically insert the commands into the linked list */        \
	while (item != NULL) {                                                       \
		int i = strcmp(item->name, item_new->name);                                \
		if (i == 0) {                                                              \
			IConsoleError(type " with this name already exists; insertion aborted"); \
			free(item_new);                                                          \
			return;                                                                  \
		}                                                                          \
                                                                               \
		if (i > 0) break; /* insert at this position */                            \
                                                                               \
		item_before = item;                                                        \
		item = item->next;                                                         \
	}                                                                            \
                                                                               \
	if (item_before == NULL) {                                                   \
		_base = item_new;                                                          \
	} else {                                                                     \
		item_before->next = item_new;                                              \
  }                                                                            \
                                                                               \
	item_new->next = item;                                                       \
	/* END - Alphabetical insert */                                              \
}

/**
 * Register a new command to be used in the console
 * @param name name of the command that will be used
 * @param proc function that will be called upon execution of command
 */
void IConsoleCmdRegister(const char *name, IConsoleCmdProc *proc)
{
	char *new_cmd = strdup(name);
	IConsoleCmd *item_new = malloc(sizeof(IConsoleCmd));

	item_new->next = NULL;
	item_new->proc = proc;
	item_new->name = new_cmd;

	item_new->hook.access = NULL;
	item_new->hook.pre = NULL;
	item_new->hook.post = NULL;

	IConsoleAddSorted(_iconsole_cmds, item_new, IConsoleCmd, "a command");
}

/**
 * Find the command pointed to by its string
 * @param name command to be found
 * @return return Cmdstruct of the found command, or NULL on failure
 */
IConsoleCmd *IConsoleCmdGet(const char *name)
{
	IConsoleCmd *item;

	for (item = _iconsole_cmds; item != NULL; item = item->next) {
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
	char *new_alias = strdup(name);
	char *cmd_aliased = strdup(cmd);
	IConsoleAlias *item_new = malloc(sizeof(IConsoleAlias));

	item_new->next = NULL;
	item_new->cmdline = cmd_aliased;
	item_new->name = new_alias;

	IConsoleAddSorted(_iconsole_aliases, item_new, IConsoleAlias, "an alias");
}

/**
 * Find the alias pointed to by its string
 * @param name alias to be found
 * @return return Aliasstruct of the found alias, or NULL on failure
 */
IConsoleAlias *IConsoleAliasGet(const char *name)
{
	IConsoleAlias* item;

	for (item = _iconsole_aliases; item != NULL; item = item->next) {
		if (strcmp(item->name, name) == 0) return item;
	}

	return NULL;
}

/** copy in an argument into the aliasstream */
static inline int IConsoleCopyInParams(char *dst, const char *src, uint bufpos)
{
	int len = min(ICON_MAX_STREAMSIZE - bufpos, (uint)strlen(src));
	strncpy(dst, src, len);

	return len;
}

/**
 * An alias is just another name for a command, or for more commands
 * Execute it as well.
 * @param *alias is the alias of the command
 * @param tokencount the number of parameters passed
 * @param *tokens are the parameters given to the original command (0 is the first param)
 */
static void IConsoleAliasExec(const IConsoleAlias *alias, byte tokencount, char *tokens[ICON_TOKEN_COUNT])
{
	const char *cmdptr;
	char *aliases[ICON_MAX_ALIAS_LINES], aliasstream[ICON_MAX_STREAMSIZE];
	uint i;
	uint a_index, astream_i;

	memset(&aliases, 0, sizeof(aliases));
	memset(&aliasstream, 0, sizeof(aliasstream));

	if (_stdlib_con_developer)
		IConsolePrintF(_icolour_dbg, "condbg: requested command is an alias; parsing...");

	aliases[0] = aliasstream;
	for (cmdptr = alias->cmdline, a_index = 0, astream_i = 0; *cmdptr != '\0'; cmdptr++) {
		if (a_index >= lengthof(aliases) || astream_i >= lengthof(aliasstream)) break;

		switch (*cmdptr) {
		case '\'': /* ' will double for "" */
			aliasstream[astream_i++] = '"';
			break;
		case ';': /* Cmd seperator, start new command */
			aliasstream[astream_i] = '\0';
			aliases[++a_index] = &aliasstream[++astream_i];
			cmdptr++;
			break;
		case '%': /* Some or all parameters */
			cmdptr++;
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
				default: { /* One specific parameter: %A = [param 1] %B = [param 2] ... */
				int param = *cmdptr - 'A';

				if (param < 0 || param >= tokencount) {
					IConsoleError("too many or wrong amount of parameters passed to alias, aborting");
					IConsolePrintF(_icolour_warn, "Usage of alias '%s': %s", alias->name, alias->cmdline);
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

	for (i = 0; i <= a_index; i++) IConsoleCmdExec(aliases[i]); // execute each alias in turn
}

/**
 * Special function for adding string-type variables. They in addition
 * also need a 'size' value saying how long their string buffer is.
 * @param size the length of the string buffer
 * For more information see @IConsoleVarRegister()
 */
void IConsoleVarStringRegister(const char *name, void *addr, uint32 size, const char *help)
{
	IConsoleVar *var;
	IConsoleVarRegister(name, addr, ICONSOLE_VAR_STRING, help);
	var = IConsoleVarGet(name);
	var->size = size;
}

/**
 * Register a new variable to be used in the console
 * @param name name of the variable that will be used
 * @param addr memory location the variable will point to
 * @param help the help string shown for the variable
 * @param type the type of the variable (simple atomic) so we know which values it can get
 */
void IConsoleVarRegister(const char *name, void *addr, IConsoleVarTypes type, const char *help)
{
	char *new_cmd = strdup(name);
	IConsoleVar *item_new = malloc(sizeof(IConsoleVar));

	item_new->help = (help != NULL) ? strdup(help) : NULL;

	item_new->next = NULL;
	item_new->name = new_cmd;
	item_new->addr = addr;
	item_new->proc = NULL;
	item_new->type = type;

	item_new->hook.access = NULL;
	item_new->hook.pre = NULL;
	item_new->hook.post = NULL;

	IConsoleAddSorted(_iconsole_vars, item_new, IConsoleVar, "a variable");
}

/**
 * Find the variable pointed to by its string
 * @param name variable to be found
 * @return return Varstruct of the found variable, or NULL on failure
 */
IConsoleVar *IConsoleVarGet(const char *name)
{
	IConsoleVar *item;
	for (item = _iconsole_vars; item != NULL; item = item->next) {
		if (strcmp(item->name, name) == 0) return item;
	}

	return NULL;
}

/**
 * Set a new value to a console variable
 * @param *var the variable being set/changed
 * @param value the new value given to the variable, cast properly
 */
static void IConsoleVarSetValue(const IConsoleVar *var, uint32 value)
{
	IConsoleHookHandle(&var->hook, ICONSOLE_HOOK_PRE_ACTION);
	switch (var->type) {
		case ICONSOLE_VAR_BOOLEAN:
			*(bool*)var->addr = (value != 0);
			break;
		case ICONSOLE_VAR_BYTE:
			*(byte*)var->addr = (byte)value;
			break;
		case ICONSOLE_VAR_UINT16:
			*(uint16*)var->addr = (uint16)value;
			break;
		case ICONSOLE_VAR_INT16:
			*(int16*)var->addr = (int16)value;
			break;
		case ICONSOLE_VAR_UINT32:
			*(uint32*)var->addr = (uint32)value;
			break;
		case ICONSOLE_VAR_INT32:
			*(int32*)var->addr = (int32)value;
			break;
		default: NOT_REACHED();
	}

	IConsoleHookHandle(&var->hook, ICONSOLE_HOOK_POST_ACTION);
	IConsoleVarPrintSetValue(var);
}

/**
 * Set a new value to a string-type variable. Basically this
 * means to copy the new value over to the container.
 * @param *var the variable in question
 * @param *value the new value
 */
static void IConsoleVarSetStringvalue(const IConsoleVar *var, const char *value)
{
	if (var->type != ICONSOLE_VAR_STRING || var->addr == NULL) return;

	IConsoleHookHandle(&var->hook, ICONSOLE_HOOK_PRE_ACTION);
	ttd_strlcpy(var->addr, value, var->size);
	IConsoleHookHandle(&var->hook, ICONSOLE_HOOK_POST_ACTION);
	IConsoleVarPrintSetValue(var); // print out the new value, giving feedback
	return;
}

/**
 * Query the current value of a variable and return it
 * @param *var the variable queried
 * @return current value of the variable
 */
static uint32 IConsoleVarGetValue(const IConsoleVar *var)
{
	uint32 result = 0;

	switch (var->type) {
		case ICONSOLE_VAR_BOOLEAN:
			result = *(bool*)var->addr;
			break;
		case ICONSOLE_VAR_BYTE:
			result = *(byte*)var->addr;
			break;
		case ICONSOLE_VAR_UINT16:
			result = *(uint16*)var->addr;
			break;
		case ICONSOLE_VAR_INT16:
			result = *(int16*)var->addr;
			break;
		case ICONSOLE_VAR_UINT32:
			result = *(uint32*)var->addr;
			break;
		case ICONSOLE_VAR_INT32:
			result = *(int32*)var->addr;
			break;
		default: NOT_REACHED();
	}
	return result;
}

/**
 * Get the value of the variable and put it into a printable
 * string form so we can use it for printing
 */
static char *IConsoleVarGetStringValue(const IConsoleVar *var)
{
	static char tempres[50];
	char *value = tempres;

	switch (var->type) {
		case ICONSOLE_VAR_BOOLEAN:
			snprintf(tempres, sizeof(tempres), "%s", (*(bool*)var->addr) ? "on" : "off");
			break;
		case ICONSOLE_VAR_BYTE:
			snprintf(tempres, sizeof(tempres), "%u", *(byte*)var->addr);
			break;
		case ICONSOLE_VAR_UINT16:
			snprintf(tempres, sizeof(tempres), "%u", *(uint16*)var->addr);
			break;
		case ICONSOLE_VAR_UINT32:
			snprintf(tempres, sizeof(tempres), "%u",  *(uint32*)var->addr);
			break;
		case ICONSOLE_VAR_INT16:
			snprintf(tempres, sizeof(tempres), "%i", *(int16*)var->addr);
			break;
		case ICONSOLE_VAR_INT32:
			snprintf(tempres, sizeof(tempres), "%i",  *(int32*)var->addr);
			break;
		case ICONSOLE_VAR_STRING:
			value = (char*)var->addr;
			break;
		default: NOT_REACHED();
	}

	return value;
}

/**
 * Print out the value of the variable when asked
 */
void IConsoleVarPrintGetValue(const IConsoleVar *var)
{
	char *value;
	/* Some variables need really specific handling, handle this in its
	 * callback function */
	if (var->proc != NULL) {
		var->proc(0, NULL);
		return;
	}

	value = IConsoleVarGetStringValue(var);
	IConsolePrintF(_icolour_warn, "Current value for '%s' is:  %s", var->name, value);
}

/**
 * Print out the value of the variable after it has been assigned
 * a new value, thus giving us feedback on the action
 */
void IConsoleVarPrintSetValue(const IConsoleVar *var)
{
	char *value = IConsoleVarGetStringValue(var);
	IConsolePrintF(_icolour_warn, "'%s' changed to:  %s", var->name, value);
}

/**
 * Execute a variable command. Without any parameters, print out its value
 * with parameters it assigns a new value to the variable
 * @param *var the variable that we will be querying/changing
 * @param tokencount how many additional parameters have been given to the commandline
 * @param *token the actual parameters the variable was called with
 */
void IConsoleVarExec(const IConsoleVar *var, byte tokencount, char *token[ICON_TOKEN_COUNT])
{
	const char *tokenptr = token[0];
	byte t_index = tokencount;
	uint32 value;

	if (_stdlib_con_developer)
		IConsolePrintF(_icolour_dbg, "condbg: requested command is a variable");

	if (tokencount == 0) { /* Just print out value */
		IConsoleVarPrintGetValue(var);
		return;
	}

	/* Use of assignment sign is not mandatory but supported, so just 'ignore it appropiately' */
	if (strcmp(tokenptr, "=") == 0) tokencount--;

	if (tokencount == 1) {
		/* Some variables need really special handling, handle it in their callback procedure */
		if (var->proc != NULL) {
			var->proc(tokencount, &token[t_index - tokencount]); // set the new value
			return;
		}
		/* Strings need special processing. No need to convert the argument to
		 * an integer value, just copy over the argument on a one-by-one basis */
		if (var->type == ICONSOLE_VAR_STRING) {
			IConsoleVarSetStringvalue(var, token[t_index - tokencount]);
			return;
		} else if (GetArgumentInteger(&value, token[t_index - tokencount])) {
			IConsoleVarSetValue(var, value);
			return;
		}

		/* Increase or decrease the value by one. This of course can only happen to 'number' types */
		if (strcmp(tokenptr, "++") == 0 && var->type != ICONSOLE_VAR_STRING) {
			IConsoleVarSetValue(var, IConsoleVarGetValue(var) + 1);
			return;
		}

		if (strcmp(tokenptr, "--") == 0 && var->type != ICONSOLE_VAR_STRING) {
			IConsoleVarSetValue(var, IConsoleVarGetValue(var) - 1);
			return;
		}
	}

	IConsoleError("invalid variable assignment");
}

/**
 * Add a callback function to the variable. Some variables need
 * very special processing, which can only be done with custom code
 * @param name name of the variable the callback function is added to
 * @param proc the function called
 */
void IConsoleVarProcAdd(const char *name, IConsoleCmdProc *proc)
{
	IConsoleVar *var = IConsoleVarGet(name);
	if (var == NULL) return;
	var->proc = proc;
}

/**
 * Execute a given command passed to us. First chop it up into
 * individual tokens (seperated by spaces), then execute it if possible
 * @param cmdstr string to be parsed and executed
 */
void IConsoleCmdExec(const char *cmdstr)
{
	IConsoleCmd   *cmd    = NULL;
	IConsoleAlias *alias  = NULL;
	IConsoleVar   *var    = NULL;

	const char *cmdptr;
	char *tokens[ICON_TOKEN_COUNT], tokenstream[ICON_MAX_STREAMSIZE];
	uint t_index, tstream_i;

	bool longtoken = false;
	bool foundtoken = false;

	if (cmdstr[0] == '#') return; // comments

	for (cmdptr = cmdstr; *cmdptr != '\0'; cmdptr++) {
		if (!IsValidChar(*cmdptr, CS_ALPHANUMERAL)) {
			IConsoleError("command contains malformed characters, aborting");
			IConsolePrintF(_icolour_err, "ERROR: command was: '%s'", cmdstr);
			return;
		}
	}

	if (_stdlib_con_developer)
		IConsolePrintF(_icolour_dbg, "condbg: executing cmdline: '%s'", cmdstr);

	memset(&tokens, 0, sizeof(tokens));
	memset(&tokenstream, 0, sizeof(tokenstream));

	/* 1. Split up commandline into tokens, seperated by spaces, commands
	 * enclosed in "" are taken as one token. We can only go as far as the amount
	 * of characters in our stream or the max amount of tokens we can handle */
	for (cmdptr = cmdstr, t_index = 0, tstream_i = 0; *cmdptr != '\0'; cmdptr++) {
		if (t_index >= lengthof(tokens) || tstream_i >= lengthof(tokenstream)) break;

		switch (*cmdptr) {
		case ' ': /* Token seperator */
			if (!foundtoken) break;

			if (longtoken) {
				tokenstream[tstream_i] = *cmdptr;
			} else {
				tokenstream[tstream_i] = '\0';
				foundtoken = false;
			}

			tstream_i++;
			break;
		case '"': /* Tokens enclosed in "" are one token */
			longtoken = !longtoken;
			break;
		case '\\': /* Escape character for "" */
			if (cmdptr[1] == '"' && tstream_i + 1 < lengthof(tokenstream)) {
				tokenstream[tstream_i++] = *++cmdptr;
				break;
			}
			/* fallthrough */
		default: /* Normal character */
			tokenstream[tstream_i++] = *cmdptr;

			if (!foundtoken) {
				tokens[t_index++] = &tokenstream[tstream_i - 1];
				foundtoken = true;
			}
			break;
		}
	}

	if (_stdlib_con_developer) {
		uint i;

		for (i = 0; tokens[i] != NULL; i++) {
			IConsolePrintF(_icolour_dbg, "condbg: token %d is: '%s'", i, tokens[i]);
		}
	}

	if (tokens[0] == '\0') return; // don't execute empty commands
	/* 2. Determine type of command (cmd, alias or variable) and execute
	 * First try commands, then aliases, and finally variables. Execute
	 * the found action taking into account its hooking code
	 */
	cmd = IConsoleCmdGet(tokens[0]);
	if (cmd != NULL) {
		if (IConsoleHookHandle(&cmd->hook, ICONSOLE_HOOK_ACCESS)) {
			IConsoleHookHandle(&cmd->hook, ICONSOLE_HOOK_PRE_ACTION);
			if (cmd->proc(t_index, tokens)) { // index started with 0
				IConsoleHookHandle(&cmd->hook, ICONSOLE_HOOK_POST_ACTION);
			} else {
				cmd->proc(0, NULL); // if command failed, give help
			}
		}
		return;
	}

	t_index--; // ignore the variable-name for comfort for both aliases and variaables
	alias = IConsoleAliasGet(tokens[0]);
	if (alias != NULL) {
		IConsoleAliasExec(alias, t_index, &tokens[1]);
		return;
	}

	var = IConsoleVarGet(tokens[0]);
	if (var != NULL) {
		if (IConsoleHookHandle(&var->hook, ICONSOLE_HOOK_ACCESS)) {
			IConsoleVarExec(var, t_index, &tokens[1]);
		}
		return;
	}

	IConsoleError("command or variable not found");
}
