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
static byte _stdlib_developer=0;
static void IConsoleStdLibRegister();
static byte * _stdlib_temp_string;
static byte * _stdlib_temp_pointer;
static uint32 _stdlib_temp_uint32;
static bool _stdlib_temp_bool;

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
		int i=79;
		int max=(w->height/12)-1;
		while ((i>79-max) && (_iconsole_buffer[i]!=NULL)) {
			DoDrawString(_iconsole_buffer[i],5,w->height-((81-i)*12),_iconsole_cbuffer[i]);
			i--;
			}
		DoDrawString((char *)&_iconsole_cmdline,5,w->height-12,_iconsole_color_commands);
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
				posx=5+GetStringWidth((char *)&_iconsole_cmdline);
				posy=w->height-3;
				GfxFillRect(posx,posy,posx+5,posy+1,color);
				_video_driver->make_dirty(posx,posy,5,1);
			}
			_icursor_counter=0;			
			}
		break;

	case WE_KEYPRESS:
		e->keypress.cont=false;
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
			_iconsole_cmdline[_iconsole_cmdpos]=e->keypress.ascii;
			if (_iconsole_cmdpos!=255) _iconsole_cmdpos++;	
			SetWindowDirty(w);
			e->keypress.keycode=0;
			} else e->keypress.cont=true;	
		break;

	}
}

