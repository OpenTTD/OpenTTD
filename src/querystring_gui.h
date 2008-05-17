/* $Id$ */

/** @file querystring_gui.h Base for the GUIs that have an edit box in them. */

#ifndef QUERYSTRING_GUI_H
#define QUERYSTRING_GUI_H

#include "textbuf_gui.h"
#include "window_gui.h"

struct QueryString {
	StringID caption;
	Textbuf text;
	const char *orig;
	CharSetFilter afilter;
	bool handled;

	void DrawEditBox(Window *w, int wid);
	void HandleEditBox(Window *w, int wid);
	int HandleEditBoxKey(Window *w, int wid, uint16 key, uint16 keycode, Window::EventState &state);
};

struct QueryStringBaseWindow : public Window, public QueryString {
	char edit_str_buf[64];
	char orig_str_buf[64];

	QueryStringBaseWindow(const WindowDesc *desc, WindowNumber window_number = 0) : Window(desc, window_number)
	{
	}

	void DrawEditBox(int wid);
	void HandleEditBox(int wid);
	int HandleEditBoxKey(int wid, uint16 key, uint16 keycode, EventState &state);
};

void ShowOnScreenKeyboard(QueryStringBaseWindow *parent, int button, int cancel, int ok);

#endif /* QUERYSTRING_GUI_H */
