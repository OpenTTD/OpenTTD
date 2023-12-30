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
#include "zoom_func.h"

#include "widgets/osk_widget.h"

#include "table/sprites.h"
#include "table/strings.h"

#include "safeguards.h"

std::string _keyboard_opt[2];
static char32_t _keyboard[2][OSK_KEYBOARD_ENTRIES];

enum KeyStateBits {
	KEYS_NONE,
	KEYS_SHIFT,
	KEYS_CAPS
};
static byte _keystate = KEYS_NONE;

struct OskWindow : public Window {
	StringID caption;      ///< the caption for this window.
	QueryString *qs;       ///< text-input
	WidgetID text_btn;     ///< widget number of parent's text field
	Textbuf *text;         ///< pointer to parent's textbuffer (to update caret position)
	std::string orig_str;  ///< Original string.
	bool shift;            ///< Is the shift effectively pressed?

	OskWindow(WindowDesc *desc, Window *parent, int button) : Window(desc)
	{
		this->parent = parent;
		assert(parent != nullptr);

		NWidgetCore *par_wid = parent->GetWidget<NWidgetCore>(button);
		assert(par_wid != nullptr);

		assert(parent->querystrings.count(button) != 0);
		this->qs         = parent->querystrings.find(button)->second;
		this->caption = (par_wid->widget_data != STR_NULL) ? par_wid->widget_data : this->qs->caption;
		this->text_btn   = button;
		this->text       = &this->qs->text;
		this->querystrings[WID_OSK_TEXT] = this->qs;

		/* make a copy in case we need to reset later */
		this->orig_str = this->qs->text.buf;

		this->InitNested(0);
		this->SetFocusedWidget(WID_OSK_TEXT);

		/* Not needed by default. */
		this->DisableWidget(WID_OSK_SPECIAL);

		this->UpdateOskState();
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

	void SetStringParameters(WidgetID widget) const override
	{
		if (widget == WID_OSK_CAPTION) SetDParam(0, this->caption);
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget < WID_OSK_LETTERS) return;

		widget -= WID_OSK_LETTERS;
		DrawCharCentered(_keyboard[this->shift][widget], r, TC_BLACK);
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		/* clicked a letter */
		if (widget >= WID_OSK_LETTERS) {
			char32_t c = _keyboard[this->shift][widget - WID_OSK_LETTERS];

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
				if (!this->qs->orig.has_value() || this->qs->text.buf != this->qs->orig) {
					/* pass information by simulating a button press on parent window */
					if (this->qs->ok_button >= 0) {
						this->parent->OnClick(pt, this->qs->ok_button, 1);
						/* Window gets deleted when the parent window removes itself. */
						return;
					}
				}
				this->Close();
				break;

			case WID_OSK_CANCEL:
				if (this->qs->cancel_button >= 0) { // pass a cancel event to the parent window
					this->parent->OnClick(pt, this->qs->cancel_button, 1);
					/* Window gets deleted when the parent window removes itself. */
					return;
				} else { // or reset to original string
					qs->text.Assign(this->orig_str);
					qs->text.MovePos(WKC_END);
					this->OnEditboxChanged(WID_OSK_TEXT);
					this->Close();
				}
				break;
		}
	}

	void OnEditboxChanged(WidgetID widget) override
	{
		if (widget == WID_OSK_TEXT) {
			this->SetWidgetDirty(WID_OSK_TEXT);
			this->parent->OnEditboxChanged(this->text_btn);
			this->parent->SetWidgetDirty(this->text_btn);
		}
	}

	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (!gui_scope) return;
		this->SetWidgetDirty(WID_OSK_TEXT);
		this->parent->SetWidgetDirty(this->text_btn);
	}

	void OnFocusLost(bool closing) override
	{
		VideoDriver::GetInstance()->EditBoxLostFocus();
		if (!closing) this->Close();
	}
};

static const int HALF_KEY_WIDTH = 7;  // Width of 1/2 key in pixels.
static const int INTER_KEY_SPACE = 2; // Number of pixels between two keys.

static const int TOP_KEY_PADDING = 2; // Vertical padding for the top row of keys.
static const int KEY_PADDING = 6;     // Vertical padding for remaining key rows.

/**
 * Add a key widget to a row of the keyboard.
 * @param hor     Row container to add key widget to.
 * @param pad_y   Vertical padding of the key (all keys in a row should have equal padding).
 * @param num_half Number of 1/2 key widths that this key has.
 * @param widtype Widget type of the key. Must be either \c NWID_SPACER for an invisible key, or a \c WWT_* widget.
 * @param widnum  Widget number of the key.
 * @param widdata Data value of the key widget.
 * @note Key width is measured in 1/2 keys to allow for 1/2 key shifting between rows.
 */
