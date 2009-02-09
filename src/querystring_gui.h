/* $Id$ */

/** @file querystring_gui.h Base for the GUIs that have an edit box in them. */

#ifndef QUERYSTRING_GUI_H
#define QUERYSTRING_GUI_H

#include "textbuf_gui.h"
#include "window_gui.h"

/**
 * Return values for HandleEditBoxKey
 */
enum HandleEditBoxResult
{
	HEBR_EDITING = 0, // Other key pressed.
	HEBR_CONFIRM,     // Return or enter key pressed.
	HEBR_CANCEL,      // Escape key pressed.
	HEBR_NOT_FOCUSED, // Edit box widget not focused.
};

/**
 * Data stored about a string that can be modified in the GUI
 */
struct QueryString {
	StringID caption;
	Textbuf text;
	const char *orig;
	CharSetFilter afilter;
	bool handled;

	/**
	 * Make sure everything gets initialized properly.
	 */
	QueryString() : orig(NULL)
	{
	}

	/**
	 * Make sure everything gets freed.
	 */
	~QueryString()
	{
		free((void*)this->orig);
	}

	bool HasEditBoxFocus(const Window *w, int wid) const;
	void DrawEditBox(Window *w, int wid);
	void HandleEditBox(Window *w, int wid);
	HandleEditBoxResult HandleEditBoxKey(Window *w, int wid, uint16 key, uint16 keycode, Window::EventState &state);
};

struct QueryStringBaseWindow : public Window, public QueryString {
	char *edit_str_buf;
	char *orig_str_buf;
	const uint16 edit_str_size; ///< maximum length of string (in bytes), including terminating '\0'

	QueryStringBaseWindow(uint16 size, const WindowDesc *desc, WindowNumber window_number = 0) : Window(desc, window_number), edit_str_size(size)
	{
		assert(size != 0);
		this->edit_str_buf = CallocT<char>(size);
	}

	~QueryStringBaseWindow()
	{
		free(this->edit_str_buf);
	}

	void DrawEditBox(int wid);
	void HandleEditBox(int wid);
	HandleEditBoxResult HandleEditBoxKey(int wid, uint16 key, uint16 keycode, EventState &state);
	virtual void OnOpenOSKWindow(int wid);
	virtual void OnOSKInput(int wid) {}
};

void ShowOnScreenKeyboard(QueryStringBaseWindow *parent, int button, int cancel, int ok);

#endif /* QUERYSTRING_GUI_H */
