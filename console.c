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
static byte _iconsole_mode;
static byte _iconsole_color_default = 1;
static byte _iconsole_color_error = 3;
static byte _iconsole_color_debug = 5;
static byte _iconsole_color_commands = 2;
static Window *_iconsole_win = NULL;
static byte _iconsole_scroll;

// ** console cursor ** //
static bool _icursor_state;
static byte _icursor_rate;
static byte _icursor_counter;

// ** console window ** //

static void IConsoleWndProc(Window *w, WindowEvent *e);
static const Widget _iconsole_window_widgets[] = {{WWT_LAST}};
static const WindowDesc _iconsole_window_desc = {
	0, 0, 2, 2,
	WC_CONSOLE,0,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_iconsole_window_widgets,
	IConsoleWndProc,
};

// ** console parser ** //

static _iconsole_cmd * _iconsole_cmds; // list of registred commands
static _iconsole_var * _iconsole_vars; // list of registred vars

// ** console std lib ** // 
static byte _stdlib_developer=1;
static void IConsoleStdLibRegister();

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

		if (_iconsole_mode==ICONSOLE_OPENING) {
			_iconsole_mode=ICONSOLE_OPENED;
			}

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

	case WE_KEYPRESS:
		e->keypress.cont=false;
		if (e->keypress.keycode == WKC_PAGEUP)
			{
			if ((_iconsole_scroll - ((w->height/12)-1))<0) {
				_iconsole_scroll = 0;
				} else {
				_iconsole_scroll -= (w->height/12)-1;
				}
			SetWindowDirty(w);
			} else
		if (e->keypress.keycode == WKC_PAGEDOWN)
			{
			if ((_iconsole_scroll + ((w->height/12)-1))>79) {
				_iconsole_scroll = 79;
				} else {
				_iconsole_scroll += (w->height/12)-1;
				}
			SetWindowDirty(w);
			} else
		if (e->keypress.keycode == WKC_UP)
			{
			if ((_iconsole_scroll - 1)<0) {
				_iconsole_scroll = 0;
				} else {
				_iconsole_scroll -= 1;
				}
			SetWindowDirty(w);
			} else
		if (e->keypress.keycode == WKC_DOWN)
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
			IConsoleCmdExec((byte *) _iconsole_cmdline);
			IConsoleClearCommand();
			} else
		if (e->keypress.keycode == WKC_BACKSPACE) 
			{
			if (_iconsole_cmdpos!=0) _iconsole_cmdpos--;
			_iconsole_cmdline[_iconsole_cmdpos]=0;
			SetWindowDirty(w);
			} else
		if (IS_INT_INSIDE((e->keypress.ascii), 32, 256))
			{
			_iconsole_scroll=79;
			_iconsole_cmdline[_iconsole_cmdpos]=e->keypress.ascii;
			if (_iconsole_cmdpos!=255) _iconsole_cmdpos++;	
			SetWindowDirty(w);
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
_iconsole_scroll=79;
_iconsole_inited=true;
_iconsole_mode=ICONSOLE_CLOSED;
_iconsole_win=NULL;
_icursor_state=false;
_icursor_rate=5;
_icursor_counter=0;
for (i=0;i<80;i++) _iconsole_buffer[i]=NULL;
IConsoleStdLibRegister();
#if defined(WITH_REV)
IConsolePrintF(13,"OpenTTD Game Console %s",_openttd_revision);	
#else
IConsolePrint(13,"OpenTTD Game Console");	
#endif
IConsolePrint(12,"---------------------------------");
IConsolePrint(12,"use \"help\" for more info");
IConsolePrint(12,"");
IConsoleClearCommand();
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

void IConsoleResize() {
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
		_iconsole_mode=ICONSOLE_OPENING;
		} else
	if (_iconsole_mode==ICONSOLE_OPENED) {
		DeleteWindow(_iconsole_win);
		_iconsole_win=NULL;
		_iconsole_mode=ICONSOLE_CLOSED;
		}
	MarkWholeScreenDirty();
	MarkAllViewportsDirty(0,0,_screen.width,_screen.height);
	_video_driver->make_dirty(0,0,_screen.width,_screen.height);
}

void IConsoleClose() {
if (_iconsole_mode==ICONSOLE_OPENED)  IConsoleSwitch();
}

void IConsoleOpen() {
if (_iconsole_mode==ICONSOLE_CLOSED) IConsoleSwitch();
}

