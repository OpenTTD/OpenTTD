#ifndef WINDOW_H
#define WINDOW_H

#include "vehicle_gui.h"

typedef union WindowEvent WindowEvent;

//typedef void WindowProc(Window *w, int event, int wparam, long lparam);

typedef void WindowProc(Window *w, WindowEvent *e);

typedef struct Widget {
	byte type;
	byte color;
	uint16 left, right, top, bottom;
	uint16 unkA;
	uint16 tooltips;
} Widget;

union WindowEvent {
	byte event;
	struct {
		byte event;
		Point pt;
		int widget;
	} click;

	struct {
		byte event;
		Point pt;
		uint tile;
		uint starttile;
		int userdata;
	} place;

	struct {
		byte event;
		Point pt;
		int widget;
	} dragdrop;

	struct {
		byte event;
		byte *str;
	} edittext;

	struct {
		byte event;
		Point pt;
	} popupmenu;

	struct {
		byte event;
		int button;
		int index;
	} dropdown;

	struct {
		byte event;
		Point pt;
		int widget;
	} mouseover;

	struct {
		byte event;
		bool cont;   // continue the search? (default true)
		byte ascii;  // 8-bit ASCII-value of the key
		uint16 keycode;// untranslated key (including shift-state)
	} keypress;
};

enum WindowKeyCodes {
	WKC_SHIFT = 0x8000,
	WKC_CTRL  = 0x4000,
	WKC_ALT   = 0x2000,
	WKC_META  = 0x1000,

	// Special ones
	WKC_NONE = 0,
	WKC_ESC=1,
	WKC_BACKSPACE = 2,
	WKC_INSERT = 3,
	WKC_DELETE = 4,

	WKC_PAGEUP = 5,
	WKC_PAGEDOWN = 6,
	WKC_END = 7,
	WKC_HOME = 8,

	// Arrow keys
	WKC_LEFT = 9,
	WKC_UP = 10,
	WKC_RIGHT = 11,
	WKC_DOWN = 12,

	// Return & tab
	WKC_RETURN = 13,
	WKC_TAB = 14,

	// Numerical keyboard
	WKC_NUM_0 = 16,
	WKC_NUM_1 = 17,
	WKC_NUM_2 = 18,
	WKC_NUM_3 = 19,
	WKC_NUM_4 = 20,
	WKC_NUM_5 = 21,
	WKC_NUM_6 = 22,
	WKC_NUM_7 = 23,
	WKC_NUM_8 = 24,
	WKC_NUM_9 = 25,
	WKC_NUM_DIV = 26,
	WKC_NUM_MUL = 27,
	WKC_NUM_MINUS = 28,
	WKC_NUM_PLUS = 29,
	WKC_NUM_ENTER = 30,
	WKC_NUM_DECIMAL = 31,

	// Space
	WKC_SPACE = 32,

	// Function keys
	WKC_F1 = 33,
	WKC_F2 = 34,
	WKC_F3 = 35,
	WKC_F4 = 36,
	WKC_F5 = 37,
	WKC_F6 = 38,
	WKC_F7 = 39,
	WKC_F8 = 40,
	WKC_F9 = 41,
	WKC_F10 = 42,
	WKC_F11 = 43,
	WKC_F12 = 44,

	// backquote is the key left of "1"
	// we only store this key here, no matter what character is really mapped to it
	// on a particular keyboard. (US keyboard: ` and ~ ; German keyboard: ^ and °)
	WKC_BACKQUOTE = 45,
	WKC_PAUSE     = 46,

	// 0-9 are mapped to 48-57
	// A-Z are mapped to 65-90
	// a-z are mapped to 97-122


	//WKC_UNKNOWN = 0xFF,

};

typedef struct WindowDesc {
	int16 left, top, width, height;
	byte cls;
	byte parent_cls;
	uint32 flags;
	const Widget *widgets;
	WindowProc *proc;
} WindowDesc;

enum {
	WDF_STD_TOOLTIPS   = 1, /* use standard routine when displaying tooltips */
	WDF_DEF_WIDGET     = 2,	/* default widget control for some widgets in the on click event */
	WDF_STD_BTN        = 4,	/* default handling for close and drag widgets (widget no 0 and 1) */
	WDF_RESTORE_DPARAM = 8, /* when drawing widgets, restore the dparam so all widgets recieve the same set of them */
	WDF_UNCLICK_BUTTONS=16, /* Unclick buttons when the window event times out */
	WDF_STICKY_BUTTON  =32, /* Set window to sticky mode; they are not closed unless closed with 'X' (widget 2) */
};

/* can be used as x or y coordinates to cause a specific placement */
enum {
	WDP_AUTO = -1,
	WDP_CENTER = -2,
};

typedef struct {
	StringID caption;
	bool caret;
	WindowClass wnd_class;
	WindowNumber wnd_num;
	uint16 maxlen, maxwidth;
	byte *buf;
} querystr_d;

