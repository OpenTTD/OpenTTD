/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file textbuf.cpp Textbuffer handling. */

#include "stdafx.h"
#include "textbuf_type.h"
#include "string_func.h"
#include "gfx_type.h"
#include "gfx_func.h"
#include "window_func.h"

/**
 * Try to retrive the current clipboard contents.
 *
 * @note OS-specific funtion.
 * @return True if some text could be retrived.
 */
bool GetClipboardContents(char *buffer, size_t buff_len);

int _caret_timer;


/* Delete a character at the caret position in a text buf.
 * If backspace is set, delete the character before the caret,
 * else delete the character after it. */
static void DelChar(Textbuf *tb, bool backspace)
{
	WChar c;
	char *s = tb->buf + tb->caretpos;

	if (backspace) s = Utf8PrevChar(s);

	uint16 len = (uint16)Utf8Decode(&c, s);
	uint width = GetCharacterWidth(FS_NORMAL, c);

	tb->pixels -= width;
	if (backspace) {
		tb->caretpos   -= len;
		tb->caretxoffs -= width;
	}

	/* Move the remaining characters over the marker */
	memmove(s, s + len, tb->bytes - (s - tb->buf) - len);
	tb->bytes -= len;
	tb->chars--;
}

/**
 * Delete a character from a textbuffer, either with 'Delete' or 'Backspace'
 * The character is delete from the position the caret is at
 * @param tb Textbuf type to be changed
 * @param delmode Type of deletion, either WKC_BACKSPACE or WKC_DELETE
 * @return Return true on successful change of Textbuf, or false otherwise
 */
bool DeleteTextBufferChar(Textbuf *tb, int delmode)
{
	if (delmode == WKC_BACKSPACE && tb->caretpos != 0) {
		DelChar(tb, true);
		return true;
	} else if (delmode == WKC_DELETE && tb->caretpos < tb->bytes - 1) {
		DelChar(tb, false);
		return true;
	}

	return false;
}

/**
 * Delete every character in the textbuffer
 * @param tb Textbuf buffer to be emptied
 */
void DeleteTextBufferAll(Textbuf *tb)
{
	memset(tb->buf, 0, tb->max_bytes);
	tb->bytes = tb->chars = 1;
	tb->pixels = tb->caretpos = tb->caretxoffs = 0;
}

/**
 * Insert a character to a textbuffer. If maxwidth of the Textbuf is zero,
 * we don't care about the visual-length but only about the physical
 * length of the string
 * @param tb Textbuf type to be changed
 * @param key Character to be inserted
 * @return Return true on successful change of Textbuf, or false otherwise
 */
bool InsertTextBufferChar(Textbuf *tb, WChar key)
{
	const byte charwidth = GetCharacterWidth(FS_NORMAL, key);
	uint16 len = (uint16)Utf8CharLen(key);
	if (tb->bytes + len <= tb->max_bytes && tb->chars + 1 <= tb->max_chars) {
		memmove(tb->buf + tb->caretpos + len, tb->buf + tb->caretpos, tb->bytes - tb->caretpos);
		Utf8Encode(tb->buf + tb->caretpos, key);
		tb->chars++;
		tb->bytes  += len;
		tb->pixels += charwidth;

		tb->caretpos   += len;
		tb->caretxoffs += charwidth;
		return true;
	}
	return false;
}

/**
 * Insert a chunk of text from the clipboard onto the textbuffer. Get TEXT clipboard
 * and append this up to the maximum length (either absolute or screenlength). If maxlength
 * is zero, we don't care about the screenlength but only about the physical length of the string
 * @param tb Textbuf type to be changed
 * @return true on successful change of Textbuf, or false otherwise
 */
bool InsertTextBufferClipboard(Textbuf *tb)
{
	char utf8_buf[512];

	if (!GetClipboardContents(utf8_buf, lengthof(utf8_buf))) return false;

	uint16 pixels = 0, bytes = 0, chars = 0;
	WChar c;
	for (const char *ptr = utf8_buf; (c = Utf8Consume(&ptr)) != '\0';) {
		if (!IsPrintable(c)) break;

		byte len = Utf8CharLen(c);
		if (tb->bytes + bytes + len > tb->max_bytes) break;
		if (tb->chars + chars + 1   > tb->max_chars) break;

		byte char_pixels = GetCharacterWidth(FS_NORMAL, c);

		pixels += char_pixels;
		bytes += len;
		chars++;
	}

	if (bytes == 0) return false;

	memmove(tb->buf + tb->caretpos + bytes, tb->buf + tb->caretpos, tb->bytes - tb->caretpos);
	memcpy(tb->buf + tb->caretpos, utf8_buf, bytes);
	tb->pixels += pixels;
	tb->caretxoffs += pixels;

	tb->bytes += bytes;
	tb->chars += chars;
	tb->caretpos += bytes;
	assert(tb->bytes <= tb->max_bytes);
	assert(tb->chars <= tb->max_chars);
	tb->buf[tb->bytes - 1] = '\0'; // terminating zero

	return true;
}

