#include "stdafx.h"
#include "ttd.h"
#include "window.h"
#include "gui.h"
#include "gfx.h"
#include "player.h"
#include "variables.h"
#include "hal.h"
#include <stdarg.h>
#include "console.h"

// ** main console ** //
static bool _iconsole_inited;
static byte* _iconsole_buffer[80];
static byte _iconsole_cbuffer[80];
static byte _iconsole_cmdline[255];
static byte _iconsole_cmdpos;
static byte _iconsole_mode = ICONSOLE_CLOSED;
static Window *_iconsole_win = NULL;
static byte _iconsole_scroll;

// ** console cursor ** //
static bool _icursor_state;
static byte _icursor_rate;
static byte _icursor_counter;

// ** stdlib ** //
byte _stdlib_developer=1;
bool _stdlib_con_developer=false;

// ** main console cmd buffer ** // sign_de: especialy for Celestar :D
static byte* _iconsole_cmdbuffer[20];
static byte _iconsole_cmdbufferpos;

// ** console window ** //
static void IConsoleWndProc(Window *w, WindowEvent *e);
static const Widget _iconsole_window_widgets[] = {{WIDGETS_END}};
static const WindowDesc _iconsole_window_desc = {
	0, 0, 2, 2,
	WC_CONSOLE,0,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_iconsole_window_widgets,
	IConsoleWndProc,
};

/* *************** */
/*  end of header  */
/* *************** */

void IConsoleClearCommand()
{
int i;
for (i=0; i<255; i++) _iconsole_cmdline[i]=0;
_iconsole_cmdpos=0;
SetWindowDirty(_iconsole_win);
}

static void IConsoleWndProc(Window *w, WindowEvent *e)
{
	// only do window events with the console
	w = FindWindowById(WC_CONSOLE, 0);

	switch(e->event) {

	case WE_PAINT:
		GfxFillRect(w->left,w->top,w->width,w->height-1,0);
		{
		int i=_iconsole_scroll;
		int max=(w->height/12)-1;
		while ((i>_iconsole_scroll-max) && (_iconsole_buffer[i]!=NULL)) {
			DoDrawString(_iconsole_buffer[i],5,w->height-(((_iconsole_scroll+2)-i)*12),_iconsole_cbuffer[i]);
			i--;
			}
		DoDrawString("]",5,w->height-12,_iconsole_color_commands);
		DoDrawString((char *)&_iconsole_cmdline,10,w->height-12,_iconsole_color_commands);
		}
		break;

	case WE_TICK:

		_icursor_counter++;
		if (_icursor_counter>_icursor_rate) {
			_icursor_state=!_icursor_state;
			{
				int posx;
				int posy;
				int color;
				_cur_dpi=&_screen;
				if (_icursor_state) color=14; else color=0;
				posx=10+GetStringWidth((char *)&_iconsole_cmdline);
				posy=w->height-3;
				GfxFillRect(posx,posy,posx+5,posy+1,color);
				_video_driver->make_dirty(posx,posy,5,1);
			}
			_icursor_counter=0;
			}
		break;

	case WE_DESTROY:
		_iconsole_win=NULL;
		_iconsole_mode=ICONSOLE_CLOSED;
		break;

	case WE_KEYPRESS:
		e->keypress.cont=false;
		if (e->keypress.keycode == (WKC_UP))
			{
			IConsoleCmdBufferNavigate(+1);
			SetWindowDirty(w);
			} else
		if (e->keypress.keycode == (WKC_DOWN))
			{
			IConsoleCmdBufferNavigate(-1);
			SetWindowDirty(w);
			} else
		if (e->keypress.keycode == (WKC_SHIFT | WKC_PAGEUP))
			{
			if ((_iconsole_scroll - ((w->height/12)-1))<0) {
				_iconsole_scroll = 0;
				} else {
				_iconsole_scroll -= (w->height/12)-1;
				}
			SetWindowDirty(w);
			} else
		if (e->keypress.keycode == (WKC_SHIFT | WKC_PAGEDOWN))
			{
			if ((_iconsole_scroll + ((w->height/12)-1))>79) {
				_iconsole_scroll = 79;
				} else {
				_iconsole_scroll += (w->height/12)-1;
				}
			SetWindowDirty(w);
			} else
		if (e->keypress.keycode == (WKC_SHIFT | WKC_UP))
			{
			if ((_iconsole_scroll - 1)<0) {
				_iconsole_scroll = 0;
				} else {
				_iconsole_scroll -= 1;
				}
			SetWindowDirty(w);
			} else
		if (e->keypress.keycode == (WKC_SHIFT | WKC_DOWN))
			{
			if ((_iconsole_scroll + 1)>79) {
				_iconsole_scroll = 79;
				} else {
				_iconsole_scroll += 1;
				}
			SetWindowDirty(w);
			} else
		if (e->keypress.keycode == WKC_BACKQUOTE)
			{
			IConsoleSwitch();
			} else
		if (e->keypress.keycode == WKC_RETURN)
			{
			IConsolePrintF(_iconsole_color_commands, "] %s", _iconsole_cmdline);
			IConsoleCmdBufferAdd(_iconsole_cmdline);
			IConsoleCmdExec((byte *) _iconsole_cmdline);
			IConsoleClearCommand();
			} else
		if (e->keypress.keycode == WKC_BACKSPACE)
			{
			if (_iconsole_cmdpos!=0) _iconsole_cmdpos--;
			_iconsole_cmdline[_iconsole_cmdpos]=0;
			SetWindowDirty(w);
			_iconsole_cmdbufferpos=19;
			} else
		if (IS_INT_INSIDE((e->keypress.ascii), 32, 256))
			{
			_iconsole_scroll=79;
			_iconsole_cmdline[_iconsole_cmdpos]=e->keypress.ascii;
			if (_iconsole_cmdpos!=255) _iconsole_cmdpos++;
			SetWindowDirty(w);
			_iconsole_cmdbufferpos=19;
			} else e->keypress.cont=true;
		break;

	}
}