#define WP(ptr,str) (*(str*)(ptr)->custom)
// querystr_d is the bigest struct that comes in w->custom
//  because 64-bit systems use 64-bit pointers, it is bigger on a 64-bit system
//  than on a 32-bit system. Therefore, the size is calculated from querystr_d
//  instead of a hardcoded number.
// if any struct becomes bigger the querystr_d, it should be replaced.
#define WINDOW_CUSTOM_SIZE sizeof(querystr_d)

typedef struct {
	uint16 count, cap, pos;
} Scrollbar;

struct Window {
	uint16 flags4;
	WindowClass window_class;
	WindowNumber window_number;

	int left,top;
	int width,height;

	Scrollbar hscroll, vscroll, vscroll2;

	byte caption_color;

	uint32 click_state, disabled_state, hidden_state;
	WindowProc *wndproc;
	ViewPort *viewport;
	const Widget *widget;
	//const WindowDesc *desc;
	uint32 desc_flags;

	byte custom[WINDOW_CUSTOM_SIZE];
};

typedef struct {
	byte item_count; /* follow_vehicle */
	byte sel_index;		/* scrollpos_x */
	byte main_button; /* scrollpos_y */
	byte action_id;
	StringID string_id; /* unk30 */
	uint16 checked_items; /* unk32 */
} menu_d;

typedef struct {
	int16 data_1, data_2, data_3;
	int16 data_4, data_5;
	bool close; /* scrollpos_y */
	byte byte_1;
} def_d;

typedef struct {
	void *data;
} void_d;

typedef struct {
	uint16 base; /* follow_vehicle */
	uint16 count;/* scrollpos_x */
} tree_d;

typedef struct {
	byte refresh_counter; /* follow_vehicle */
} plstations_d;

typedef struct {
	StringID string_id;
} tooltips_d;

typedef struct {
	byte railtype;
	byte sel_index;
	int16 sel_engine;
	int16 rename_engine;
} buildtrain_d;

typedef struct {
	byte railtype;
	byte vehicletype;
	byte sel_index[2];
	int16 sel_engine[2];
	uint16 count[2];
	byte line_height;
} replaceveh_d;

typedef struct {
	VehicleID sel;
} traindepot_d;

typedef struct {
	int sel;
} order_d;

typedef struct {
	byte tab;
} traindetails_d;

typedef struct {
	int16 scroll_x, scroll_y, subscroll;
} smallmap_d;

typedef struct {
	uint32 face;
	byte gender;
} facesel_d;

typedef struct {
	int sel;
	byte cargo;
} refit_d;

typedef struct {
	uint16 follow_vehicle;
	int16 scrollpos_x, scrollpos_y;
} vp_d;

typedef struct {
	uint16 follow_vehicle;
	int16 scrollpos_x, scrollpos_y;
	NewsItem *ni;
} news_d;

typedef enum VehicleListFlags {
	VL_DESC    = 0x01,
	VL_RESORT  = 0x02,
	VL_REBUILD = 0x04
} VehicleListFlags;

typedef struct vehiclelist_d {
	SortStruct *sort_list;
	uint16 list_length;
	byte sort_type;
	VehicleListFlags flags;
	uint16 resort_timer;
} vehiclelist_d;
assert_compile(sizeof(vehiclelist_d) <= WINDOW_CUSTOM_SIZE);

enum WindowEvents {
	WE_CLICK = 0,
	WE_PAINT = 1,
	WE_MOUSELOOP = 2,
	WE_TICK = 3,
	WE_4 = 4,
	WE_TIMEOUT = 5,
	WE_PLACE_OBJ = 6,
	WE_ABORT_PLACE_OBJ = 7,
	WE_DESTROY = 8,
	WE_ON_EDIT_TEXT = 9,
	WE_POPUPMENU_SELECT = 10,
	WE_POPUPMENU_OVER = 11,
	WE_DRAGDROP = 12,
	WE_PLACE_DRAG = 13,
	WE_PLACE_MOUSEUP = 14,
	WE_PLACE_PRESIZE = 15,
	WE_DROPDOWN_SELECT = 16,
	WE_RCLICK = 17,
	WE_KEYPRESS = 18,
	WE_CREATE = 19,
	WE_MOUSEOVER = 20,
	WE_ON_EDIT_TEXT_CANCEL = 21,
};


/****************** THESE ARE NOT WIDGET TYPES!!!!! *******************/
enum WindowWidgetBehaviours {
	WWB_PUSHBUTTON = 1 << 5,
	WWB_NODISBUTTON = 2 << 5,
};


enum WindowWidgetTypes {
	WWT_EMPTY = 0,

	WWT_IMGBTN = 1,						/* button with image */
	WWT_PANEL = WWT_IMGBTN,
	WWT_PANEL_2 = 2,					/* button with diff image when clicked */

	WWT_TEXTBTN = 3,					/* button with text */
	WWT_CLOSEBOX = WWT_TEXTBTN,
	WWT_4 = 4,								/* button with diff text when clicked */
	WWT_5 = 5,								/* label */
	WWT_6 = 6,								/* combo box text area */
	WWT_MATRIX = 7,
	WWT_SCROLLBAR = 8,
	WWT_FRAME = 9,						/* frame */
	WWT_CAPTION = 10,

