/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file osk_gui.cpp The On Screen Keyboard GUI */

#include "stdafx.h"
#include "string_func.h"
#include "strings_func.h"
#include "debug.h"
#include "window_func.h"
#include "gfx_func.h"
#include "querystring_gui.h"
#include "video/video_driver.hpp"

#include "widgets/osk_widget.h"

#include "table/sprites.h"
#include "table/strings.h"

#include "safeguards.h"

char _keyboard_opt[2][OSK_KEYBOARD_ENTRIES * 4 + 1];
static WChar _keyboard[2][OSK_KEYBOARD_ENTRIES];

enum KeyStateBits {
	KEYS_NONE,
	KEYS_SHIFT,
	KEYS_CAPS
};
static byte _keystate = KEYS_NONE;

struct OskWindow : public Window {
	StringID caption;      ///< the caption for this window.
	QueryString *qs;       ///< text-input
	int text_btn;          ///< widget number of parent's text field
	Textbuf *text;         ///< pointer to parent's textbuffer (to update caret position)
	char *orig_str_buf;    ///< Original string.
	bool shift;            ///< Is the shift effectively pressed?

	OskWindow(WindowDesc *desc, Window *parent, int button) : Window(desc)
	{
		this->parent = parent;
		assert(parent != NULL);

		NWidgetCore *par_wid = parent->GetWidget<NWidgetCore>(button);
		assert(par_wid != NULL);

		assert(parent->querystrings.Contains(button));
		this->qs         = parent->querystrings.Find(button)->second;
		this->caption = (par_wid->widget_data != STR_NULL) ? par_wid->widget_data : this->qs->caption;
		this->text_btn   = button;
		this->text       = &this->qs->text;
		this->querystrings[WID_OSK_TEXT] = this->qs;

		/* make a copy in case we need to reset later */
		this->orig_str_buf = stredup(this->qs->text.buf);

		this->InitNested(0);
		this->SetFocusedWidget(WID_OSK_TEXT);

		/* Not needed by default. */
		this->DisableWidget(WID_OSK_SPECIAL);

		this->UpdateOskState();
	}

	~OskWindow()
	{
		free(this->orig_str_buf);
	}

	/**
	 * Only show valid characters; do not show characters that would
	 * only insert a space when we have a spacebar to do that or
	 * characters that are not allowed to be entered.
	 */
	void UpdateOskState()
	{
		this->shift = HasBit(_keystate, KEYS_CAPS) ^ HasBit(_keystate, KEYS_SHIFT);

		for (uint i = 0; i < OSK_KEYBOARD_ENTRIES; i++) {
			this->SetWidgetDisabledState(WID_OSK_LETTERS + i,
					!IsValidChar(_keyboard[this->shift][i], this->qs->text.afilter) || _keyboard[this->shift][i] == ' ');
		}
		this->SetWidgetDisabledState(WID_OSK_SPACE, !IsValidChar(' ', this->qs->text.afilter));

		this->SetWidgetLoweredState(WID_OSK_SHIFT, HasBit(_keystate, KEYS_SHIFT));
		this->SetWidgetLoweredState(WID_OSK_CAPS, HasBit(_keystate, KEYS_CAPS));
	}