void IConsoleInit()
{
	int i;
	#if defined(WITH_REV)
	extern char _openttd_revision[];
	#endif
	_iconsole_color_default = 1;
	_iconsole_color_error = 3;
	_iconsole_color_debug = 5;
	_iconsole_color_commands = 2;
	_iconsole_scroll=79;
	_iconsole_cmdbufferpos=19;
	_iconsole_inited=true;
	_iconsole_mode=ICONSOLE_CLOSED;
	_iconsole_win=NULL;
	_icursor_state=false;
	_icursor_rate=5;
	_icursor_counter=0;
	for (i=0;i<20;i++) {
		_iconsole_cmdbuffer[i]=NULL;
		}
	for (i=0;i<80;i++) {
		_iconsole_buffer[i]=NULL;
		_iconsole_cbuffer[i]=0;
		}
	IConsoleStdLibRegister();
	#if defined(WITH_REV)
	IConsolePrintF(13,"OpenTTD Game Console Revision 4 - %s",_openttd_revision);
	#else
	IConsolePrint(13,"OpenTTD Game Console Revision 4");
	#endif
	IConsolePrint(12,"---------------------------------");
	IConsolePrint(12,"use \"help\" for more info");
	IConsolePrint(12,"");
	IConsoleClearCommand();
	IConsoleCmdBufferAdd("");
}

void IConsoleClear()
{
	int i;
	for (i=0;i<80;i++) if (_iconsole_buffer[i]!=NULL) {
		free(_iconsole_buffer[i]);
		}
}

void IConsoleFree()
{
	_iconsole_inited=false;
	IConsoleClear();
}

void IConsoleResize()
{
	if (_iconsole_win!=NULL) {
		_iconsole_win->height = _screen.height / 3;
		_iconsole_win->width= _screen.width;
	}
}

void IConsoleSwitch()
{
	if (_iconsole_mode==ICONSOLE_CLOSED) {
		_iconsole_win = AllocateWindowDesc(&_iconsole_window_desc);
		_iconsole_win->height = _screen.height / 3;
		_iconsole_win->width= _screen.width;
		_iconsole_mode=ICONSOLE_OPENED;
		} else
	if (_iconsole_mode==ICONSOLE_OPENED) {
		DeleteWindowById(WC_CONSOLE,0);
		_iconsole_win=NULL;
		_iconsole_mode=ICONSOLE_CLOSED;
		}
	MarkWholeScreenDirty();
	MarkAllViewportsDirty(0,0,_screen.width,_screen.height);
	_video_driver->make_dirty(0,0,_screen.width,_screen.height);
}

void IConsoleClose()
{
	if (_iconsole_mode==ICONSOLE_OPENED)  IConsoleSwitch();
	_iconsole_mode=ICONSOLE_CLOSED;
}

void IConsoleOpen()
{
	if (_iconsole_mode==ICONSOLE_CLOSED) IConsoleSwitch();
}

