/* $Id$ */

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

/** Helper/buffer for input fields. */
struct Textbuf {
	char *buf;                ///< buffer in which text is saved
	uint16 max_bytes;         ///< the maximum size of the buffer in bytes (including terminating '\0')
	uint16 max_chars;         ///< the maximum size of the buffer in characters (including terminating '\0')
	uint16 bytes;             ///< the current size of the string in bytes (including terminating '\0')
	uint16 chars;             ///< the current size of the string in characters (including terminating '\0')
	uint16 pixels;            ///< the current size of the string in pixels
	bool caret;               ///< is the caret ("_") visible or not
	uint16 caretpos;          ///< the current position of the caret in the buffer, in bytes
	uint16 caretxoffs;        ///< the current position of the caret in pixels

	void Initialize(char *buf, uint16 max_bytes);
	void Initialize(char *buf, uint16 max_bytes, uint16 max_chars);

	void Assign(StringID string);
	void Assign(const char *text);
	void CDECL Print(const char *format, ...) WARN_FORMAT(2, 3);

	void DeleteAll();
	bool DeleteChar(int delmode);
	bool InsertChar(uint32 key);
	bool InsertClipboard();
	bool MovePos(int navmode);

	bool HandleCaret();
	void UpdateSize();

private:
	bool CanDelChar(bool backspace);
	WChar GetNextDelChar(bool backspace);
	void DelChar(bool backspace);
	bool CanMoveCaretLeft();
	WChar MoveCaretLeft();
	bool CanMoveCaretRight();
	WChar MoveCaretRight();

};

#endif /* TEXTBUF_TYPE_H */