	virtual void SetStringParameters(int widget) const
	{
		if (widget == WID_OSK_CAPTION) SetDParam(0, this->caption);
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		if (widget < WID_OSK_LETTERS) return;

		widget -= WID_OSK_LETTERS;
		DrawCharCentered(_keyboard[this->shift][widget],
			r.left + 8,
			r.top + 3,
			TC_BLACK);
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		/* clicked a letter */
		if (widget >= WID_OSK_LETTERS) {
			WChar c = _keyboard[this->shift][widget - WID_OSK_LETTERS];

			if (!IsValidChar(c, this->qs->text.afilter)) return;

			if (this->qs->text.InsertChar(c)) this->OnEditboxChanged(WID_OSK_TEXT);

			if (HasBit(_keystate, KEYS_SHIFT)) {
				ToggleBit(_keystate, KEYS_SHIFT);
				this->UpdateOskState();
				this->SetDirty();
			}
			return;
		}

		switch (widget) {
			case WID_OSK_BACKSPACE:
				if (this->qs->text.DeleteChar(WKC_BACKSPACE)) this->OnEditboxChanged(WID_OSK_TEXT);
				break;

			case WID_OSK_SPECIAL:
				/*
				 * Anything device specific can go here.
				 * The button itself is hidden by default, and when you need it you
				 * can not hide it in the create event.
				 */
				break;

			case WID_OSK_CAPS:
				ToggleBit(_keystate, KEYS_CAPS);
				this->UpdateOskState();
				this->SetDirty();
				break;

			case WID_OSK_SHIFT:
				ToggleBit(_keystate, KEYS_SHIFT);
				this->UpdateOskState();
				this->SetDirty();
				break;

			case WID_OSK_SPACE:
				if (this->qs->text.InsertChar(' ')) this->OnEditboxChanged(WID_OSK_TEXT);
				break;

			case WID_OSK_LEFT:
				if (this->qs->text.MovePos(WKC_LEFT)) this->InvalidateData();
				break;

			case WID_OSK_RIGHT:
				if (this->qs->text.MovePos(WKC_RIGHT)) this->InvalidateData();
				break;

			case WID_OSK_OK:
				if (this->qs->orig == NULL || strcmp(this->qs->text.buf, this->qs->orig) != 0) {
					/* pass information by simulating a button press on parent window */
					if (this->qs->ok_button >= 0) {
						this->parent->OnClick(pt, this->qs->ok_button, 1);
						/* Window gets deleted when the parent window removes itself. */
						return;
					}
				}
				delete this;
				break;

			case WID_OSK_CANCEL:
				if (this->qs->cancel_button >= 0) { // pass a cancel event to the parent window
					this->parent->OnClick(pt, this->qs->cancel_button, 1);
					/* Window gets deleted when the parent window removes itself. */
					return;
				} else { // or reset to original string
					qs->text.Assign(this->orig_str_buf);
					qs->text.MovePos(WKC_END);
					this->OnEditboxChanged(WID_OSK_TEXT);
					delete this;
				}
				break;
		}
	}

	virtual void OnEditboxChanged(int widget)
	{
		this->SetWidgetDirty(WID_OSK_TEXT);
		this->parent->OnEditboxChanged(this->text_btn);
		this->parent->SetWidgetDirty(this->text_btn);
	}

	virtual void OnInvalidateData(int data = 0, bool gui_scope = true)
	{
		if (!gui_scope) return;
		this->SetWidgetDirty(WID_OSK_TEXT);
		this->parent->SetWidgetDirty(this->text_btn);
	}

	virtual void OnFocusLost()
	{
		VideoDriver::GetInstance()->EditBoxLostFocus();
		delete this;
	}
};

static const int HALF_KEY_WIDTH = 7;  // Width of 1/2 key in pixels.
static const int INTER_KEY_SPACE = 2; // Number of pixels between two keys.

/**
 * Add a key widget to a row of the keyboard.
 * @param hor     Row container to add key widget to.
 * @param height  Height of the key (all keys in a row should have equal height).
 * @param num_half Number of 1/2 key widths that this key has.
 * @param widtype Widget type of the key. Must be either \c NWID_SPACER for an invisible key, or a \c WWT_* widget.
 * @param widnum  Widget number of the key.
 * @param widdata Data value of the key widget.
 * @param biggest_index Collected biggest widget index so far.
 * @note Key width is measured in 1/2 keys to allow for 1/2 key shifting between rows.
 */
static void AddKey(NWidgetHorizontal *hor, int height, int num_half, WidgetType widtype, int widnum, uint16 widdata, int *biggest_index)
{
	int key_width = HALF_KEY_WIDTH + (INTER_KEY_SPACE + HALF_KEY_WIDTH) * (num_half - 1);

	if (widtype == NWID_SPACER) {
		if (!hor->IsEmpty()) key_width += INTER_KEY_SPACE;
		NWidgetSpacer *spc = new NWidgetSpacer(key_width, height);
		hor->Add(spc);
	} else {
		if (!hor->IsEmpty()) {
			NWidgetSpacer *spc = new NWidgetSpacer(INTER_KEY_SPACE, height);
			hor->Add(spc);
		}
		NWidgetLeaf *leaf = new NWidgetLeaf(widtype, COLOUR_GREY, widnum, widdata, STR_NULL);
		leaf->SetMinimalSize(key_width, height);
		hor->Add(leaf);
	}

	*biggest_index = max(*biggest_index, widnum);
}

/** Construct the top row keys (cancel, ok, backspace). */
static NWidgetBase *MakeTopKeys(int *biggest_index)
{
	NWidgetHorizontal *hor = new NWidgetHorizontal();
	int key_height = FONT_HEIGHT_NORMAL + 2;

	AddKey(hor, key_height, 6 * 2, WWT_TEXTBTN,    WID_OSK_CANCEL,    STR_BUTTON_CANCEL,  biggest_index);
	AddKey(hor, key_height, 6 * 2, WWT_TEXTBTN,    WID_OSK_OK,        STR_BUTTON_OK,      biggest_index);
	AddKey(hor, key_height, 2 * 2, WWT_PUSHIMGBTN, WID_OSK_BACKSPACE, SPR_OSK_BACKSPACE, biggest_index);
	return hor;
}