void IConsoleCmdBufferAdd(const byte * cmd)
{
	int i;
	if (_iconsole_cmdbufferpos != 19) return;
	if (_iconsole_cmdbuffer[18]!=NULL) free(_iconsole_cmdbuffer[18]);
	for (i=18; i>0; i--) _iconsole_cmdbuffer[i]=_iconsole_cmdbuffer[i-1];
	i=strlen(cmd);
	_iconsole_cmdbuffer[0]=malloc(i+1);
	memset(((void *)_iconsole_cmdbuffer[0]),0,i+1);
	memcpy(((void *)_iconsole_cmdbuffer[0]),cmd,i);
	_iconsole_cmdbuffer[0][i]=0;
	_iconsole_cmdbufferpos = 19;
}

void IConsoleCmdBufferNavigate(signed char direction)
{
	int i;
	i=_iconsole_cmdbufferpos + direction;
	if (i<0) i=19;
	if (i>19) i=0;
	if (direction>0) while (_iconsole_cmdbuffer[i]==NULL) {
		i++;
		if (i>19) i=0;
		}
	if (direction<0) while (_iconsole_cmdbuffer[i]==NULL) {
		i--;
		if (i<0) i=19;
		}
	_iconsole_cmdbufferpos = i;
	IConsoleClearCommand();
	memcpy((void *)_iconsole_cmdline,(void *)_iconsole_cmdbuffer[i],strlen(_iconsole_cmdbuffer[i]));
	_iconsole_cmdpos =strlen(_iconsole_cmdbuffer[i]);
}

void IConsolePrint(byte color_code, const byte* string)
{
	byte * _ex;
	byte * _new;
	byte _exc;
	byte _newc;
	int i,j;

	if (!_iconsole_inited) return;

	_newc=color_code;
	i=strlen(string);
	_new=malloc(i+1);
	memset(_new,0,i+1);
	memcpy(_new,string,i);

	for (j=0;j<i;j++) {
		if (_new[j]<0x1F) _new[j]=0x20;
		}

	i=79;
	while (i>=0) {
		_ex=_iconsole_buffer[i];
		_exc=_iconsole_cbuffer[i];
		_iconsole_buffer[i]=_new;
		_iconsole_cbuffer[i]=_newc;
		_new=_ex;
		_newc=_exc;
		i--;
		}
	if (_ex!=NULL) free(_ex);

	if (_iconsole_win!=NULL) SetWindowDirty(_iconsole_win);
}


void CDECL IConsolePrintF(byte color_code, const char *s, ...)
{
	va_list va;
	char buf[1024];
	va_start(va, s);
	vsprintf(buf, s, va);
	va_end(va);
	IConsolePrint(color_code, (byte *) &buf);
}

void IConsoleDebug(byte* string)
{
	if (_stdlib_developer>1) IConsolePrintF(_iconsole_color_debug, "DEBUG: %s", string);
}

void IConsoleError(const byte* string)
{
	if (_stdlib_developer>0) IConsolePrintF(_iconsole_color_error, "ERROR: %s", string);
}

void IConsoleCmdRegister(const byte * name, void * addr)
{
	byte * _new;
	_iconsole_cmd * item;
	_iconsole_cmd * item_new;
	int i;

	i=strlen(name);
	_new=malloc(i+1);
	memset(_new,0,i+1);
	memcpy(_new,name,i);

	item_new = malloc(sizeof(_iconsole_cmd));

	item_new->_next = NULL;
	item_new->addr  = addr;
	item_new->name  = _new;

	item_new->hook_access = NULL;
	item_new->hook_after_exec = NULL;
	item_new->hook_before_exec = NULL;

	item = _iconsole_cmds;
	if (item == NULL) {
		_iconsole_cmds = item_new;
		} else {
		while (item->_next != NULL) { item = item->_next; };
		item->_next = item_new;
		}
}

void* IConsoleCmdGet(const byte * name)
{
	_iconsole_cmd * item;

	item = _iconsole_cmds;
	while (item != NULL) {
		if (strcmp(item->name,name)==0) return item;
		item = item->_next;
		}
	return NULL;
}

void IConsoleVarRegister(const byte * name, void * addr, byte type)
{
	byte * _new;
	_iconsole_var * item;
	_iconsole_var * item_new;
	int i;

	i=strlen(name)+1;
	_new=malloc(i+1);
	memset(_new,0,i+1);
	_new[0]='*';
	memcpy(_new+1,name,i);

	item_new = malloc(sizeof(_iconsole_var));

	item_new->_next = NULL;
	item_new->addr  = addr;
	item_new->name  = _new;
	item_new->type  = type;
	item_new->_malloc = false;

	item_new->hook_access = NULL;
	item_new->hook_after_change = NULL;
	item_new->hook_before_change = NULL;

	item = _iconsole_vars;
	if (item == NULL) {
		_iconsole_vars = item_new;
		} else {
		while (item->_next != NULL) { item = item->_next; };
		item->_next = item_new;
		}
}

