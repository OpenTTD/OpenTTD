#include "stdafx.h"
#include "ttd.h"
#include "table/strings.h"
#include "window.h"
#include "gui.h"
#include "gfx.h"
#include "player.h"
#include "variables.h"
#include "hal.h"
#include <stdarg.h>
#include <string.h>
#include "console.h"
#include "network.h"

#ifdef WIN32
#include <windows.h>
#endif

#define ICON_BUFFER 79
#define ICON_CMDBUF_SIZE 20
#define ICON_CMDLN_SIZE 255
#define ICON_LINE_HEIGHT 12
#define ICON_RIGHT_BORDERWIDTH 10
#define ICON_BOTTOM_BORDERWIDTH 12
#define ICON_MAX_ALIAS_LINES 40

// ** main console ** //
static bool _iconsole_inited;
static char* _iconsole_buffer[ICON_BUFFER + 1];
static uint16 _iconsole_cbuffer[ICON_BUFFER + 1];
static char _iconsole_cmdline[ICON_CMDLN_SIZE];
static byte _iconsole_cmdpos;
static Window* _iconsole_win = NULL;
static byte _iconsole_scroll;

// ** console cursor ** //
static bool _icursor_state;
static byte _icursor_rate;
static byte _icursor_counter;

// ** stdlib ** //
byte _stdlib_developer = 1;
bool _stdlib_con_developer = false;
FILE* _iconsole_output_file;

// ** main console cmd buffer
static char* _iconsole_cmdbuffer[ICON_CMDBUF_SIZE];
static byte _iconsole_cmdbufferpos;

// ** console window ** //
static void IConsoleWndProc(Window* w, WindowEvent* e);
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

/* *************** */
/*  end of header  */
/* *************** */

static void IConsoleAppendClipboard(void)
{
#ifdef WIN32
	if (IsClipboardFormatAvailable(CF_TEXT)) {
		const char* data;
		HGLOBAL cbuf;

		OpenClipboard(NULL);
		cbuf = GetClipboardData(CF_TEXT);
		data = GlobalLock(cbuf);

		/* IS_INT_INSIDE = filter for ascii-function codes like BELL and so on [we need an special filter here later] */
		for (; (IS_INT_INSIDE(*data, ' ', 256)) && (_iconsole_cmdpos < lengthof(_iconsole_cmdline) - 1); ++data)
			_iconsole_cmdline[_iconsole_cmdpos++] = *data;

		GlobalUnlock(cbuf);
		CloseClipboard();
	}
#endif
}

static void IConsoleClearCommand(void)
{
	memset(_iconsole_cmdline, 0, sizeof(_iconsole_cmdline));
	_iconsole_cmdpos = 0;
	SetWindowDirty(_iconsole_win);
}

