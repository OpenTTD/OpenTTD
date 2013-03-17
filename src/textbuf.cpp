/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file textbuf.cpp Textbuffer handling. */

#include "stdafx.h"
#include <stdarg.h>

#include "textbuf_type.h"
#include "string_func.h"
#include "strings_func.h"
#include "gfx_type.h"
#include "gfx_func.h"
#include "window_func.h"
#include "core/alloc_func.hpp"

/**
 * Try to retrieve the current clipboard contents.
 *
 * @note OS-specific function.
 * @return True if some text could be retrieved.
 */
bool GetClipboardContents(char *buffer, size_t buff_len);

int _caret_timer;


/**
 * Checks if it is possible to delete a character.
 * @param backspace if set, delete the character before the caret,
 * otherwise, delete the character after it.
 * @return true if a character can be deleted in the given direction.
 */
bool Textbuf::CanDelChar(bool backspace)
{
	return backspace ? this->caretpos != 0 : this->caretpos < this->bytes - 1;
}

/**
 * Get the next character that will be removed by DelChar.
 * @param backspace if set, delete the character before the caret,
 * otherwise, delete the character after it.
 * @return the next character that will be removed by DelChar.
 * @warning You should ensure Textbuf::CanDelChar returns true before calling this function.
 */
WChar Textbuf::GetNextDelChar(bool backspace)
{
	assert(this->CanDelChar(backspace));

	const char *s;
	if (backspace) {
		s = Utf8PrevChar(this->buf + this->caretpos);
	} else {
		s = this->buf + this->caretpos;
	}

	WChar c;
	Utf8Decode(&c, s);
	return c;
}

/**
 * Delete a character at the caret position in a text buf.
 * @param backspace if set, delete the character before the caret,
 * else delete the character after it.
 * @warning You should ensure Textbuf::CanDelChar returns true before calling this function.
 */
void Textbuf::DelChar(bool backspace)
{
	assert(this->CanDelChar(backspace));

	WChar c;
	char *s = this->buf + this->caretpos;

	if (backspace) s = Utf8PrevChar(s);

	uint16 len = (uint16)Utf8Decode(&c, s);
	uint width = GetCharacterWidth(FS_NORMAL, c);

	this->pixels -= width;
	if (backspace) {
		this->caretpos   -= len;
		this->caretxoffs -= width;
	}

	/* Move the remaining characters over the marker */
	memmove(s, s + len, this->bytes - (s - this->buf) - len);
	this->bytes -= len;
	this->chars--;
}

/**
 * Delete a character from a textbuffer, either with 'Delete' or 'Backspace'
 * The character is delete from the position the caret is at
 * @param keycode Type of deletion, either WKC_BACKSPACE or WKC_DELETE
 * @return Return true on successful change of Textbuf, or false otherwise
 */
bool Textbuf::DeleteChar(uint16 keycode)
{
	if (keycode == WKC_BACKSPACE || keycode == WKC_DELETE) {
		bool backspace = keycode == WKC_BACKSPACE;
		if (CanDelChar(backspace)) {
			this->DelChar(backspace);
			return true;
		}
		return false;
	}

	if (keycode == (WKC_CTRL | WKC_BACKSPACE) || keycode == (WKC_CTRL | WKC_DELETE)) {
		bool backspace = keycode == (WKC_CTRL | WKC_BACKSPACE);

		if (!CanDelChar(backspace)) return false;
		WChar c = this->GetNextDelChar(backspace);

		/* Backspace: Delete left whitespaces.
		 * Delete:    Delete right word.
		 */
		while (backspace ? IsWhitespace(c) : !IsWhitespace(c)) {
			this->DelChar(backspace);
			if (!this->CanDelChar(backspace)) return true;
			c = this->GetNextDelChar(backspace);
		}
		/* Backspace: Delete left word.
		 * Delete:    Delete right whitespaces.
		 */
		while (backspace ? !IsWhitespace(c) : IsWhitespace(c)) {
			this->DelChar(backspace);
			if (!this->CanDelChar(backspace)) return true;
			c = this->GetNextDelChar(backspace);
		}
		return true;
	}

	return false;
}

/**
 * Delete every character in the textbuffer
 */
void Textbuf::DeleteAll()
{
	memset(this->buf, 0, this->max_bytes);
	this->bytes = this->chars = 1;
	this->pixels = this->caretpos = this->caretxoffs = 0;
}