void IConsoleVarMemRegister(byte * name, byte type) /* XXX TRON */
{
	_iconsole_var * item;
	item = IConsoleVarAlloc(type);
	IConsoleVarInsert(item,name);
}


void IConsoleVarInsert(_iconsole_var * var, const byte * name)
{
	byte * _new;
	_iconsole_var * item;
	_iconsole_var * item_new;
	int i;

	item_new = var;

	// dont allow to build variable rings
	if (item_new->_next != NULL) return;

	i=strlen(name)+1;
	_new=malloc(i+1);
	memset(_new,0,i+1);
	_new[0]='*';
	memcpy(_new+1,name,i);

	item_new->name  = _new;

	item = _iconsole_vars;
	if (item == NULL) {
		_iconsole_vars = item_new;
	} else {
		while (item->_next != NULL) { item = item->_next; };
		item->_next = item_new;
	}
}


_iconsole_var * IConsoleVarGet(const byte * name)
{
	_iconsole_var * item;

	item = _iconsole_vars;
	while (item != NULL) {
		if (strcmp(item->name,name)==0) return item;
		item = item->_next;
		}
	return NULL;
}

_iconsole_var * IConsoleVarAlloc(byte type)
{
	_iconsole_var * item;
	item=malloc(sizeof(_iconsole_var));
	item->_next = NULL;
	item->name  = "";
	item->type  = type;
	switch (item->type) {
			case ICONSOLE_VAR_BOOLEAN:
					{
					item->addr=malloc(sizeof(bool));
					memset(item->addr,0,sizeof(bool));
					item->_malloc=true;
					}
					break;
			case ICONSOLE_VAR_BYTE:
					{
					item->addr=malloc(sizeof(byte));
					memset(item->addr,0,sizeof(byte));
					item->_malloc=true;
					}
					break;
			case ICONSOLE_VAR_UINT16:
					{
					item->addr=malloc(sizeof(unsigned short));
					memset(item->addr,0,sizeof(unsigned short));
					item->_malloc=true;
					}
					break;
			case ICONSOLE_VAR_UINT32:
					{
					item->addr=malloc(sizeof(unsigned int));
					memset(item->addr,0,sizeof(unsigned int));
					item->_malloc=true;
					}
					break;
			case ICONSOLE_VAR_INT16:
					{
					item->addr=malloc(sizeof(signed short));
					memset(item->addr,0,sizeof(signed short));
					item->_malloc=true;
					}
					break;
			case ICONSOLE_VAR_INT32:
					{
					item->addr=malloc(sizeof(signed int));
					memset(item->addr,0,sizeof(signed int));
					item->_malloc=true;
					}
					break;
			default:
					item->addr  = NULL;
					item->_malloc = false;
					break;
			}

	item->hook_access = NULL;
	item->hook_after_change = NULL;
	item->hook_before_change = NULL;
	return item;
}


void IConsoleVarFree(_iconsole_var * var)
{
	if (var->_malloc)
		free(var->addr);
	free(var);
}

void IConsoleVarSetString(_iconsole_var * var, const byte * string)
{
	int l;

	if (string == NULL) return;

	if (var->_malloc) {
		free(var->addr);
		}

	l=strlen(string);
	var->addr=malloc(l+1);
	var->_malloc=true;
	memset(var->addr,0,l);
	memcpy(var->addr, string, l);
	((byte *)var->addr)[l]=0;
}

void IConsoleVarSetValue(_iconsole_var * var, int value) {
	switch (var->type) {
			case ICONSOLE_VAR_BOOLEAN:
					*(bool *)var->addr = (value != 0);
					break;
			case ICONSOLE_VAR_BYTE:
					*(byte *)var->addr = value;
					break;
			case ICONSOLE_VAR_UINT16:
					*(unsigned short *)var->addr = value;
					break;
			case ICONSOLE_VAR_UINT32:
					*(unsigned int *)var->addr = value;
					break;
			case ICONSOLE_VAR_INT16:
					*(signed short *)var->addr = value;
					break;
			case ICONSOLE_VAR_INT32:
					*(signed int *)var->addr = value;
					break;
			default:
					break;
	}
}