static void AddKey(std::unique_ptr<NWidgetHorizontal> &hor, int pad_y, int num_half, WidgetType widtype, WidgetID widnum, uint16_t widdata)
{
	int key_width = HALF_KEY_WIDTH + (INTER_KEY_SPACE + HALF_KEY_WIDTH) * (num_half - 1);

	if (widtype == NWID_SPACER) {
		if (!hor->IsEmpty()) key_width += INTER_KEY_SPACE;
		auto spc = std::make_unique<NWidgetSpacer>(key_width, 0);
		spc->SetMinimalTextLines(1, pad_y, FS_NORMAL);
		hor->Add(std::move(spc));
	} else {
		if (!hor->IsEmpty()) {
			auto spc = std::make_unique<NWidgetSpacer>(INTER_KEY_SPACE, 0);
			spc->SetMinimalTextLines(1, pad_y, FS_NORMAL);
			hor->Add(std::move(spc));
		}
		auto leaf = std::make_unique<NWidgetLeaf>(widtype, COLOUR_GREY, widnum, widdata, STR_NULL);
		leaf->SetMinimalSize(key_width, 0);
		leaf->SetMinimalTextLines(1, pad_y, FS_NORMAL);
		hor->Add(std::move(leaf));
	}
}

/** Construct the top row keys (cancel, ok, backspace). */
static std::unique_ptr<NWidgetBase> MakeTopKeys()
{
	auto hor = std::make_unique<NWidgetHorizontal>();

	AddKey(hor, TOP_KEY_PADDING, 6 * 2, WWT_TEXTBTN,    WID_OSK_CANCEL,    STR_BUTTON_CANCEL);
	AddKey(hor, TOP_KEY_PADDING, 6 * 2, WWT_TEXTBTN,    WID_OSK_OK,        STR_BUTTON_OK    );
	AddKey(hor, TOP_KEY_PADDING, 2 * 2, WWT_PUSHIMGBTN, WID_OSK_BACKSPACE, SPR_OSK_BACKSPACE);
	return hor;
}

/** Construct the row containing the digit keys. */
static std::unique_ptr<NWidgetBase> MakeNumberKeys()
{
	std::unique_ptr<NWidgetHorizontal> hor = std::make_unique<NWidgetHorizontalLTR>();

	for (WidgetID widnum = WID_OSK_NUMBERS_FIRST; widnum <= WID_OSK_NUMBERS_LAST; widnum++) {
		AddKey(hor, KEY_PADDING, 2, WWT_PUSHBTN, widnum, 0x0);
	}
	return hor;
}

/** Construct the qwerty row keys. */
static std::unique_ptr<NWidgetBase> MakeQwertyKeys()
{
	std::unique_ptr<NWidgetHorizontal> hor = std::make_unique<NWidgetHorizontalLTR>();

	AddKey(hor, KEY_PADDING, 3, WWT_PUSHIMGBTN, WID_OSK_SPECIAL, SPR_OSK_SPECIAL);
	for (WidgetID widnum = WID_OSK_QWERTY_FIRST; widnum <= WID_OSK_QWERTY_LAST; widnum++) {
		AddKey(hor, KEY_PADDING, 2, WWT_PUSHBTN, widnum, 0x0);
	}
	AddKey(hor, KEY_PADDING, 1, NWID_SPACER, 0, 0);
	return hor;
}

/** Construct the asdfg row keys. */
static std::unique_ptr<NWidgetBase> MakeAsdfgKeys()
{
	std::unique_ptr<NWidgetHorizontal> hor = std::make_unique<NWidgetHorizontalLTR>();

	AddKey(hor, KEY_PADDING, 4, WWT_IMGBTN, WID_OSK_CAPS, SPR_OSK_CAPS);
	for (WidgetID widnum = WID_OSK_ASDFG_FIRST; widnum <= WID_OSK_ASDFG_LAST; widnum++) {
		AddKey(hor, KEY_PADDING, 2, WWT_PUSHBTN, widnum, 0x0);
	}
	return hor;
}

/** Construct the zxcvb row keys. */
static std::unique_ptr<NWidgetBase> MakeZxcvbKeys()
{
	std::unique_ptr<NWidgetHorizontal> hor = std::make_unique<NWidgetHorizontalLTR>();

	AddKey(hor, KEY_PADDING, 3, WWT_IMGBTN, WID_OSK_SHIFT, SPR_OSK_SHIFT);
	for (WidgetID widnum = WID_OSK_ZXCVB_FIRST; widnum <= WID_OSK_ZXCVB_LAST; widnum++) {
		AddKey(hor, KEY_PADDING, 2, WWT_PUSHBTN, widnum, 0x0);
	}
	AddKey(hor, KEY_PADDING, 1, NWID_SPACER, 0, 0);
	return hor;
}

