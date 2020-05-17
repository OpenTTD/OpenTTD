/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file textbuf_type.h Stuff related to text buffers. */

#ifndef TEXTBUF_TYPE_H
#define TEXTBUF_TYPE_H

#include "string_type.h"
#include "strings_type.h"
#include "string_base.h"

/**
 * Return values for Textbuf::HandleKeypress
 */
enum HandleKeyPressResult
{
	HKPR_EDITING,     ///< Textbuf content changed.
	HKPR_CURSOR,      ///< Non-text change, e.g. cursor position.
	HKPR_CONFIRM,     ///< Return or enter key pressed.
	HKPR_CANCEL,      ///< Escape key pressed.
	HKPR_NOT_HANDLED, ///< Key does not affect editboxes.
};

/** Helper/buffer for input fields. */
struct Textbuf {
	CharSetFilter afilter;    ///< Allowed characters
	char * const buf;         ///< buffer in which text is saved
	uint16 max_bytes;         ///< the maximum size of the buffer in bytes (including terminating '\0')
	uint16 max_chars;         ///< the maximum size of the buffer in characters (including terminating '\0')
	uint16 bytes;             ///< the current size of the string in bytes (including terminating '\0')
	uint16 chars;             ///< the current size of the string in characters (including terminating '\0')
	uint16 pixels;            ///< the current size of the string in pixels
	bool caret;               ///< is the caret ("_") visible or not
	uint16 caretpos;          ///< the current position of the caret in the buffer, in bytes
	uint16 caretxoffs;        ///< the current position of the caret in pixels
	uint16 markpos;           ///< the start position of the marked area in the buffer, in bytes
	uint16 markend;           ///< the end position of the marked area in the buffer, in bytes
	uint16 markxoffs;         ///< the start position of the marked area in pixels
	uint16 marklength;        ///< the length of the marked area in pixels

	explicit Textbuf(uint16 max_bytes, uint16 max_chars = UINT16_MAX);
	~Textbuf();

	void Assign(StringID string);
	void Assign(const char *text);
	void CDECL Print(const char *format, ...) WARN_FORMAT(2, 3);

	void DeleteAll();
	bool InsertClipboard();

	bool InsertChar(WChar key);
	bool InsertString(const char *str, bool marked, const char *caret = nullptr, const char *insert_location = nullptr, const char *replacement_end = nullptr);

	bool DeleteChar(uint16 keycode);
	bool MovePos(uint16 keycode);

	HandleKeyPressResult HandleKeyPress(WChar key, uint16 keycode);

	bool HandleCaret();
	void UpdateSize();

	void DiscardMarkedText(bool update = true);

private:
	StringIterator *char_iter;

	bool CanDelChar(bool backspace);

	void DeleteText(uint16 from, uint16 to, bool update);

	void UpdateStringIter();
	void UpdateWidth();
	void UpdateCaretPosition();
	void UpdateMarkedText();
};

#endif /* TEXTBUF_TYPE_H */