void IConsoleVarDump(_iconsole_var * var, const byte * dump_desc)
{
	byte var_b; // TYPE BYTE
	unsigned short var_ui16; // TYPE UINT16
	unsigned int var_ui32; // TYPE UINT32
	signed short var_i16; // TYPE INT16
	signed int var_i32; // TYPE INT32
	byte * var_s; // TYPE STRING

	if (dump_desc==NULL) dump_desc = var->name;

	switch (var->type) {
		case ICONSOLE_VAR_BOOLEAN:
				{
				if (*(bool *)var->addr) {
					IConsolePrintF(_iconsole_color_default, "%s = true",dump_desc);
					} else {
					IConsolePrintF(_iconsole_color_default, "%s = false",dump_desc);
					}
				}
				break;
		case ICONSOLE_VAR_BYTE:
				{
				var_b=*(byte *)var->addr;
				IConsolePrintF(_iconsole_color_default, "%s = %i",dump_desc,var_b);
				}
				break;
		case ICONSOLE_VAR_UINT16:
				{
				var_ui16=*(unsigned short *)var->addr;
				IConsolePrintF(_iconsole_color_default, "%s = %i",dump_desc,var_ui16);
				}
				break;
		case ICONSOLE_VAR_UINT32:
				{
				var_ui32=*(unsigned int *)var->addr;
				IConsolePrintF(_iconsole_color_default, "%s = %i",dump_desc,var_ui32);
				}
				break;
		case ICONSOLE_VAR_INT16:
				{
				var_i16=*(signed short *)var->addr;
				IConsolePrintF(_iconsole_color_default, "%s = %i",dump_desc,var_i16);
				}
				break;
		case ICONSOLE_VAR_INT32:
				{
				var_i32=*(signed int *)var->addr;
				IConsolePrintF(_iconsole_color_default, "%s = %i",dump_desc,var_i32);
				}
				break;
		case ICONSOLE_VAR_STRING:
				{
				var_s=(byte *)var->addr;
				IConsolePrintF(_iconsole_color_default, "%s = %s",dump_desc,var_s);
				}
				break;
		case ICONSOLE_VAR_REFERENCE:
				IConsolePrintF(_iconsole_color_default, "%s = @%s",dump_desc,((_iconsole_var *)var->addr)->name);
				break;
		case ICONSOLE_VAR_UNKNOWN:
		case ICONSOLE_VAR_POINTER:
				{
				var_i32=(signed int)((byte *)var->addr);
				IConsolePrintF(_iconsole_color_default, "%s = @%i",dump_desc,var_i32);
				}
				break;
		}
}

// * ************************* * //
// * hooking code              * //
// * ************************* * //

void IConsoleVarHook(const byte * name, byte type, void * proc)
{
	_iconsole_var * hook_var;
	hook_var = IConsoleVarGet(name);
	if (hook_var == NULL) return;
	switch (type) {
	case ICONSOLE_HOOK_BEFORE_CHANGE:
		hook_var->hook_after_change = proc;
		break;
	case ICONSOLE_HOOK_AFTER_CHANGE:
		hook_var->hook_after_change = proc;
		break;
	case ICONSOLE_HOOK_ACCESS:
		hook_var->hook_access = proc;
		break;
	}
}

bool IConsoleVarHookHandle(_iconsole_var * hook_var, byte type)
{
	bool (*proc)(_iconsole_var * hook_var) = NULL;
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
	}

	if (proc == NULL) { return true;}

	return proc(hook_var);
}

void IConsoleCmdHook(const byte * name, byte type, void * proc)
{
	_iconsole_cmd * hook_cmd;
	hook_cmd = IConsoleCmdGet(name);
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
	}
}

bool IConsoleCmdHookHandle(_iconsole_cmd * hook_cmd, byte type)
{
	bool (*proc)(_iconsole_cmd * hook_cmd) = NULL;
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
	}

	if (proc == NULL) { return true;}

	return proc(hook_cmd);
}