/**
 * Insert a character to a textbuffer. If maxwidth of the Textbuf is zero,
 * we don't care about the visual-length but only about the physical
 * length of the string
 * @param key Character to be inserted
 * @return Return true on successful change of Textbuf, or false otherwise
 */
bool Textbuf::InsertChar(WChar key)
{
	const byte charwidth = GetCharacterWidth(FS_NORMAL, key);
	uint16 len = (uint16)Utf8CharLen(key);
	if (this->bytes + len <= this->max_bytes && this->chars + 1 <= this->max_chars) {
		memmove(this->buf + this->caretpos + len, this->buf + this->caretpos, this->bytes - this->caretpos);
		Utf8Encode(this->buf + this->caretpos, key);
		this->chars++;
		this->bytes  += len;
		this->pixels += charwidth;

		this->caretpos   += len;
		this->caretxoffs += charwidth;
		return true;
	}
	return false;
}

/**
 * Insert a chunk of text from the clipboard onto the textbuffer. Get TEXT clipboard
 * and append this up to the maximum length (either absolute or screenlength). If maxlength
 * is zero, we don't care about the screenlength but only about the physical length of the string
 * @return true on successful change of Textbuf, or false otherwise
 */
bool Textbuf::InsertClipboard()
{
	char utf8_buf[512];

	if (!GetClipboardContents(utf8_buf, lengthof(utf8_buf))) return false;

	uint16 pixels = 0, bytes = 0, chars = 0;
	WChar c;
	for (const char *ptr = utf8_buf; (c = Utf8Consume(&ptr)) != '\0';) {
		if (!IsValidChar(c, this->afilter)) break;

		byte len = Utf8CharLen(c);
		if (this->bytes + bytes + len > this->max_bytes) break;
		if (this->chars + chars + 1   > this->max_chars) break;

		byte char_pixels = GetCharacterWidth(FS_NORMAL, c);

		pixels += char_pixels;
		bytes += len;
		chars++;
	}

	if (bytes == 0) return false;

	memmove(this->buf + this->caretpos + bytes, this->buf + this->caretpos, this->bytes - this->caretpos);
	memcpy(this->buf + this->caretpos, utf8_buf, bytes);
	this->pixels += pixels;
	this->caretxoffs += pixels;

	this->bytes += bytes;
	this->chars += chars;
	this->caretpos += bytes;
	assert(this->bytes <= this->max_bytes);
	assert(this->chars <= this->max_chars);
	this->buf[this->bytes - 1] = '\0'; // terminating zero

	return true;
}

/**
 * Checks if it is possible to move caret to the left
 * @return true if the caret can be moved to the left, otherwise false.
 */
bool Textbuf::CanMoveCaretLeft()
{
	return this->caretpos != 0;
}

/**
 * Moves the caret to the left.
 * @pre Ensure that Textbuf::CanMoveCaretLeft returns true
 * @return The character under the caret.
 */
WChar Textbuf::MoveCaretLeft()
{
	assert(this->CanMoveCaretLeft());

	WChar c;
	const char *s = Utf8PrevChar(this->buf + this->caretpos);
	Utf8Decode(&c, s);
	this->caretpos    = s - this->buf;
	this->caretxoffs -= GetCharacterWidth(FS_NORMAL, c);

	return c;
}

/**
 * Checks if it is possible to move caret to the right
 * @return true if the caret can be moved to the right, otherwise false.
 */
bool Textbuf::CanMoveCaretRight()
{
	return this->caretpos < this->bytes - 1;
}

/**
 * Moves the caret to the right.
 * @pre Ensure that Textbuf::CanMoveCaretRight returns true
 * @return The character under the caret.
 */
WChar Textbuf::MoveCaretRight()
{
	assert(this->CanMoveCaretRight());

	WChar c;
	this->caretpos   += (uint16)Utf8Decode(&c, this->buf + this->caretpos);
	this->caretxoffs += GetCharacterWidth(FS_NORMAL, c);

	Utf8Decode(&c, this->buf + this->caretpos);
	return c;
}

/**
 * Handle text navigation with arrow keys left/right.
 * This defines where the caret will blink and the next character interaction will occur
 * @param keycode Direction in which navigation occurs (WKC_CTRL |) WKC_LEFT, (WKC_CTRL |) WKC_RIGHT, WKC_END, WKC_HOME
 * @return Return true on successful change of Textbuf, or false otherwise
 */