static void IConsoleWndProc(Window* w, WindowEvent* e)
{
	// only do window events with the console
	w = FindWindowById(WC_CONSOLE, 0);

	switch(e->event) {
		case WE_PAINT:
		{
			int i = _iconsole_scroll;
			int max = (w->height / ICON_LINE_HEIGHT) - 1;
			int delta = 0;
			GfxFillRect(w->left, w->top, w->width, w->height - 1, 0);
			while ((i > _iconsole_scroll - max) && (_iconsole_buffer[i] != NULL)) {
				DoDrawString(_iconsole_buffer[i], 5,
					w->height - (_iconsole_scroll + 2 - i) * ICON_LINE_HEIGHT, _iconsole_cbuffer[i]);
				i--;
			}
			delta = w->width - 10 - GetStringWidth(_iconsole_cmdline) - ICON_RIGHT_BORDERWIDTH;
			if (delta > 0) {
				DoDrawString("]", 5, w->height - ICON_LINE_HEIGHT, _iconsole_color_commands);
				delta = 0;
			}

			DoDrawString(_iconsole_cmdline, 10 + delta, w->height - ICON_LINE_HEIGHT, _iconsole_color_commands);
			break;
		}
		case WE_TICK:
			_icursor_counter++;
			if (_icursor_counter > _icursor_rate) {
				int posx;
				int posy;
				int delta;

				_icursor_state = !_icursor_state;

				_cur_dpi = &_screen;
				delta = w->width - 10 - GetStringWidth(_iconsole_cmdline) - ICON_RIGHT_BORDERWIDTH;
				if (delta > 0)
					delta = 0;
				posx = 10 + GetStringWidth(_iconsole_cmdline) + delta;
				posy = w->height - 3;
				GfxFillRect(posx, posy, posx + 5, posy + 1, _icursor_state ? 14 : 0);
				_video_driver->make_dirty(posx, posy, 5, 1);
				_icursor_counter = 0;
			}
			break;
		case WE_DESTROY:
			_iconsole_win = NULL;
			_iconsole_mode = ICONSOLE_CLOSED;
			break;
		case WE_KEYPRESS:
		{
			e->keypress.cont = false;
			switch (e->keypress.keycode) {
				case WKC_CTRL | 'V':
					IConsoleAppendClipboard();
					SetWindowDirty(w);
					break;
				case WKC_UP:
					IConsoleCmdBufferNavigate(+1);
					SetWindowDirty(w);
					break;
				case WKC_DOWN:
					IConsoleCmdBufferNavigate(-1);
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
				case WKC_RETURN:
					IConsolePrintF(_iconsole_color_commands, "] %s", _iconsole_cmdline);
					_iconsole_cmdbufferpos = 19;
					IConsoleCmdBufferAdd(_iconsole_cmdline);
					IConsoleCmdExec(_iconsole_cmdline);
					IConsoleClearCommand();
					break;
				case WKC_CTRL | WKC_RETURN:
					if (_iconsole_mode == ICONSOLE_FULL) {
						_iconsole_mode = ICONSOLE_OPENED;
					} else {
						_iconsole_mode = ICONSOLE_FULL;
					}
					IConsoleResize();
					MarkWholeScreenDirty();
					break;
				case WKC_BACKSPACE:
					if (_iconsole_cmdpos != 0) _iconsole_cmdpos--;
					_iconsole_cmdline[_iconsole_cmdpos] = 0;
					SetWindowDirty(w);
					_iconsole_cmdbufferpos = 19;
					break;
				default:
					/* IS_INT_INSIDE = filter for ascii-function codes like BELL and so on [we need an special filter here later] */
					if (IS_INT_INSIDE(e->keypress.ascii, ' ', 256)) {
						_iconsole_scroll = ICON_BUFFER;
						_iconsole_cmdline[_iconsole_cmdpos] = e->keypress.ascii;
						if (_iconsole_cmdpos != lengthof(_iconsole_cmdline))
							_iconsole_cmdpos++;
						SetWindowDirty(w);
						_iconsole_cmdbufferpos = ICON_CMDBUF_SIZE - 1;
					}
					else
						e->keypress.cont = true;
			}
			break;
		}
	}
}

void IConsoleInit(void)
{
	uint i;
#ifdef WITH_REV_HACK
	#define WITH_REV
	const char _openttd_revision[] = WITH_REV_HACK;
#else
	#if defined(WITH_REV)
	extern char _openttd_revision[];
	#endif
#endif
	_iconsole_output_file = NULL;
	_iconsole_color_default = 1;
	_iconsole_color_error = 3;
	_iconsole_color_warning = 13;
	_iconsole_color_debug = 5;
	_iconsole_color_commands = 2;
	_iconsole_scroll = ICON_BUFFER;
	_iconsole_cmdbufferpos = ICON_CMDBUF_SIZE - 1;
	_iconsole_inited = true;
	_iconsole_mode = ICONSOLE_CLOSED;
	_iconsole_win = NULL;
	_icursor_state = false;
	_icursor_rate = 5;
	_icursor_counter = 0;
	for (i = 0; i < lengthof(_iconsole_cmdbuffer); i++)
		_iconsole_cmdbuffer[i] = NULL;
	for (i = 0; i <= ICON_BUFFER; i++) {
		_iconsole_buffer[i] = NULL;
		_iconsole_cbuffer[i] = 0;
	}
	IConsoleStdLibRegister();
	#if defined(WITH_REV)
	IConsolePrintF(13, "OpenTTD Game Console Revision 6 - %s", _openttd_revision);
	#else
	IConsolePrint(13, "OpenTTD Game Console Revision 6");
	#endif
	IConsolePrint(12, "---------------------------------");
	IConsolePrint(12, "use \"help\" for more info");
	IConsolePrint(12, "");
	IConsoleClearCommand();
	IConsoleCmdBufferAdd("");
}