void IConsoleInit()
{
int i;
_iconsole_inited=true;
_iconsole_mode=ICONSOLE_CLOSED;
_iconsole_win=NULL;
_icursor_state=false;
_icursor_rate=5;
_icursor_counter=0;
for (i=0;i<80;i++) _iconsole_buffer[i]=NULL;
IConsoleStdLibRegister();
IConsolePrint(12,"OpenTTD Game Console Build #");	
IConsolePrint(12,"---------------------------------");
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

static void* IConsoleCmdGetAddr(byte * name) {
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

static void IConsoleVarStringSet(_iconsole_var * var, byte * string) {
int l;

if (strlen((byte *) var->addr)!=0) {
	free(var->addr);
	}
l=strlen((char *) string);
var->addr=malloc(l+1);
memset(var->addr,0,l);
memcpy((void *) var->addr,(void *) string, l);
((byte *)var->addr)[l]=0;
}

void IConsoleCmdExec(byte * cmdstr) {
void (*function)(byte argc, byte* argv[], byte argt[]);
byte * tokens[20];
byte tokentypes[20];
byte * tokenstream;
byte * tokenstream_s;
byte execution_mode;
_iconsole_var * var = NULL;

byte var_b; // TYPE BYTE
unsigned short var_ui16; // TYPE UINT16
unsigned int var_ui32; // TYPE UINT32
signed short var_i16; // TYPE INT16
signed int var_i32; // TYPE INT32
byte * var_s; // TYPE STRING

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
		if (tokens[i][0]=='*') {
			var = IConsoleVarGet(tokens[i]);
			if (var!=NULL) {
				tokens[i]=(byte *)var->addr;
				tokentypes[i]=var->type;
				}
			} else { 
			tokentypes[i]=ICONSOLE_VAR_STRING;
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
	function(c,tokens,tokentypes);
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
						*(bool *)var->addr=(atoi((char *) tokens[2])==1);
						c=1;
						} else {
						*(bool *)var->addr=false;
						c=1;
						}
					}
				else if (strcmp(tokens[1],"++")==0) {
					*(bool *)var->addr=!*(bool *)var->addr;
					c=1;
					}
				else if (strcmp(tokens[1],"--")==0) {
					*(bool *)var->addr=!*(bool *)var->addr;
					c=1;
					}	 
				else { IConsoleError("operation not supported"); }
				}
				break;
		case ICONSOLE_VAR_BYTE:
				{
				if (strcmp(tokens[1],"=")==0) {
					if (c==3) {
						*(byte *)var->addr=atoi((char *) tokens[2]);
						c=1;
						} else {
						*(byte *)var->addr=0;
						c=1;
						}
					}
				else if (strcmp(tokens[1],"++")==0) {
					(*(byte *)var->addr)++;
					c=1;
					}
				else if (strcmp(tokens[1],"--")==0) {
					(*(byte *)var->addr)--;
					c=1;
					}	 
				else { IConsoleError("operation not supported"); }
				}
				break;
		case ICONSOLE_VAR_UINT16:
				{
				if (strcmp(tokens[1],"=")==0) {
					if (c==3) {
						*(unsigned short *)var->addr=atoi((char *) tokens[2]);
						c=1;
						} else {
						*(unsigned short *)var->addr=0;
						c=1;
						}
					}
				else if (strcmp(tokens[1],"++")==0) {
					(*(unsigned short *)var->addr)++;
					c=1;
					}
				else if (strcmp(tokens[1],"--")==0) {
					(*(unsigned short *)var->addr)--;
					c=1;
					}
				else { IConsoleError("operation not supported"); }
				}
				break;
		case ICONSOLE_VAR_UINT32:
				{
				if (strcmp(tokens[1],"=")==0) {
					if (c==3) {
						*(unsigned int *)var->addr=atoi((char *) tokens[2]);
						c=1;
						} else {
						*(unsigned int *)var->addr=0;
						c=1;
						}
					}
				else if (strcmp(tokens[1],"++")==0) {
					(*(unsigned int *)var->addr)++;
					c=1;
					}
				else if (strcmp(tokens[1],"--")==0) {
					(*(unsigned int *)var->addr)--;
					c=1;
					}
				else { IConsoleError("operation not supported"); }
				}
				break;
		case ICONSOLE_VAR_INT16:
				{
				if (strcmp(tokens[1],"=")==0) {
					if (c==3) {
						*(signed short *)var->addr=atoi((char *) tokens[2]);
						c=1;
						} else {
						*(signed short *)var->addr=0;
						c=1;
						}
					}
				else if (strcmp(tokens[1],"++")==0) {
					(*(signed short *)var->addr)++;
					c=1;
					}
				else if (strcmp(tokens[1],"--")==0) {
					(*(signed short *)var->addr)--;
					c=1;
					}
				else { IConsoleError("operation not supported"); }
				}
				break;
		case ICONSOLE_VAR_INT32:
				{
				if (strcmp(tokens[1],"=")==0) {
					if (c==3) {
						*(signed int *)var->addr=atoi((char *) tokens[2]);
						c=1;
						} else {
						*(signed int *)var->addr=0;
						c=1;
						}
					}
				else if (strcmp(tokens[1],"++")==0) {
					(*(signed int *)var->addr)++;
					c=1;
					}
				else if (strcmp(tokens[1],"--")==0) {
					(*(signed int *)var->addr)--;
					c=1;
					}
				else { IConsoleError("operation not supported"); }
				}
				break;
		case ICONSOLE_VAR_STRING:
				{
				if (strcmp(tokens[1],"=")==0) {
					if (c==3) {
						IConsoleVarStringSet(var, tokens[2]);
						c=1;
						} else {
						IConsoleVarStringSet(var, "");
						c=1;
						}
					}
				else { IConsoleError("operation not supported"); }
				}
				break;
		case ICONSOLE_VAR_POINTER:
				{
				if (strcmp(tokens[1],"=")==0) {
					if (c==3) {
						var->addr = tokens[2];
						c=1;
						} else {
						var->addr = NULL;
						c=1;
						}
					}
				else if (strcmp(tokens[1],"++")==0) {
					var->addr = ((char *)var->addr)+1;
					c=1;
					}
				else if (strcmp(tokens[1],"--")==0) {
					var->addr = ((char *)var->addr)-1;;
					c=1;
					}
				else { IConsoleError("operation not supported"); }
				}
				break;
			}
	}
	if (c==1) {
		// ** variable output ** //
		switch (var->type) {
		case ICONSOLE_VAR_BOOLEAN:
				{
				if (*(bool *)var->addr) {
					IConsolePrintF(_iconsole_color_default, "%s = true",var->name);
					} else {
					IConsolePrintF(_iconsole_color_default, "%s = false",var->name);
					}
				}
				break;
		case ICONSOLE_VAR_BYTE:
				{
				var_b=*(byte *)var->addr;
				IConsolePrintF(_iconsole_color_default, "%s = %i",var->name,var_b);
				}
				break;
		case ICONSOLE_VAR_UINT16:
				{
				var_ui16=*(unsigned short *)var->addr;
				IConsolePrintF(_iconsole_color_default, "%s = %i",var->name,var_ui16);
				}
				break;
		case ICONSOLE_VAR_UINT32:
				{
				var_ui32=*(unsigned int *)var->addr;
				IConsolePrintF(_iconsole_color_default, "%s = %i",var->name,var_ui32);
				}
				break;
		case ICONSOLE_VAR_INT16:
				{
				var_i16=*(signed short *)var->addr;
				IConsolePrintF(_iconsole_color_default, "%s = %i",var->name,var_i16);
				}
				break;
		case ICONSOLE_VAR_INT32:
				{
				var_i32=*(signed int *)var->addr;
				IConsolePrintF(_iconsole_color_default, "%s = %i",var->name,var_i32);
				}
				break;
		case ICONSOLE_VAR_STRING:
				{
				var_s=(byte *)var->addr;
				IConsolePrintF(_iconsole_color_default, "%s = %s",var->name,var_s);
				}
				break;
		case ICONSOLE_VAR_UNKNOWN:
		case ICONSOLE_VAR_VARPTR:
		case ICONSOLE_VAR_POINTER:
				{
				var_i32=(signed int)((byte *)var->addr);
				IConsolePrintF(_iconsole_color_default, "%s = @%i",var->name,var_i32);
				}
				break;
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


static void IConsoleStdLibEcho(byte argc, byte * argv[], byte argt[]) {
	if (argc<2) return;
	IConsolePrint(_iconsole_color_default, argv[1]);
}

static void IConsoleStdLibEchoC(byte argc, byte * argv[], byte argt[]) {
	if (argc<3) return;
	IConsolePrint(atoi(argv[1]), argv[2]);
}

static void IConsoleStdLibPrintF(byte argc, byte * argv[], byte argt[]) {
	if (argc<3) return;
	IConsolePrintF(_iconsole_color_default, argv[1] ,argv[2],argv[3],argv[4],argv[5],argv[6],argv[7],argv[8],argv[9],argv[10],argv[11],argv[12],argv[13],argv[14],argv[15],argv[16],argv[17],argv[18],argv[19]);
}

static void IConsoleStdLibPrintFC(byte argc, byte * argv[], byte argt[]) {
	if (argc<3) return;
	IConsolePrintF(atoi(argv[1]), argv[2] ,argv[3],argv[4],argv[5],argv[6],argv[7],argv[8],argv[9],argv[10],argv[11],argv[12],argv[13],argv[14],argv[15],argv[16],argv[17],argv[18],argv[19]);
}

static void IConsoleStdLibScreenShot(byte argc, byte * argv[], byte argt[]) {
	
	if (argc<2) {
		_make_screenshot=1;
	} else {
		if (strcmp(argv[1],"big")==0) {
			_make_screenshot=2;
			}
		}
}

static void IConsoleStdLibDebugLevel(byte argc, byte * argv[], byte argt[]) {
	if (argc<2) return;
	SetDebugString(argv[1]);
}

static void IConsoleStdLibExit(byte argc, byte * argv[], byte argt[]) {
	_exit_game = true;
}

static void IConsoleStdLibListCommands(byte argc, byte * argv[], byte argt[]) {
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
}

static void IConsoleStdLibListVariables(byte argc, byte * argv[], byte argt[]) {
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
}

static void IConsoleStdLibRegister() {
	IConsoleCmdRegister("echo",IConsoleStdLibEcho);
	IConsoleCmdRegister("echoc",IConsoleStdLibEchoC);
	IConsoleCmdRegister("printf",IConsoleStdLibPrintF);
	IConsoleCmdRegister("printfc",IConsoleStdLibPrintFC);
	IConsoleCmdRegister("list_cmds",IConsoleStdLibListCommands);
	IConsoleCmdRegister("list_vars",IConsoleStdLibListVariables);
	IConsoleCmdRegister("screenshot",IConsoleStdLibScreenShot);
	IConsoleCmdRegister("debug_level",IConsoleStdLibDebugLevel);
	IConsoleCmdRegister("exit",IConsoleStdLibExit);
	IConsoleVarRegister("developer",(void *) &_stdlib_developer,ICONSOLE_VAR_BYTE);
	IConsoleVarRegister("cursor_rate",(void *) &_icursor_rate,ICONSOLE_VAR_BYTE);
	IConsoleVarRegister("temp_string",(void *) &_stdlib_temp_string,ICONSOLE_VAR_STRING);
	IConsoleVarRegister("temp_pointer",(void *) &_stdlib_temp_pointer,ICONSOLE_VAR_POINTER);
	IConsoleVarRegister("temp_uint32",(void *) &_stdlib_temp_uint32,ICONSOLE_VAR_UINT32);
	IConsoleVarRegister("temp_bool",(void *) &_stdlib_temp_bool,ICONSOLE_VAR_BOOLEAN);
}