/** Construct the row containing the digit keys. */
static NWidgetBase *MakeNumberKeys(int *biggest_index)
{
	NWidgetHorizontal *hor = new NWidgetHorizontalLTR();
	int key_height = FONT_HEIGHT_NORMAL + 6;

	for (int widnum = WID_OSK_NUMBERS_FIRST; widnum <= WID_OSK_NUMBERS_LAST; widnum++) {
		AddKey(hor, key_height, 2, WWT_PUSHBTN, widnum, 0x0, biggest_index);
	}
	return hor;
}

/** Construct the qwerty row keys. */
static NWidgetBase *MakeQwertyKeys(int *biggest_index)
{
	NWidgetHorizontal *hor = new NWidgetHorizontalLTR();
	int key_height = FONT_HEIGHT_NORMAL + 6;

	AddKey(hor, key_height, 3, WWT_PUSHIMGBTN, WID_OSK_SPECIAL, SPR_OSK_SPECIAL, biggest_index);
	for (int widnum = WID_OSK_QWERTY_FIRST; widnum <= WID_OSK_QWERTY_LAST; widnum++) {
		AddKey(hor, key_height, 2, WWT_PUSHBTN, widnum, 0x0, biggest_index);
	}
	AddKey(hor, key_height, 1, NWID_SPACER, 0, 0, biggest_index);
	return hor;
}

/** Construct the asdfg row keys. */
static NWidgetBase *MakeAsdfgKeys(int *biggest_index)
{
	NWidgetHorizontal *hor = new NWidgetHorizontalLTR();
	int key_height = FONT_HEIGHT_NORMAL + 6;

	AddKey(hor, key_height, 4, WWT_IMGBTN, WID_OSK_CAPS, SPR_OSK_CAPS, biggest_index);
	for (int widnum = WID_OSK_ASDFG_FIRST; widnum <= WID_OSK_ASDFG_LAST; widnum++) {
		AddKey(hor, key_height, 2, WWT_PUSHBTN, widnum, 0x0, biggest_index);
	}
	return hor;
}

/** Construct the zxcvb row keys. */
static NWidgetBase *MakeZxcvbKeys(int *biggest_index)
{
	NWidgetHorizontal *hor = new NWidgetHorizontalLTR();
	int key_height = FONT_HEIGHT_NORMAL + 6;

	AddKey(hor, key_height, 3, WWT_IMGBTN, WID_OSK_SHIFT, SPR_OSK_SHIFT, biggest_index);
	for (int widnum = WID_OSK_ZXCVB_FIRST; widnum <= WID_OSK_ZXCVB_LAST; widnum++) {
		AddKey(hor, key_height, 2, WWT_PUSHBTN, widnum, 0x0, biggest_index);
	}
	AddKey(hor, key_height, 1, NWID_SPACER, 0, 0, biggest_index);
	return hor;
}

/** Construct the spacebar row keys. */
static NWidgetBase *MakeSpacebarKeys(int *biggest_index)
{
	NWidgetHorizontal *hor = new NWidgetHorizontal();
	int key_height = FONT_HEIGHT_NORMAL + 6;

	AddKey(hor, key_height,  8, NWID_SPACER, 0, 0, biggest_index);
	AddKey(hor, key_height, 13, WWT_PUSHTXTBTN, WID_OSK_SPACE, STR_EMPTY, biggest_index);
	AddKey(hor, key_height,  3, NWID_SPACER, 0, 0, biggest_index);
	AddKey(hor, key_height,  2, WWT_PUSHIMGBTN, WID_OSK_LEFT,  SPR_OSK_LEFT, biggest_index);
	AddKey(hor, key_height,  2, WWT_PUSHIMGBTN, WID_OSK_RIGHT, SPR_OSK_RIGHT, biggest_index);
	return hor;
}