void IConsoleClear(void)
{
	uint i;
	for (i = 0; i <= ICON_BUFFER; i++)
		free(_iconsole_buffer[i]);
}

void IConsoleFree(void)
{
	_iconsole_inited = false;
	IConsoleClear();
	if (_iconsole_output_file != NULL) fclose(_iconsole_output_file);
}

void IConsoleResize(void)
{
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
			break;
	}
}

void IConsoleSwitch(void)
{
	switch (_iconsole_mode) {
		case ICONSOLE_CLOSED:
			_iconsole_win = AllocateWindowDesc(&_iconsole_window_desc);
			_iconsole_win->height = _screen.height / 3;
			_iconsole_win->width = _screen.width;
			_iconsole_mode = ICONSOLE_OPENED;
			break;
		case ICONSOLE_OPENED:
			DeleteWindowById(WC_CONSOLE, 0);
			_iconsole_win = NULL;
			_iconsole_mode = ICONSOLE_CLOSED;
			break;
		case ICONSOLE_FULL:
			DeleteWindowById(WC_CONSOLE, 0);
			_iconsole_win = NULL;
			_iconsole_mode = ICONSOLE_CLOSED;
			break;
	}
}

void IConsoleClose(void)
{
	if (_iconsole_mode == ICONSOLE_OPENED) IConsoleSwitch();
}

void IConsoleOpen(void)
{
	if (_iconsole_mode == ICONSOLE_CLOSED) IConsoleSwitch();
}

void IConsoleCmdBufferAdd(const char* cmd)
{
	int i;
	if (_iconsole_cmdbufferpos != 19) return;
	free(_iconsole_cmdbuffer[18]);
	for (i = 18; i > 0; i--) _iconsole_cmdbuffer[i] = _iconsole_cmdbuffer[i - 1];
	_iconsole_cmdbuffer[0] = strdup(cmd);
}

void IConsoleCmdBufferNavigate(signed char direction)
{
	int i;
	i = _iconsole_cmdbufferpos + direction;
	if (i < 0) i = 19;
	if (i > 19) i = 0;
	if (direction > 0)
		while (_iconsole_cmdbuffer[i] == NULL) {
			++i;
			if (i > 19) i = 0;
		}
	if (direction < 0)
		while (_iconsole_cmdbuffer[i] == NULL) {
			--i;
			if (i < 0) i = 19;
		}
	_iconsole_cmdbufferpos = i;
	IConsoleClearCommand();
	memcpy(_iconsole_cmdline, _iconsole_cmdbuffer[i],
		strlen(_iconsole_cmdbuffer[i]));
	_iconsole_cmdpos = strlen(_iconsole_cmdbuffer[i]);
}

void IConsolePrint(uint16 color_code, const char* string)
{
	char* _ex;
	char* _new;
	uint16 _exc;
	uint16 _newc;
	char* i;
	int j;

	if (_network_dedicated) {
		printf("%s\n", string);
		return;
	}

	if (!_iconsole_inited) return;

	_newc = color_code;
	_new = strdup(string);

	for (i = _new; *i != '\0'; ++i)
		if (*i < ' ') *i = ' '; /* filter for ascii-function codes like BELL and so on [we need an special filter here later] */

	for (j = ICON_BUFFER; j >= 0; --j) {
		_ex = _iconsole_buffer[j];
		_exc = _iconsole_cbuffer[j];
		_iconsole_buffer[j] = _new;
		_iconsole_cbuffer[j] = _newc;
		_new = _ex;
		_newc = _exc;
	}
	free(_ex);

	if (_iconsole_win != NULL) SetWindowDirty(_iconsole_win);
}


void CDECL IConsolePrintF(uint16 color_code, const char* s, ...)
{
	va_list va;
	char buf[1024];
	int len;

	va_start(va, s);
	len = vsnprintf(buf, sizeof(buf), s, va);
	va_end(va);

	IConsolePrint(color_code, buf);

	if (_iconsole_output_file != NULL) {
		// if there is an console output file ... also print it there
		fwrite(buf, len, 1, _iconsole_output_file);
		fwrite("\n", 1, 1, _iconsole_output_file);
	}
}

void IConsoleDebug(const char* string)
{
	if (_stdlib_developer > 1)
		IConsolePrintF(_iconsole_color_debug, "dbg: %s", string);
}