/** Construct the spacebar row keys. */
static std::unique_ptr<NWidgetBase> MakeSpacebarKeys()
{
	auto hor = std::make_unique<NWidgetHorizontal>();

	AddKey(hor, KEY_PADDING,  8, NWID_SPACER, 0, 0);
	AddKey(hor, KEY_PADDING, 13, WWT_PUSHTXTBTN, WID_OSK_SPACE, STR_EMPTY);
	AddKey(hor, KEY_PADDING,  3, NWID_SPACER, 0, 0);
	AddKey(hor, KEY_PADDING,  2, WWT_PUSHIMGBTN, WID_OSK_LEFT,  SPR_OSK_LEFT);
	AddKey(hor, KEY_PADDING,  2, WWT_PUSHIMGBTN, WID_OSK_RIGHT, SPR_OSK_RIGHT);
	return hor;
}


static const NWidgetPart _nested_osk_widgets[] = {
	NWidget(WWT_CAPTION, COLOUR_GREY, WID_OSK_CAPTION), SetDataTip(STR_JUST_STRING, STR_NULL), SetTextStyle(TC_WHITE),
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

static WindowDesc _osk_desc(__FILE__, __LINE__,
	WDP_CENTER, nullptr, 0, 0,
	WC_OSK, WC_NONE,
	0,
	std::begin(_nested_osk_widgets), std::end(_nested_osk_widgets)
);

/**
 * Retrieve keyboard layout from language string or (if set) config file.
 * Also check for invalid characters.
 */
void GetKeyboardLayout()
{
	std::string keyboard[2];
	std::string errormark[2]; // used for marking invalid chars
	bool has_error = false; // true when an invalid char is detected

	keyboard[0] = _keyboard_opt[0].empty() ? GetString(STR_OSK_KEYBOARD_LAYOUT) : _keyboard_opt[0];
	keyboard[1] = _keyboard_opt[1].empty() ? GetString(STR_OSK_KEYBOARD_LAYOUT_CAPS) : _keyboard_opt[1];

	for (uint j = 0; j < 2; j++) {
		auto kbd = keyboard[j].begin();
		bool ended = false;
		for (uint i = 0; i < OSK_KEYBOARD_ENTRIES; i++) {
			_keyboard[j][i] = Utf8Consume(kbd);

			/* Be lenient when the last characters are missing (is quite normal) */
			if (_keyboard[j][i] == '\0' || ended) {
				ended = true;
				_keyboard[j][i] = ' ';
				continue;
			}

			if (IsPrintable(_keyboard[j][i])) {
				errormark[j] += ' ';
			} else {
				has_error = true;
				errormark[j] += '^';
				_keyboard[j][i] = ' ';
			}
		}
	}

	if (has_error) {
		ShowInfo("The keyboard layout you selected contains invalid chars. Please check those chars marked with ^.");
		ShowInfo("Normal keyboard:  {}", keyboard[0]);
		ShowInfo("                  {}", errormark[0]);
		ShowInfo("Caps Lock:        {}", keyboard[1]);
		ShowInfo("                  {}", errormark[1]);
	}
}

/**
 * Show the on-screen keyboard (osk) associated with a given textbox
 * @param parent pointer to the Window where this keyboard originated from
 * @param button widget number of parent's textbox
 */
void ShowOnScreenKeyboard(Window *parent, WidgetID button)
{
	CloseWindowById(WC_OSK, 0);

	GetKeyboardLayout();
	new OskWindow(&_osk_desc, parent, button);
}

/**
 * Updates the original text of the OSK so when the 'parent' changes the
 * original and you press on cancel you won't get the 'old' original text
 * but the updated one.
 * @param parent window that just updated its original text
 * @param button widget number of parent's textbox to update
 */
void UpdateOSKOriginalText(const Window *parent, WidgetID button)
{
	OskWindow *osk = dynamic_cast<OskWindow *>(FindWindowById(WC_OSK, 0));
	if (osk == nullptr || osk->parent != parent || osk->text_btn != button) return;

	osk->orig_str = osk->qs->text.buf;

	osk->SetDirty();
}

/**
 * Check whether the OSK is opened for a specific editbox.
 * @param w Window to check for
 * @param button Editbox of \a w to check for
 * @return true if the OSK is opened for \a button.
 */
bool IsOSKOpenedFor(const Window *w, WidgetID button)
{
	OskWindow *osk = dynamic_cast<OskWindow *>(FindWindowById(WC_OSK, 0));
	return osk != nullptr && osk->parent == w && osk->text_btn == button;
}