void IConsoleCmdExec(byte * cmdstr)
{
	_iconsole_var * (*function)(byte argc, byte* argv[], byte argt[]);
	byte * tokens[20];
	byte tokentypes[20];
	byte * tokenstream;
	byte * tokenstream_s;
	byte execution_mode;
	_iconsole_var * var = NULL;
	_iconsole_var * result = NULL;
	_iconsole_cmd * cmd = NULL;

	bool longtoken;
	bool valid_token;
	bool skip_lt_change;

	int c;
	int i;
	int l;

	//** clearing buffer **//

	for (i=0;i<20;i++) { tokens[i]=NULL; tokentypes[i]=ICONSOLE_VAR_NONE; };
	tokenstream_s=tokenstream=malloc(1024);
	memset(tokenstream,0,1024);

	//** parsing **//

	longtoken=false;
	valid_token=false;
	skip_lt_change=false;
	l=strlen((char *) cmdstr);
	i=0;
	c=0;
	tokens[c] = tokenstream;
	while (i<l) {
		if (cmdstr[i]=='"') {
			if (longtoken) {
				if (cmdstr[i+1]=='"') {
					i++;
					*tokenstream = '"';
					tokenstream++;
					skip_lt_change=true;
					} else {
					longtoken=!longtoken;
					}
				} else {
				longtoken=!longtoken;
				}
			if (!skip_lt_change) {
				if (!longtoken) {
					if (valid_token) {
						c++;
						*tokenstream = 0;
						tokenstream++;
						tokens[c] = tokenstream;
						valid_token = false;
						}
					}
				skip_lt_change=false;
				}
			}
		else if ((!longtoken) && (cmdstr[i]==' ')) {
			if (valid_token) {
				c++;
				*tokenstream = 0;
				tokenstream++;
				tokens[c] = tokenstream;
				valid_token = false;
				}
			}
		else {
			valid_token=true;
			*tokenstream = cmdstr[i];
			tokenstream++;
			}
		i++;
		}

	tokenstream--;
	if (!(*tokenstream==0)) {
		c++;
		tokenstream++;
		*tokenstream = 0;
		}

	//** interpreting **//

	for (i=0; i<c; i++) {
		tokentypes[i]=ICONSOLE_VAR_UNKNOWN;
		if (tokens[i]!=NULL) if (i>0) if (strlen((char *) tokens[i])>0) {
			if (tokens[i][0]=='*') {
				if ((i==2) && (tokentypes[1]==ICONSOLE_VAR_UNKNOWN) && (strcmp(tokens[1],"<<")==0)) {
					// dont change the variable to an pointer if execution_mode 4 is being prepared
					// this is used to assign one variable the value of the other one [token 0 and 2]
					} else {
					var = IConsoleVarGet(tokens[i]);
					if (var!=NULL) {
						tokens[i]=(byte *)var->addr;
						tokentypes[i]=var->type;
						}
					}
				}
			if (tokens[i]!=NULL) if (tokens[i][0]=='@') if (tokens[i][1]=='*') {
				var = IConsoleVarGet(tokens[i]+1);
				if (var!=NULL) {
					tokens[i]=(byte *)var;
					tokentypes[i]=ICONSOLE_VAR_REFERENCE;
					}
				}
			}
		}

	execution_mode=0;

	function = NULL;
	cmd = IConsoleCmdGet(tokens[0]);
	if (cmd != NULL) function = cmd->addr;

	if (function != NULL) {
		execution_mode=1; // this is a command
		} else {
		var = IConsoleVarGet(tokens[0]);
		if (var != NULL) {
			execution_mode=2; // this is a variable
			if (c>2) if (strcmp(tokens[1],"<<")==0) {
				// this is command to variable mode [normal]

				function = NULL;
				cmd = IConsoleCmdGet(tokens[2]);
				if (cmd != NULL) function = cmd->addr;

				if (function != NULL) {
					execution_mode=3;
					} else {
					result = IConsoleVarGet(tokens[2]);
					if (result != NULL) {
						execution_mode=4;
						}
					}
				}
			}
		}

	//** executing **//
	if (_stdlib_con_developer) IConsolePrintF(_iconsole_color_debug,"CONDEBUG: execution_mode: %i",execution_mode);
	switch (execution_mode) {
	case 0:
		{
		// not found
		IConsoleError("command or variable not found");
		}
		break;
	case 1:
		if (IConsoleCmdHookHandle(cmd,ICONSOLE_HOOK_ACCESS)) {
		// execution with command syntax
		IConsoleCmdHookHandle(cmd,ICONSOLE_HOOK_BEFORE_EXEC);
		result = function(c,tokens,tokentypes);
		if (result!=NULL) {
			IConsoleVarDump(result,"result");
			IConsoleVarFree(result);
			}
		IConsoleCmdHookHandle(cmd,ICONSOLE_HOOK_AFTER_EXEC);
		}
		break;
	case 2:
		{
		// execution with variable syntax
		if (IConsoleVarHookHandle(var,ICONSOLE_HOOK_ACCESS)) if ((c==2) || (c==3)) {
			// ** variable modifications ** //
			IConsoleVarHookHandle(var,ICONSOLE_HOOK_BEFORE_CHANGE);
			switch (var->type) {
			case ICONSOLE_VAR_BOOLEAN:
					{
					if (strcmp(tokens[1],"=")==0) {
						if (c==3) {
							*(bool *)var->addr=(atoi((char *) tokens[2])!=0);
							IConsoleVarDump(var,NULL);
							} else {
							*(bool *)var->addr=false;
							IConsoleVarDump(var,NULL);
							}
						}
					else if (strcmp(tokens[1],"++")==0) {
						*(bool *)var->addr=!*(bool *)var->addr;
						IConsoleVarDump(var,NULL);
						}
					else if (strcmp(tokens[1],"--")==0) {
						*(bool *)var->addr=!*(bool *)var->addr;
						IConsoleVarDump(var,NULL);
						}
					else { IConsoleError("operation not supported"); }
					}
					break;
			case ICONSOLE_VAR_BYTE:
					{
					if (strcmp(tokens[1],"=")==0) {
						if (c==3) {
							*(byte *)var->addr=atoi((char *) tokens[2]);
							IConsoleVarDump(var,NULL);
							} else {
							*(byte *)var->addr=0;
							IConsoleVarDump(var,NULL);
							}
						}
					else if (strcmp(tokens[1],"++")==0) {
						(*(byte *)var->addr)++;
						IConsoleVarDump(var,NULL);
						}
					else if (strcmp(tokens[1],"--")==0) {
						(*(byte *)var->addr)--;
						IConsoleVarDump(var,NULL);
						}
					else { IConsoleError("operation not supported"); }
					}
					break;
			case ICONSOLE_VAR_UINT16:
					{
					if (strcmp(tokens[1],"=")==0) {
						if (c==3) {
							*(unsigned short *)var->addr=atoi((char *) tokens[2]);
							IConsoleVarDump(var,NULL);
							} else {
							*(unsigned short *)var->addr=0;
							IConsoleVarDump(var,NULL);
							}
						}
					else if (strcmp(tokens[1],"++")==0) {
						(*(unsigned short *)var->addr)++;
						IConsoleVarDump(var,NULL);
						}
					else if (strcmp(tokens[1],"--")==0) {
						(*(unsigned short *)var->addr)--;
						IConsoleVarDump(var,NULL);
						}
					else { IConsoleError("operation not supported"); }
					}
					break;
			case ICONSOLE_VAR_UINT32:
					{
					if (strcmp(tokens[1],"=")==0) {
						if (c==3) {
							*(unsigned int *)var->addr=atoi((char *) tokens[2]);
							IConsoleVarDump(var,NULL);
							} else {
							*(unsigned int *)var->addr=0;
							IConsoleVarDump(var,NULL);
							}
						}
					else if (strcmp(tokens[1],"++")==0) {
						(*(unsigned int *)var->addr)++;
						IConsoleVarDump(var,NULL);
						}
					else if (strcmp(tokens[1],"--")==0) {
						(*(unsigned int *)var->addr)--;
						IConsoleVarDump(var,NULL);
						}
					else { IConsoleError("operation not supported"); }
					}
					break;
			case ICONSOLE_VAR_INT16:
					{
					if (strcmp(tokens[1],"=")==0) {
						if (c==3) {
							*(signed short *)var->addr=atoi((char *) tokens[2]);
							IConsoleVarDump(var,NULL);
							} else {
							*(signed short *)var->addr=0;
							IConsoleVarDump(var,NULL);
							}
						}
					else if (strcmp(tokens[1],"++")==0) {
						(*(signed short *)var->addr)++;
						IConsoleVarDump(var,NULL);
						}
					else if (strcmp(tokens[1],"--")==0) {
						(*(signed short *)var->addr)--;
						IConsoleVarDump(var,NULL);
						}
					else { IConsoleError("operation not supported"); }
					}
					break;
			case ICONSOLE_VAR_INT32:
					{
					if (strcmp(tokens[1],"=")==0) {
						if (c==3) {
							*(signed int *)var->addr=atoi((char *) tokens[2]);
							IConsoleVarDump(var,NULL);
							} else {
							*(signed int *)var->addr=0;
							IConsoleVarDump(var,NULL);
							}
						}
					else if (strcmp(tokens[1],"++")==0) {
						(*(signed int *)var->addr)++;
						IConsoleVarDump(var,NULL);
						}
					else if (strcmp(tokens[1],"--")==0) {
						(*(signed int *)var->addr)--;
						IConsoleVarDump(var,NULL);
						}
					else { IConsoleError("operation not supported"); }
					}
					break;
			case ICONSOLE_VAR_STRING:
					{
					if (strcmp(tokens[1],"=")==0) {
						if (c==3) {
							IConsoleVarSetString(var, tokens[2]);
							IConsoleVarDump(var,NULL);
							} else {
							IConsoleVarSetString(var, "");
							IConsoleVarDump(var,NULL);
							}
						}
					else { IConsoleError("operation not supported"); }
					}
					break;
			case ICONSOLE_VAR_POINTER:
					{
					if (strcmp(tokens[1],"=")==0) {
						if (c==3) {
							if (tokentypes[2]==ICONSOLE_VAR_UNKNOWN) {
								var->addr = (void *)atoi(tokens[2]);
								} else {
								var->addr = (void *)tokens[2];
								}
							IConsoleVarDump(var,NULL);
							} else {
							var->addr = NULL;
							IConsoleVarDump(var,NULL);
							}
						}
					else if (strcmp(tokens[1],"++")==0) {
						var->addr = ((char *)var->addr)+1;
						IConsoleVarDump(var,NULL);
						}
					else if (strcmp(tokens[1],"--")==0) {
						var->addr = ((char *)var->addr)-1;;
						IConsoleVarDump(var,NULL);
						}
					else { IConsoleError("operation not supported"); }
					}
					break;
				}
				IConsoleVarHookHandle(var,ICONSOLE_HOOK_AFTER_CHANGE);
		}
		if (c==1) {
			// ** variable output ** //
			IConsoleVarDump(var,NULL);
			}
		}
		break;
	case 3:
	case 4:
		{
		// execute command with result or assign a variable
			if (execution_mode==3) {
				if (IConsoleCmdHookHandle(cmd,ICONSOLE_HOOK_ACCESS)) {
					int i;
					int diff;
					void * temp;
					byte temp2;

					// tokenshifting
					for (diff=0; diff<2; diff++) {
						temp=tokens[0];
						temp2=tokentypes[0];
						for (i=1; i<20; i++) {
							tokens[i-1]=tokens[i];
							tokentypes[i-1]=tokentypes[i];
						}
						tokens[19]=temp;
						tokentypes[19]=temp2;
					}
					IConsoleCmdHookHandle(cmd,ICONSOLE_HOOK_BEFORE_EXEC);
					result = function(c,tokens,tokentypes);
					IConsoleCmdHookHandle(cmd,ICONSOLE_HOOK_AFTER_EXEC);
				} else
					execution_mode=255;
			}

		if (IConsoleVarHookHandle(var,ICONSOLE_HOOK_ACCESS)) if (result!=NULL) {
			if (result ->type != var -> type) {
				IConsoleError("variable type missmatch");
				} else {
				IConsoleVarHookHandle(var,ICONSOLE_HOOK_BEFORE_CHANGE);
				switch (result->type) {
				case ICONSOLE_VAR_BOOLEAN:
					{
					(*(bool *)var->addr)=(*(bool *)result->addr);
					IConsoleVarDump(var,NULL);
					}
					break;
				case ICONSOLE_VAR_BYTE:
					{
					(*(byte *)var->addr)=(*(byte *)result->addr);
					IConsoleVarDump(var,NULL);
					}
					break;
				case ICONSOLE_VAR_UINT16:
					{
					(*(unsigned short *)var->addr)=(*(unsigned short *)result->addr);
					IConsoleVarDump(var,NULL);
					}
					break;
				case ICONSOLE_VAR_UINT32:
					{
					(*(unsigned int *)var->addr)=(*(unsigned int *)result->addr);
					IConsoleVarDump(var,NULL);
					}
					break;
				case ICONSOLE_VAR_INT16:
					{
					(*(signed short *)var->addr)=(*(signed short *)result->addr);
					IConsoleVarDump(var,NULL);
					}
					break;
				case ICONSOLE_VAR_INT32:
					{
					(*(signed int *)var->addr)=(*(signed int *)result->addr);
					IConsoleVarDump(var,NULL);
					}
					break;
				case ICONSOLE_VAR_POINTER:
					{
					var->addr=result->addr;
					IConsoleVarDump(var,NULL);
					}
					break;
				case ICONSOLE_VAR_STRING:
					{
					IConsoleVarSetString(var,result->addr);
					IConsoleVarDump(var,NULL);
					}
					break;
				default:
					{
					IConsoleError("variable type missmatch");
					}
					break;
					}
				IConsoleVarHookHandle(var,ICONSOLE_HOOK_AFTER_CHANGE);
				}

			if (execution_mode==3) {
				IConsoleVarFree(result);
				result = NULL;
				}
			}

		}
		break;
	default:
		{
		// execution mode invalid
		IConsoleError("invalid execution mode");
		}
	}

	//** freeing the tokens **//
	for (i=0;i<20;i++) tokens[i]=NULL;
	free(tokenstream_s);

}