/**
 * Handle text navigation with arrow keys left/right.
 * This defines where the caret will blink and the next characer interaction will occur
 * @param tb Textbuf type where navigation occurs
 * @param navmode Direction in which navigation occurs WKC_LEFT, WKC_RIGHT, WKC_END, WKC_HOME
 * @return Return true on successful change of Textbuf, or false otherwise
 */
bool MoveTextBufferPos(Textbuf *tb, int navmode)
{
	switch (navmode) {
		case WKC_LEFT:
			if (tb->caretpos != 0) {
				WChar c;
				const char *s = Utf8PrevChar(tb->buf + tb->caretpos);
				Utf8Decode(&c, s);
				tb->caretpos    = s - tb->buf; // -= (tb->buf + tb->caretpos - s)
				tb->caretxoffs -= GetCharacterWidth(FS_NORMAL, c);

				return true;
			}
			break;

		case WKC_RIGHT:
			if (tb->caretpos < tb->bytes - 1) {
				WChar c;

				tb->caretpos   += (uint16)Utf8Decode(&c, tb->buf + tb->caretpos);
				tb->caretxoffs += GetCharacterWidth(FS_NORMAL, c);

				return true;
			}
			break;

		case WKC_HOME:
			tb->caretpos = 0;
			tb->caretxoffs = 0;
			return true;

		case WKC_END:
			tb->caretpos = tb->bytes - 1;
			tb->caretxoffs = tb->pixels;
			return true;

		default:
			break;
	}

	return false;
}

/**
 * Initialize the textbuffer by supplying it the buffer to write into
 * and the maximum length of this buffer
 * @param tb Textbuf type which is getting initialized
 * @param buf the buffer that will be holding the data for input
 * @param max_bytes maximum size in bytes, including terminating '\0'
 */
void InitializeTextBuffer(Textbuf *tb, char *buf, uint16 max_bytes)
{
	InitializeTextBuffer(tb, buf, max_bytes, max_bytes);
}

/**
 * Initialize the textbuffer by supplying it the buffer to write into
 * and the maximum length of this buffer
 * @param tb Textbuf type which is getting initialized
 * @param buf the buffer that will be holding the data for input
 * @param max_bytes maximum size in bytes, including terminating '\0'
 * @param max_chars maximum size in chars, including terminating '\0'
 */
void InitializeTextBuffer(Textbuf *tb, char *buf, uint16 max_bytes, uint16 max_chars)
{
	assert(max_bytes != 0);
	assert(max_chars != 0);

	tb->buf        = buf;
	tb->max_bytes  = max_bytes;
	tb->max_chars  = max_chars;
	tb->caret      = true;
	UpdateTextBufferSize(tb);
}

/**
 * Update Textbuf type with its actual physical character and screenlength
 * Get the count of characters in the string as well as the width in pixels.
 * Useful when copying in a larger amount of text at once
 * @param tb Textbuf type which length is calculated
 */
void UpdateTextBufferSize(Textbuf *tb)
{
	const char *buf = tb->buf;

	tb->pixels = 0;
	tb->chars = tb->bytes = 1; // terminating zero

	WChar c;
	while ((c = Utf8Consume(&buf)) != '\0') {
		tb->pixels += GetCharacterWidth(FS_NORMAL, c);
		tb->bytes += Utf8CharLen(c);
		tb->chars++;
	}

	assert(tb->bytes <= tb->max_bytes);
	assert(tb->chars <= tb->max_chars);

	tb->caretpos = tb->bytes - 1;
	tb->caretxoffs = tb->pixels;
}

/**
 * Handle the flashing of the caret.
 * @param tb The text buffer to handle the caret of.
 * @return True if the caret state changes.
 */
bool HandleCaret(Textbuf *tb)
{
	/* caret changed? */
	bool b = !!(_caret_timer & 0x20);

	if (b != tb->caret) {
		tb->caret = b;
		return true;
	}
	return false;
}
