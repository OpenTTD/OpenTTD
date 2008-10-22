/* $Id$ */

/** @file textbuf_gui.h Stuff related to the text buffer GUI. */

#ifndef TEXTBUF_GUI_H
#define TEXTBUF_GUI_H

#include "window_type.h"
#include "string_type.h"
#include "strings_type.h"
#include "core/enum_type.hpp"

struct Textbuf {
	char *buf;                ///< buffer in which text is saved
	uint16 maxsize, maxwidth; ///< the maximum size of the buffer. Maxwidth specifies screensize in pixels, maxsize is in bytes (including terminating '\0')
	uint16 size, width;       ///< the current size of the string. Width specifies screensize in pixels, size is in bytes
	bool caret;               ///< is the caret ("_") visible or not
	uint16 caretpos;          ///< the current position of the caret in the buffer, in bytes
	uint16 caretxoffs;        ///< the current position of the caret in pixels
};

bool HandleCaret(Textbuf *tb);

void DeleteTextBufferAll(Textbuf *tb);
bool DeleteTextBufferChar(Textbuf *tb, int delmode);
bool InsertTextBufferChar(Textbuf *tb, uint32 key);
bool InsertTextBufferClipboard(Textbuf *tb);
bool MoveTextBufferPos(Textbuf *tb, int navmode);
void InitializeTextBuffer(Textbuf *tb, char *buf, uint16 maxsize, uint16 maxwidth);
void UpdateTextBufferSize(Textbuf *tb);

/** Flags used in ShowQueryString() call */
enum QueryStringFlags {
	QSF_NONE             =    0,
	QSF_ACCEPT_UNCHANGED = 0x01, ///< return success even when the text didn't change
	QSF_ENABLE_DEFAULT   = 0x02, ///< enable the 'Default' button ("\0" is returned)
};

DECLARE_ENUM_AS_BIT_SET(QueryStringFlags)


typedef void QueryCallbackProc(Window*, bool);

void ShowQueryString(StringID str, StringID caption, uint maxlen, uint maxwidth, Window *parent, CharSetFilter afilter, QueryStringFlags flags);
void ShowQuery(StringID caption, StringID message, Window *w, QueryCallbackProc *callback);

/** The number of 'characters' on the on-screen keyboard. */
static const uint OSK_KEYBOARD_ENTRIES = 50;

/**
 * The number of characters has to be OSK_KEYBOARD_ENTRIES. However, these
 * have to be UTF-8 encoded, which means up to 4 bytes per character.
 * Furthermore the string needs to be '\0'-terminated.
 */
extern char _keyboard_opt[2][OSK_KEYBOARD_ENTRIES * 4 + 1];

#endif /* TEXTBUF_GUI_H */