static const NWidgetPart _nested_osk_widgets[] = {
	NWidget(WWT_CAPTION, COLOUR_GREY, WID_OSK_CAPTION), SetDataTip(STR_WHITE_STRING, STR_NULL),
	NWidget(WWT_PANEL, COLOUR_GREY),
		NWidget(WWT_EDITBOX, COLOUR_GREY, WID_OSK_TEXT), SetMinimalSize(252, 12), SetPadding(2, 2, 2, 2),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY), SetPIP(5, 2, 3),
		NWidgetFunction(MakeTopKeys), SetPadding(0, 3, 0, 3),
		NWidgetFunction(MakeNumberKeys), SetPadding(0, 3, 0, 3),
		NWidgetFunction(MakeQwertyKeys), SetPadding(0, 3, 0, 3),
		NWidgetFunction(MakeAsdfgKeys), SetPadding(0, 3, 0, 3),
		NWidgetFunction(MakeZxcvbKeys), SetPadding(0, 3, 0, 3),
		NWidgetFunction(MakeSpacebarKeys), SetPadding(0, 3, 0, 3),
	EndContainer(),
};

static WindowDesc _osk_desc(
	WDP_CENTER, "query_osk", 0, 0,
	WC_OSK, WC_NONE,
	0,
	_nested_osk_widgets, lengthof(_nested_osk_widgets)
);

/**
 * Retrieve keyboard layout from language string or (if set) config file.
 * Also check for invalid characters.
 */
void GetKeyboardLayout()
{
	char keyboard[2][OSK_KEYBOARD_ENTRIES * 4 + 1];
	char errormark[2][OSK_KEYBOARD_ENTRIES + 1]; // used for marking invalid chars
	bool has_error = false; // true when an invalid char is detected

	if (StrEmpty(_keyboard_opt[0])) {
		GetString(keyboard[0], STR_OSK_KEYBOARD_LAYOUT, lastof(keyboard[0]));
	} else {
		strecpy(keyboard[0], _keyboard_opt[0], lastof(keyboard[0]));
	}

	if (StrEmpty(_keyboard_opt[1])) {
		GetString(keyboard[1], STR_OSK_KEYBOARD_LAYOUT_CAPS, lastof(keyboard[1]));
	} else {
		strecpy(keyboard[1], _keyboard_opt[1], lastof(keyboard[1]));
	}

	for (uint j = 0; j < 2; j++) {
		const char *kbd = keyboard[j];
		bool ended = false;
		for (uint i = 0; i < OSK_KEYBOARD_ENTRIES; i++) {
			_keyboard[j][i] = Utf8Consume(&kbd);

			/* Be lenient when the last characters are missing (is quite normal) */
			if (_keyboard[j][i] == '\0' || ended) {
				ended = true;
				_keyboard[j][i] = ' ';
				continue;
			}

			if (IsPrintable(_keyboard[j][i])) {
				errormark[j][i] = ' ';
			} else {
				has_error = true;
				errormark[j][i] = '^';
				_keyboard[j][i] = ' ';
			}
		}
	}

	if (has_error) {
		ShowInfoF("The keyboard layout you selected contains invalid chars. Please check those chars marked with ^.");
		ShowInfoF("Normal keyboard:  %s", keyboard[0]);
		ShowInfoF("                  %s", errormark[0]);
		ShowInfoF("Caps Lock:        %s", keyboard[1]);
		ShowInfoF("                  %s", errormark[1]);
	}
}

/**
 * Show the on-screen keyboard (osk) associated with a given textbox
 * @param parent pointer to the Window where this keyboard originated from
 * @param button widget number of parent's textbox
 */
void ShowOnScreenKeyboard(Window *parent, int button)
{
	DeleteWindowById(WC_OSK, 0);

	GetKeyboardLayout();
	new OskWindow(&_osk_desc, parent, button);
}

/**
 * Updates the original text of the OSK so when the 'parent' changes the
 * original and you press on cancel you won't get the 'old' original text
 * but the updated one.
 * @param parent window that just updated its orignal text
 * @param button widget number of parent's textbox to update
 */
void UpdateOSKOriginalText(const Window *parent, int button)
{
	OskWindow *osk = dynamic_cast<OskWindow *>(FindWindowById(WC_OSK, 0));
	if (osk == NULL || osk->parent != parent || osk->text_btn != button) return;

	free(osk->orig_str_buf);
	osk->orig_str_buf = stredup(osk->qs->text.buf);

	osk->SetDirty();
}

/**
 * Check whether the OSK is opened for a specific editbox.
 * @param w Window to check for
 * @param button Editbox of \a w to check for
 * @return true if the OSK is opened for \a button.
 */
bool IsOSKOpenedFor(const Window *w, int button)
{
	OskWindow *osk = dynamic_cast<OskWindow *>(FindWindowById(WC_OSK, 0));
	return osk != NULL && osk->parent == w && osk->text_btn == button;
}