void IConsolePrint(byte color_code, byte* string)
{
byte * _ex;
byte * _new;
byte _exc;
byte _newc;
int i,j;

if (!_iconsole_inited) return;

_newc=color_code;

i=strlen((char *)string);
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


void IConsolePrintF(byte color_code, const char *s, ...)
{
	va_list va;
	char buf[1024];
	va_start(va, s);
	vsprintf(buf, s, va);
	va_end(va);
	IConsolePrint(color_code, (byte *) &buf);
}

void IConsoleDebug(byte* string) {
if (_stdlib_developer>1) IConsolePrintF(_iconsole_color_debug, "DEBUG: %s", string);
}

void IConsoleError(byte* string) {
if (_stdlib_developer>0) IConsolePrintF(_iconsole_color_error, "ERROR: %s", string);
}

void IConsoleCmdRegister(byte * name, void * addr) {
byte * _new;
_iconsole_cmd * item;
_iconsole_cmd * item_new;
int i;

	i=strlen((char *)name);
	_new=malloc(i+1);
	memset(_new,0,i+1);
	memcpy(_new,name,i);

item_new = malloc(sizeof(_iconsole_cmd));

item_new->_next = NULL;
item_new->addr  = addr;
item_new->name  = _new;

item = _iconsole_cmds;
if (item == NULL) {
	_iconsole_cmds = item_new;
	} else {
	while (item->_next != NULL) { item = item->_next; };
	item->_next = item_new;
	}
}

void* IConsoleCmdGetAddr(byte * name) {
_iconsole_cmd * item;

item = _iconsole_cmds;
while (item != NULL) { 
	if (strcmp(item->name,name)==0) return item->addr;
	item = item->_next;
	}
return NULL;
}

void IConsoleVarRegister(byte * name, void * addr, byte type) {
byte * _new;
_iconsole_var * item;
_iconsole_var * item_new;
int i;

	i=strlen((char *)name)+1;
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

item = _iconsole_vars;
if (item == NULL) {
	_iconsole_vars = item_new;
	} else {
	while (item->_next != NULL) { item = item->_next; };
	item->_next = item_new;
	}
}

void IConsoleVarInsert(_iconsole_var * var, byte * name) {
byte * _new;
_iconsole_var * item;
_iconsole_var * item_new;
int i;

item_new = var;

// dont allow to build variable rings
if (item_new->_next != NULL) return;

	i=strlen((char *)name)+1;
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


_iconsole_var * IConsoleVarGet(byte * name) {
_iconsole_var * item;

item = _iconsole_vars;
while (item != NULL) { 
	if (strcmp(item->name,name)==0) return item;
	item = item->_next;
	}
return NULL;
}

_iconsole_var * IConsoleVarAlloc(byte type) {
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
return item;
}


void IConsoleVarFree(_iconsole_var * var) {
if (var ->_malloc) {
	free(var ->addr);
	}
free(var);
}

void IConsoleVarSetString(_iconsole_var * var, byte * string) {
int l;

if (var->_malloc) {
	free(var->addr);
	}

l=strlen((char *) string);
var->addr=malloc(l+1);
var->_malloc=true;
memset(var->addr,0,l);
memcpy((void *) var->addr,(void *) string, l);
((byte *)var->addr)[l]=0;
}

void IConsoleVarSetValue(_iconsole_var * var, int value) {
switch (var->type) {
		case ICONSOLE_VAR_BOOLEAN:
				{
				(*(bool *)var->addr)=(value!=0);
				}
				break;
		case ICONSOLE_VAR_BYTE:
				{
				(*(byte *)var->addr)=value;
				}
				break;
		case ICONSOLE_VAR_UINT16:
				{
				(*(unsigned short *)var->addr)=value;
				}
				break;
		case ICONSOLE_VAR_UINT32:
				{
				(*(unsigned int *)var->addr)=value;
				}
				break;
		case ICONSOLE_VAR_INT16:
				{
				(*(signed short *)var->addr)=value;
				}
				break;
		case ICONSOLE_VAR_INT32:
				{
				(*(signed int *)var->addr)=value;
				}
				break;
		default:
				break;
		}
}

void IConsoleVarDump(_iconsole_var * var, byte * dump_desc) {

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
		case ICONSOLE_VAR_UNKNOWN:
		case ICONSOLE_VAR_POINTER:
				{
				var_i32=(signed int)((byte *)var->addr);
				IConsolePrintF(_iconsole_color_default, "%s = @%i",dump_desc,var_i32);
				}
				break;
		}

}

void IConsoleCmdExec(byte * cmdstr) {
_iconsole_var * (*function)(byte argc, byte* argv[], byte argt[]);
byte * tokens[20];
byte tokentypes[20];
byte * tokenstream;
byte * tokenstream_s;
byte execution_mode;
_iconsole_var * var = NULL;
_iconsole_var * result = NULL;

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
	if (i>0) if (strlen((char *) tokens[i])>0) {
		tokentypes[i]=ICONSOLE_VAR_UNKNOWN;
		if (tokens[i][0]=='*') {
			var = IConsoleVarGet(tokens[i]);
			if (var!=NULL) {
				tokens[i]=(byte *)var->addr;
				tokentypes[i]=var->type;
				}
			}
		}
	}

execution_mode=0;

function = IConsoleCmdGetAddr(tokens[0]);
if (function != NULL) {
	execution_mode=1; // this is a command
	} else {
	var = IConsoleVarGet(tokens[0]);
	if (var != NULL) {
		execution_mode=2; // this is a variable
		if (c>2) if (strcmp(tokens[1],"<<")==0) {
			// this is command to variable mode [normal]
			function = IConsoleCmdGetAddr(tokens[2]);
			if (function != NULL) {
				execution_mode=3;
				}
			}
		}
	}

//** executing **//

switch (execution_mode) {
case 0: 
	{
	// not found
	IConsoleError("command or variable not found");
	}
	break;
case 1:
	{
	// execution with command syntax
	result = function(c,tokens,tokentypes);
	if (result!=NULL) {
		IConsoleVarDump(result,"result");
		IConsoleVarFree(result);
		}
	}
	break;
case 2:
	{
	// execution with variable syntax
if ((c==2) || (c==3)) {
		// ** variable modifications ** //
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
	}
	if (c==1) {
		// ** variable output ** //
		IConsoleVarDump(var,NULL);
		}
	}
	break;
case 3:
	{
	// execute command with result
		{
		int i;
		int diff;
		void * temp;
		byte temp2;

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
		}

	result = function(c,tokens,tokentypes);
	if (result!=NULL) {
		if (result ->type != var -> type) {
			IConsoleError("variable type missmatch");
			} else {
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
			default:
				{
				IConsoleError("variable type missmatch");
				}
				break;
				}
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

/* **************************** */
/*   default console commands   */
/* **************************** */


static _iconsole_var * IConsoleStdLibEcho(byte argc, byte* argv[], byte argt[]) {
	if (argc<2) return NULL;
	IConsolePrint(_iconsole_color_default, argv[1]);
	return NULL;
}

static _iconsole_var * IConsoleStdLibEchoC(byte argc, byte* argv[], byte argt[]) {
	if (argc<3) return NULL;
	IConsolePrint(atoi(argv[1]), argv[2]);
	return NULL;
}

static _iconsole_var * IConsoleStdLibPrintF(byte argc, byte* argv[], byte argt[]) {
	if (argc<3) return NULL;
	IConsolePrintF(_iconsole_color_default, argv[1] ,argv[2],argv[3],argv[4],argv[5],argv[6],argv[7],argv[8],argv[9],argv[10],argv[11],argv[12],argv[13],argv[14],argv[15],argv[16],argv[17],argv[18],argv[19]);
	return NULL;
}

static _iconsole_var * IConsoleStdLibPrintFC(byte argc, byte* argv[], byte argt[]) {
	if (argc<3) return NULL;
	IConsolePrintF(atoi(argv[1]), argv[2] ,argv[3],argv[4],argv[5],argv[6],argv[7],argv[8],argv[9],argv[10],argv[11],argv[12],argv[13],argv[14],argv[15],argv[16],argv[17],argv[18],argv[19]);
	return NULL;
}

static _iconsole_var * IConsoleStdLibScreenShot(byte argc, byte* argv[], byte argt[]) {
	
	if (argc<2) {
		_make_screenshot=1;
	} else {
		if (strcmp(argv[1],"big")==0) {
			_make_screenshot=2;
			}
		}

return NULL;
}

static _iconsole_var * IConsoleStdLibDebugLevel(byte argc, byte* argv[], byte argt[]) {
	if (argc<2) return NULL;
	SetDebugString(argv[1]);
	return NULL;
}

static _iconsole_var * IConsoleStdLibExit(byte argc, byte* argv[], byte argt[]) {
	_exit_game = true;
	return NULL;
}

static _iconsole_var * IConsoleStdLibHelp(byte argc, byte* argv[], byte argt[]) {
	IConsolePrint(1		,"");
	IConsolePrint(13	," -- console help -- ");
	IConsolePrint(1		," variables: [command to list them: list_vars]");
	IConsolePrint(1		," *temp_string = \"my little \"");
	IConsolePrint(1		,"");
	IConsolePrint(1		," commands: [command to list them: list_cmds]");
	IConsolePrint(1		," [command] [\"string argument with spaces\"] [argument 2] ...");
	IConsolePrint(1		," printf \"%s world\" *temp_string");
	IConsolePrint(1		,"");
	IConsolePrint(1		," command returning a value into an variable:");
	IConsolePrint(1		," *temp_uint16 << random");
	IConsolePrint(1		,"");
return NULL;
}

static _iconsole_var * IConsoleStdLibRandom(byte argc, byte* argv[], byte argt[]) {
_iconsole_var * result;
result = IConsoleVarAlloc(ICONSOLE_VAR_UINT16);
IConsoleVarSetValue(result,rand());
return result;
}

static _iconsole_var * IConsoleStdLibListCommands(byte argc, byte* argv[], byte argt[]) {
_iconsole_cmd * item;
int l = 0;

if (argv[1]!=NULL) l = strlen((char *) argv[1]);

item = _iconsole_cmds;
while (item != NULL) { 
	if (argv[1]!=NULL) {

		if (memcmp((void *) item->name, (void *) argv[1],l)==0)
				IConsolePrintF(_iconsole_color_default,"%s",item->name);

		} else {

		IConsolePrintF(_iconsole_color_default,"%s",item->name);

		}
	item = item->_next;
	}

return NULL;
}

static _iconsole_var * IConsoleStdLibListVariables(byte argc, byte* argv[], byte argt[]) {
_iconsole_var * item;
int l = 0;

if (argv[1]!=NULL) l = strlen((char *) argv[1]);

item = _iconsole_vars;
while (item != NULL) { 
	if (argv[1]!=NULL) {

		if (memcmp((void *) item->name, (void *) argv[1],l)==0)
				IConsolePrintF(_iconsole_color_default,"%s",item->name);

		} else {

		IConsolePrintF(_iconsole_color_default,"%s",item->name);

		}
	item = item->_next;
	}

return NULL;
}

static _iconsole_var * IConsoleStdLibListDumpVariables(byte argc, byte* argv[], byte argt[]) {
_iconsole_var * item;
int l = 0;

if (argv[1]!=NULL) l = strlen((char *) argv[1]);

item = _iconsole_vars;
while (item != NULL) { 
	if (argv[1]!=NULL) {

		if (memcmp((void *) item->name, (void *) argv[1],l)==0)
				IConsoleVarDump(item,NULL);

		} else {

		IConsoleVarDump(item,NULL);

		}
	item = item->_next;
	}

return NULL;
}

static void IConsoleStdLibRegister() {
	IConsoleCmdRegister("debug_level",IConsoleStdLibDebugLevel);
	IConsoleCmdRegister("echo",IConsoleStdLibEcho);
	IConsoleCmdRegister("echoc",IConsoleStdLibEchoC);
	IConsoleCmdRegister("exit",IConsoleStdLibExit);
	IConsoleCmdRegister("help",IConsoleStdLibHelp);
	IConsoleCmdRegister("printf",IConsoleStdLibPrintF);
	IConsoleCmdRegister("printfc",IConsoleStdLibPrintFC);
	IConsoleCmdRegister("quit",IConsoleStdLibExit);
	IConsoleCmdRegister("random",IConsoleStdLibRandom);
	IConsoleCmdRegister("list_cmds",IConsoleStdLibListCommands);
	IConsoleCmdRegister("list_vars",IConsoleStdLibListVariables);
	IConsoleCmdRegister("dump_vars",IConsoleStdLibListDumpVariables);
	IConsoleCmdRegister("screenshot",IConsoleStdLibScreenShot);
	IConsoleVarRegister("cursor_rate",(void *) &_icursor_rate,ICONSOLE_VAR_BYTE);
	IConsoleVarRegister("developer",(void *) &_stdlib_developer,ICONSOLE_VAR_BYTE);
#if defined(_DEBUG)
	{
	_iconsole_var * var;
	var = IConsoleVarAlloc(ICONSOLE_VAR_BOOLEAN);
	IConsoleVarInsert(var,"temp_bool");

	var = IConsoleVarAlloc(ICONSOLE_VAR_INT16);
	IConsoleVarInsert(var,"temp_int16");
	var = IConsoleVarAlloc(ICONSOLE_VAR_INT32);
	IConsoleVarInsert(var,"temp_int32");

	var = IConsoleVarAlloc(ICONSOLE_VAR_POINTER);
	IConsoleVarInsert(var,"temp_pointer");

	var = IConsoleVarAlloc(ICONSOLE_VAR_UINT16);
	IConsoleVarInsert(var,"temp_uint16");
	var = IConsoleVarAlloc(ICONSOLE_VAR_UINT32);
	IConsoleVarInsert(var,"temp_uint32");


	var = IConsoleVarAlloc(ICONSOLE_VAR_STRING);
	IConsoleVarInsert(var,"temp_string");
	}
#endif
}


