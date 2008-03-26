/* $Id$ */

/** @file textbuf_gui.h Stuff related to the text buffer GUI. */

#ifndef TEXTBUF_GUI_H
#define TEXTBUF_GUI_H

#include "window_type.h"
#include "string_type.h"
#include "strings_type.h"

struct Textbuf {
	char *buf;                  ///< buffer in which text is saved
	uint16 maxlength, maxwidth; ///< the maximum size of the buffer. Maxwidth specifies screensize in pixels, maxlength is in bytes
	uint16 length, width;       ///< the current size of the string. Width specifies screensize in pixels, length is in bytes
	bool caret;                 ///< is the caret ("_") visible or not
	uint16 caretpos;            ///< the current position of the caret in the buffer, in bytes
	uint16 caretxoffs;          ///< the current position of the caret in pixels
};

struct querystr_d {
	StringID caption;
	Textbuf text;
	const char *orig;
	CharSetFilter afilter;
	bool handled;
};
assert_compile(WINDOW_CUSTOM_SIZE >= sizeof(querystr_d));

extern char _edit_str_buf[64];
extern char _orig_str_buf[lengthof(_edit_str_buf)];

void DrawEditBox(Window *w, querystr_d *string, int wid);
void HandleEditBox(Window *w, querystr_d *string, int wid);
int HandleEditBoxKey(Window *w, querystr_d *string, int wid, WindowEvent *we);
bool HandleCaret(Textbuf *tb);

void DeleteTextBufferAll(Textbuf *tb);
bool DeleteTextBufferChar(Textbuf *tb, int delmode);
bool InsertTextBufferChar(Textbuf *tb, uint32 key);
bool InsertTextBufferClipboard(Textbuf *tb);
bool MoveTextBufferPos(Textbuf *tb, int navmode);
void InitializeTextBuffer(Textbuf *tb, const char *buf, uint16 maxlength, uint16 maxwidth);
void UpdateTextBufferSize(Textbuf *tb);

void ShowQueryString(StringID str, StringID caption, uint maxlen, uint maxwidth, Window *parent, CharSetFilter afilter);
void ShowQuery(StringID caption, StringID message, Window *w, void (*callback)(Window*, bool));

/** The number of 'characters' on the on-screen keyboard. */
static const uint OSK_KEYBOARD_ENTRIES = 50;

/**
 * The number of characters has to be OSK_KEYBOARD_ENTRIES. However, these
 * have to be UTF-8 encoded, which means up to 4 bytes per character.
 * Furthermore the string needs to be '\0'-terminated.
 */
extern char _keyboard_opt[2][OSK_KEYBOARD_ENTRIES * 4 + 1];

void ShowOnScreenKeyboard(Window *parent, querystr_d *q, int button, int cancel, int ok);

#endif /* TEXTBUF_GUI_H */
