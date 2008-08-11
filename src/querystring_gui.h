/* $Id$ */

/** @file querystring_gui.h Base for the GUIs that have an edit box in them. */

#ifndef QUERYSTRING_GUI_H
#define QUERYSTRING_GUI_H

#include "textbuf_gui.h"
#include "window_gui.h"

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

	void DrawEditBox(Window *w, int wid);
	void HandleEditBox(Window *w, int wid);
	int HandleEditBoxKey(Window *w, int wid, uint16 key, uint16 keycode, Window::EventState &state);
};

struct QueryStringBaseWindow : public Window, public QueryString {
	const size_t edit_str_size;
	char *edit_str_buf;
	char *orig_str_buf;

	QueryStringBaseWindow(size_t size, const WindowDesc *desc, WindowNumber window_number = 0) : Window(desc, window_number), edit_str_size(size)
	{
		this->edit_str_buf = CallocT<char>(size);
	}

	~QueryStringBaseWindow()
	{
		free(this->edit_str_buf);
	}

	void DrawEditBox(int wid);
	void HandleEditBox(int wid);
	int HandleEditBoxKey(int wid, uint16 key, uint16 keycode, EventState &state);
};

void ShowOnScreenKeyboard(QueryStringBaseWindow *parent, int button, int cancel, int ok);

#endif /* QUERYSTRING_GUI_H */