	WWT_HSCROLLBAR = 11,
	WWT_STICKYBOX = 12,
	WWT_SCROLL2BAR = 13,				/* 2nd vertical scrollbar*/
	WWT_LAST = 14,						/* Last Item. use WIDGETS_END to fill up padding!! */

	WWT_MASK = 31,

	WWT_PUSHTXTBTN	= WWT_TEXTBTN	| WWB_PUSHBUTTON,
	WWT_PUSHIMGBTN	= WWT_IMGBTN	| WWB_PUSHBUTTON,
	WWT_NODISTXTBTN = WWT_TEXTBTN	| WWB_NODISBUTTON,
};

#define WIDGETS_END WWT_LAST,     0,     0,     0,     0,     0, 0, STR_NULL

enum WindowFlags {
	WF_TIMEOUT_SHL = 0,
	WF_TIMEOUT_MASK = 7,
	WF_DRAGGING = 1 << 3,
	WF_SCROLL_UP = 1 << 4,
	WF_SCROLL_DOWN = 1 << 5,
	WF_SCROLL_MIDDLE = 1 << 6,
	WF_HSCROLL = 1 << 7,
	WF_SIZING = 1 << 8,
	WF_STICKY = 1 << 9,
	
	WF_DISABLE_VP_SCROLL = 1 << 10,

	WF_WHITE_BORDER_ONE = 1 << 11,
	WF_WHITE_BORDER_MASK = 3 << 11,
	WF_SCROLL2 = 1 << 13,
};


void DispatchLeftClickEvent(Window *w, int x, int y);
void DispatchRightClickEvent(Window *w, int x, int y);
void DispatchMouseWheelEvent(Window *w, int wheel);

/* window.c */
void DrawOverlappedWindow(Window *w, int left, int top, int right, int bottom);
void CallWindowEventNP(Window *w, int event);
void CallWindowTickEvent();
void DrawDirtyBlocks();
void SetDirtyBlocks(int left, int top, int right, int bottom);
void SetWindowDirty(Window *w);

Window *FindWindowById(WindowClass cls, WindowNumber number);
void DeleteWindow(Window *w);
Window *BringWindowToFrontById(WindowClass cls, WindowNumber number);
Window *BringWindowToFront(Window *w);
Window *StartWindowDrag(Window *w);
Window *StartWindowSizing(Window *w);
Window *FindWindowFromPt(int x, int y);

Window *AllocateWindow(
							int x,
							int y,
							int width,
							int height,
							WindowProc *proc,
							WindowClass cls,
							const Widget *widget);

Window *AllocateWindowDesc(const WindowDesc *desc);
Window *AllocateWindowDescFront(const WindowDesc *desc, int value);

Window *AllocateWindowAutoPlace(
	int width,
	int height,
	WindowProc *proc,
	WindowClass cls,
	const Widget *widget);

Window *AllocateWindowAutoPlace2(
	WindowClass exist_class,
	WindowNumber exist_num,
	int width,
	int height,
	WindowProc *proc,
	WindowClass cls,
	const Widget *widget);

void DrawWindowViewport(Window *w);

void InitWindowSystem();
int GetMenuItemIndex(Window *w, int x, int y);
void MouseLoop();
void UpdateWindows();
void InvalidateWidget(Window *w, byte widget_index);

void GuiShowTooltips(uint16 string_id);

void UnclickWindowButtons(Window *w);
void UnclickSomeWindowButtons(Window *w, uint32 mask);
void RelocateAllWindows(int neww, int newh);
int PositionMainToolbar(Window *w);

/* widget.c */
int GetWidgetFromPos(Window *w, int x, int y);
void DrawWindowWidgets(Window *w);
void ShowDropDownMenu(Window *w, const StringID *strings, int selected, int button, uint32 disabled_mask, bool remove_filtered_strings);

void HandleButtonClick(Window *w, byte widget);

Window *GetCallbackWnd();
void DeleteNonVitalWindows();
void DeleteAllNonVitalWindows(void);

/* window.c */
VARDEF Window _windows[25];
VARDEF Window *_last_window;

VARDEF Point _cursorpos_drag_start;

VARDEF bool _left_button_down;
VARDEF bool _left_button_clicked;

VARDEF bool _right_button_down;
VARDEF bool _right_button_clicked;

VARDEF int _alloc_wnd_parent_num;

VARDEF int _scrollbar_start_pos;
VARDEF int _scrollbar_size;
VARDEF bool _demo_mode;
VARDEF byte _scroller_click_timeout;

VARDEF bool _dragging_window;
VARDEF bool _scrolling_scrollbar;
VARDEF bool _scrolling_viewport;
VARDEF bool _popup_menu_active;
//VARDEF bool _dragdrop_active;

VARDEF Point _fix_mouse_at;

VARDEF byte _special_mouse_mode;
enum SpecialMouseMode {
	WSM_NONE = 0,
	WSM_DRAGDROP = 1,
	WSM_SIZING = 2,
	WSM_PRESIZE = 3,
};

void ScrollbarClickHandler(Window *w, const Widget *wi, int x, int y);

#endif /* WINDOW_H */