void IConsoleError(const char* string)
{
	if (_stdlib_developer > 0)
		IConsolePrintF(_iconsole_color_error, "ERROR: %s", string);
}

void IConsoleWarning(const char* string)
{
	if (_stdlib_developer > 0)
		IConsolePrintF(_iconsole_color_warning, "WARNING: %s", string);
}

void IConsoleCmdRegister(const char* name, _iconsole_cmd_addr addr)
{
	char* _new;
	_iconsole_cmd* item;
	_iconsole_cmd* item_new;
	_iconsole_cmd* item_before;

	_new = strdup(name);

	item_new = malloc(sizeof(_iconsole_cmd));

	item_new->_next = NULL;
	item_new->addr = addr;
	item_new->name = _new;

	item_new->hook_access = NULL;
	item_new->hook_after_exec = NULL;
	item_new->hook_before_exec = NULL;

	item_before = NULL;
	item = _iconsole_cmds;

	if (item == NULL) {
		_iconsole_cmds = item_new;
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
				_iconsole_cmds = item_new;
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

_iconsole_cmd* IConsoleCmdGet(const char* name)
{
	_iconsole_cmd* item;

	item = _iconsole_cmds;
	while (item != NULL) {
		if (strcmp(item->name, name) == 0) return item;
		item = item->_next;
	}
	return NULL;
}

void IConsoleAliasRegister(const char* name, const char* cmdline)
{
	char* _new;
	char* _newcmd;
	_iconsole_alias* item;
	_iconsole_alias* item_new;
	_iconsole_alias* item_before;

	_new = strdup(name);
	_newcmd = strdup(cmdline);

	item_new = malloc(sizeof(_iconsole_alias));

	item_new->_next = NULL;
	item_new->cmdline = _newcmd;
	item_new->name = _new;

	item_before = NULL;
	item = _iconsole_aliases;

	if (item == NULL) {
		_iconsole_aliases = item_new;
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
				_iconsole_aliases = item_new;
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

_iconsole_alias* IConsoleAliasGet(const char* name)
{
	_iconsole_alias* item;

	item = _iconsole_aliases;
	while (item != NULL) {
		if (strcmp(item->name, name) == 0) return item;
		item = item->_next;
	}
	return NULL;
}

void IConsoleAliasExec(const char* cmdline, char* tokens[20], byte tokentypes[20]) {
	char* lines[ICON_MAX_ALIAS_LINES];
	char* linestream;
	char* linestream_s;

	int c;
	int i;
	int l;
	int x;
	byte t;

	//** clearing buffer **//

	for (i = 0; i < 40; i++) {
		lines[0] = NULL;
	}
	linestream_s = linestream = malloc(1024*ICON_MAX_ALIAS_LINES);
	memset(linestream, 0, 1024*ICON_MAX_ALIAS_LINES);

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
			} else {
				int l2;
				t = ((byte)cmdline[i]) - 64;
				if ((t<20) && (tokens[t]!=NULL)) {
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
			i++;
			if (cmdline[i] == '\\') {
				*linestream = '\\';
				linestream++;
			} else if (cmdline[i] == '\'') {
				*linestream = '\'';
				linestream++;
			}
		} else if (cmdline[i] == '\'') {
			*linestream = '"';
			linestream++;
		} else if (cmdline[i] == ';') {
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

	for (i=0; i<c; i++)	{
		IConsoleCmdExec(lines[i]);
	}

	free(linestream_s);
}

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
	for (item = _iconsole_vars; item != NULL; item = item->_next)
		if (strcmp(item->name, name) == 0) return item;
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
	_iconsole_cmd* hook_cmd = IConsoleCmdGet(name);
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

bool IConsoleCmdHookHandle(_iconsole_cmd* hook_cmd, _iconsole_hook_types type)
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

void IConsoleCmdExec(const char* cmdstr)
{
	_iconsole_cmd_addr function;
	char* tokens[20];
	byte  tokentypes[20];
	char* tokenstream;
	char* tokenstream_s;
	byte  execution_mode;
	_iconsole_var* var     = NULL;
	_iconsole_var* result  = NULL;
	_iconsole_cmd* cmd     = NULL;
	_iconsole_alias* alias = NULL;

	bool longtoken;
	bool valid_token;
	bool skip_lt_change;

	int c;
	int i;
	int l;

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