bool Textbuf::MovePos(uint16 keycode)
{
	switch (keycode) {
		case WKC_LEFT:
			if (this->CanMoveCaretLeft()) {
				this->MoveCaretLeft();
				return true;
			}
			break;

		case WKC_CTRL | WKC_LEFT: {
			if (!this->CanMoveCaretLeft()) break;

			/* Unconditionally move one char to the left. */
			WChar c = this->MoveCaretLeft();
			/* Consume left whitespaces. */
			while (IsWhitespace(c)) {
				if (!this->CanMoveCaretLeft()) return true;
				c = this->MoveCaretLeft();
			}
			/* Consume left word. */
			while (!IsWhitespace(c)) {
				if (!this->CanMoveCaretLeft()) return true;
				c = this->MoveCaretLeft();
			}
			/* Place caret at the beginning of the left word. */
			this->MoveCaretRight();
			return true;
		}

		case WKC_RIGHT:
			if (this->CanMoveCaretRight()) {
				this->MoveCaretRight();
				return true;
			}
			break;

		case WKC_CTRL | WKC_RIGHT: {
			if (!this->CanMoveCaretRight()) break;

			/* Unconditionally move one char to the right. */
			WChar c = this->MoveCaretRight();
			/* Continue to consume current word. */
			while (!IsWhitespace(c)) {
				if (!this->CanMoveCaretRight()) return true;
				c = this->MoveCaretRight();
			}
			/* Consume right whitespaces. */
			while (IsWhitespace(c)) {
				if (!this->CanMoveCaretRight()) return true;
				c = this->MoveCaretRight();
			}
			return true;
		}

		case WKC_HOME:
			this->caretpos = 0;
			this->caretxoffs = 0;
			return true;

		case WKC_END:
			this->caretpos = this->bytes - 1;
			this->caretxoffs = this->pixels;
			return true;

		default:
			break;
	}

	return false;
}

/**
 * Initialize the textbuffer by supplying it the buffer to write into
 * and the maximum length of this buffer
 * @param buf the buffer that will be holding the data for input
 * @param max_bytes maximum size in bytes, including terminating '\0'
 * @param max_chars maximum size in chars, including terminating '\0'
 */
Textbuf::Textbuf(uint16 max_bytes, uint16 max_chars)
	: buf(MallocT<char>(max_bytes))
{
	assert(max_bytes != 0);
	assert(max_chars != 0);

	this->afilter    = CS_ALPHANUMERAL;
	this->max_bytes  = max_bytes;
	this->max_chars  = max_chars == UINT16_MAX ? max_bytes : max_chars;
	this->caret      = true;
	this->DeleteAll();
}

Textbuf::~Textbuf()
{
	free(this->buf);
}

/**
 * Render a string into the textbuffer.
 * @param string String
 */
void Textbuf::Assign(StringID string)
{
	GetString(this->buf, string, &this->buf[this->max_bytes - 1]);
	this->UpdateSize();
}

/**
 * Copy a string into the textbuffer.
 * @param text Source.
 */
void Textbuf::Assign(const char *text)
{
	ttd_strlcpy(this->buf, text, this->max_bytes);
	this->UpdateSize();
}

/**
 * Print a formatted string into the textbuffer.
 */
void Textbuf::Print(const char *format, ...)
{
	va_list va;
	va_start(va, format);
	vsnprintf(this->buf, this->max_bytes, format, va);
	va_end(va);
	this->UpdateSize();
}


/**
 * Update Textbuf type with its actual physical character and screenlength
 * Get the count of characters in the string as well as the width in pixels.
 * Useful when copying in a larger amount of text at once
 */
void Textbuf::UpdateSize()
{
	const char *buf = this->buf;

	this->pixels = 0;
	this->chars = this->bytes = 1; // terminating zero

	WChar c;
	while ((c = Utf8Consume(&buf)) != '\0') {
		this->pixels += GetCharacterWidth(FS_NORMAL, c);
		this->bytes += Utf8CharLen(c);
		this->chars++;
	}

	assert(this->bytes <= this->max_bytes);
	assert(this->chars <= this->max_chars);

	this->caretpos = this->bytes - 1;
	this->caretxoffs = this->pixels;
}

/**
 * Handle the flashing of the caret.
 * @return True if the caret state changes.
 */
bool Textbuf::HandleCaret()
{
	/* caret changed? */
	bool b = !!(_caret_timer & 0x20);

	if (b != this->caret) {
		this->caret = b;
		return true;
	}
	return false;
}
