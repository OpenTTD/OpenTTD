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
#include "strings_func.h"
#include "gfx_type.h"
#include "gfx_func.h"
#include "window_func.h"
#include "core/alloc_func.hpp"

#include "safeguards.h"

/**
 * Try to retrieve the current clipboard contents.
 *
 * @note OS-specific function.
 * @return The (optional) clipboard contents.
 */
std::optional<std::string> GetClipboardContents();

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
 * Delete a character from a textbuffer, either with 'Delete' or 'Backspace'
 * The character is delete from the position the caret is at
 * @param keycode Type of deletion, either WKC_BACKSPACE or WKC_DELETE
 * @return Return true on successful change of Textbuf, or false otherwise
 */
bool Textbuf::DeleteChar(uint16_t keycode)
{
	bool word = (keycode & WKC_CTRL) != 0;

	keycode &= ~WKC_SPECIAL_KEYS;
	if (keycode != WKC_BACKSPACE && keycode != WKC_DELETE) return false;

	bool backspace = keycode == WKC_BACKSPACE;

	if (!CanDelChar(backspace)) return false;

	char *s = this->buf + this->caretpos;
	uint16_t len = 0;

	if (word) {
		/* Delete a complete word. */
		if (backspace) {
			/* Delete whitespace and word in front of the caret. */
			len = this->caretpos - (uint16_t)this->char_iter->Prev(StringIterator::ITER_WORD);
			s -= len;
		} else {
			/* Delete word and following whitespace following the caret. */
			len = (uint16_t)this->char_iter->Next(StringIterator::ITER_WORD) - this->caretpos;
		}
		/* Update character count. */
		for (const char *ss = s; ss < s + len; Utf8Consume(&ss)) {
			this->chars--;
		}
	} else {
		/* Delete a single character. */
		if (backspace) {
			/* Delete the last code point in front of the caret. */
			s = Utf8PrevChar(s);
			char32_t c;
			len = (uint16_t)Utf8Decode(&c, s);
			this->chars--;
		} else {
			/* Delete the complete character following the caret. */
			len = (uint16_t)this->char_iter->Next(StringIterator::ITER_CHARACTER) - this->caretpos;
			/* Update character count. */
			for (const char *ss = s; ss < s + len; Utf8Consume(&ss)) {
				this->chars--;
			}
		}
	}

	/* Move the remaining characters over the marker */
	memmove(s, s + len, this->bytes - (s - this->buf) - len);
	this->bytes -= len;

	if (backspace) this->caretpos -= len;

	this->UpdateStringIter();
	this->UpdateWidth();
	this->UpdateCaretPosition();
	this->UpdateMarkedText();

	return true;
}

/**
 * Delete every character in the textbuffer
 */
void Textbuf::DeleteAll()
{
	memset(this->buf, 0, this->max_bytes);
	this->bytes = this->chars = 1;
	this->pixels = this->caretpos = this->caretxoffs = 0;
	this->markpos = this->markend = this->markxoffs = this->marklength = 0;
	this->UpdateStringIter();
}

/**
 * Insert a character to a textbuffer. If maxwidth of the Textbuf is zero,
 * we don't care about the visual-length but only about the physical
 * length of the string
 * @param key Character to be inserted
 * @return Return true on successful change of Textbuf, or false otherwise
 */
bool Textbuf::InsertChar(char32_t key)
{
	uint16_t len = (uint16_t)Utf8CharLen(key);
	if (this->bytes + len <= this->max_bytes && this->chars + 1 <= this->max_chars) {
		memmove(this->buf + this->caretpos + len, this->buf + this->caretpos, this->bytes - this->caretpos);
		Utf8Encode(this->buf + this->caretpos, key);
		this->chars++;
		this->bytes    += len;
		this->caretpos += len;

		this->UpdateStringIter();
		this->UpdateWidth();
		this->UpdateCaretPosition();
		this->UpdateMarkedText();
		return true;
	}
	return false;
}

/**
 * Insert a string into the text buffer. If maxwidth of the Textbuf is zero,
 * we don't care about the visual-length but only about the physical
 * length of the string.
 * @param str String to insert.
 * @param marked Replace the currently marked text with the new text.
 * @param caret Move the caret to this point in the insertion string.
 * @param insert_location Position at which to insert the string.
 * @param replacement_end Replace all characters from #insert_location up to this location with the new string.
 * @return True on successful change of Textbuf, or false otherwise.
 */
bool Textbuf::InsertString(const char *str, bool marked, const char *caret, const char *insert_location, const char *replacement_end)
{
	uint16_t insertpos = (marked && this->marklength != 0) ? this->markpos : this->caretpos;
	if (insert_location != nullptr) {
		insertpos = insert_location - this->buf;
		if (insertpos > this->bytes) return false;

		if (replacement_end != nullptr) {
			this->DeleteText(insertpos, replacement_end - this->buf, str == nullptr);
		}
	} else {
		if (marked) this->DiscardMarkedText(str == nullptr);
	}

	if (str == nullptr) return false;

	uint16_t bytes = 0, chars = 0;
	char32_t c;
	for (const char *ptr = str; (c = Utf8Consume(&ptr)) != '\0';) {
		if (!IsValidChar(c, this->afilter)) break;

		byte len = Utf8CharLen(c);
		if (this->bytes + bytes + len > this->max_bytes) break;
		if (this->chars + chars + 1   > this->max_chars) break;

		bytes += len;
		chars++;

		/* Move caret if needed. */
		if (ptr == caret) this->caretpos = insertpos + bytes;
	}

	if (bytes == 0) return false;

	if (marked) {
		this->markpos = insertpos;
		this->markend = insertpos + bytes;
	}

	memmove(this->buf + insertpos + bytes, this->buf + insertpos, this->bytes - insertpos);
	memcpy(this->buf + insertpos, str, bytes);

	this->bytes += bytes;
	this->chars += chars;
	if (!marked && caret == nullptr) this->caretpos += bytes;
	assert(this->bytes <= this->max_bytes);
	assert(this->chars <= this->max_chars);
	this->buf[this->bytes - 1] = '\0'; // terminating zero

	this->UpdateStringIter();
	this->UpdateWidth();
	this->UpdateCaretPosition();
	this->UpdateMarkedText();

	return true;
}

/**
 * Insert a chunk of text from the clipboard onto the textbuffer. Get TEXT clipboard
 * and append this up to the maximum length (either absolute or screenlength). If maxlength
 * is zero, we don't care about the screenlength but only about the physical length of the string
 * @return true on successful change of Textbuf, or false otherwise
 */
bool Textbuf::InsertClipboard()
{
	auto contents = GetClipboardContents();
	if (!contents.has_value()) return false;

	return this->InsertString(contents.value().c_str(), false);
}

/**
 * Delete a part of the text.
 * @param from Start of the text to delete.
 * @param to End of the text to delete.
 * @param update Set to true if the internal state should be updated.
 */
void Textbuf::DeleteText(uint16_t from, uint16_t to, bool update)
{
	uint c = 0;
	const char *s = this->buf + from;
	while (s < this->buf + to) {
		Utf8Consume(&s);
		c++;
	}

	/* Strip marked characters from buffer. */
	memmove(this->buf + from, this->buf + to, this->bytes - to);
	this->bytes -= to - from;
	this->chars -= c;

	auto fixup = [&](uint16_t &pos) {
		if (pos <= from) return;
		if (pos <= to) {
			pos = from;
		} else {
			pos -= to - from;
		}
	};

	/* Fixup caret if needed. */
	fixup(this->caretpos);

	/* Fixup marked text if needed. */
	fixup(this->markpos);
	fixup(this->markend);

	if (update) {
		this->UpdateStringIter();
		this->UpdateCaretPosition();
		this->UpdateMarkedText();
	}
}

/**
 * Discard any marked text.
 * @param update Set to true if the internal state should be updated.
 */
void Textbuf::DiscardMarkedText(bool update)
{
	if (this->markend == 0) return;

	this->DeleteText(this->markpos, this->markend, update);
	this->markpos = this->markend = this->markxoffs = this->marklength = 0;
}

/**
 * Get the current text.
 * @return Current text.
 */
const char *Textbuf::GetText() const
{
	return this->buf;
}

/** Update the character iter after the text has changed. */
void Textbuf::UpdateStringIter()
{
	this->char_iter->SetString(this->buf);
	size_t pos = this->char_iter->SetCurPosition(this->caretpos);
	this->caretpos = pos == StringIterator::END ? 0 : (uint16_t)pos;
}

/** Update pixel width of the text. */
void Textbuf::UpdateWidth()
{
	this->pixels = GetStringBoundingBox(this->buf, FS_NORMAL).width;
}

/** Update pixel position of the caret. */
void Textbuf::UpdateCaretPosition()
{
	this->caretxoffs = this->chars > 1 ? GetCharPosInString(this->buf, this->buf + this->caretpos, FS_NORMAL).x : 0;
}

/** Update pixel positions of the marked text area. */
void Textbuf::UpdateMarkedText()
{
	if (this->markend != 0) {
		this->markxoffs  = GetCharPosInString(this->buf, this->buf + this->markpos, FS_NORMAL).x;
		this->marklength = GetCharPosInString(this->buf, this->buf + this->markend, FS_NORMAL).x - this->markxoffs;
	} else {
		this->markxoffs = this->marklength = 0;
	}
}

/**
 * Handle text navigation with arrow keys left/right.
 * This defines where the caret will blink and the next character interaction will occur
 * @param keycode Direction in which navigation occurs (WKC_CTRL |) WKC_LEFT, (WKC_CTRL |) WKC_RIGHT, WKC_END, WKC_HOME
 * @return Return true on successful change of Textbuf, or false otherwise
 */
bool Textbuf::MovePos(uint16_t keycode)
{
	switch (keycode) {
		case WKC_LEFT:
		case WKC_CTRL | WKC_LEFT: {
			if (this->caretpos == 0) break;

			size_t pos = this->char_iter->Prev(keycode & WKC_CTRL ? StringIterator::ITER_WORD : StringIterator::ITER_CHARACTER);
			if (pos == StringIterator::END) return true;

			this->caretpos = (uint16_t)pos;
			this->UpdateCaretPosition();
			return true;
		}

		case WKC_RIGHT:
		case WKC_CTRL | WKC_RIGHT: {
			if (this->caretpos >= this->bytes - 1) break;

			size_t pos = this->char_iter->Next(keycode & WKC_CTRL ? StringIterator::ITER_WORD : StringIterator::ITER_CHARACTER);
			if (pos == StringIterator::END) return true;

			this->caretpos = (uint16_t)pos;
			this->UpdateCaretPosition();
			return true;
		}

		case WKC_HOME:
			this->caretpos = 0;
			this->char_iter->SetCurPosition(this->caretpos);
			this->UpdateCaretPosition();
			return true;

		case WKC_END:
			this->caretpos = this->bytes - 1;
			this->char_iter->SetCurPosition(this->caretpos);
			this->UpdateCaretPosition();
			return true;

		default:
			break;
	}

	return false;
}

/**
 * Initialize the textbuffer by supplying it the buffer to write into
 * and the maximum length of this buffer
 * @param max_bytes maximum size in bytes, including terminating '\0'
 * @param max_chars maximum size in chars, including terminating '\0'
 */
Textbuf::Textbuf(uint16_t max_bytes, uint16_t max_chars)
	: buf(MallocT<char>(max_bytes)), char_iter(StringIterator::Create())
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
	this->Assign(GetString(string));
}

/**
 * Copy a string into the textbuffer.
 * @param text Source.
 */
void Textbuf::Assign(const std::string_view text)
{
	const char *last_of = &this->buf[this->max_bytes - 1];
	strecpy(this->buf, text.data(), last_of);
	StrMakeValidInPlace(this->buf, last_of, SVS_NONE);

	/* Make sure the name isn't too long for the text buffer in the number of
	 * characters (not bytes). max_chars also counts the '\0' characters. */
	while (Utf8StringLength(this->buf) + 1 > this->max_chars) {
		*Utf8PrevChar(this->buf + strlen(this->buf)) = '\0';
	}

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

	this->chars = this->bytes = 1; // terminating zero

	char32_t c;
	while ((c = Utf8Consume(&buf)) != '\0') {
		this->bytes += Utf8CharLen(c);
		this->chars++;
	}
	assert(this->bytes <= this->max_bytes);
	assert(this->chars <= this->max_chars);

	this->caretpos = this->bytes - 1;
	this->UpdateStringIter();
	this->UpdateWidth();
	this->UpdateMarkedText();

	this->UpdateCaretPosition();
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

HandleKeyPressResult Textbuf::HandleKeyPress(char32_t key, uint16_t keycode)
{
	bool edited = false;

	switch (keycode) {
		case WKC_ESC: return HKPR_CANCEL;

		case WKC_RETURN: case WKC_NUM_ENTER: return HKPR_CONFIRM;

		case (WKC_CTRL | 'V'):
		case (WKC_SHIFT | WKC_INSERT):
			edited = this->InsertClipboard();
			break;

		case (WKC_CTRL | 'U'):
			this->DeleteAll();
			edited = true;
			break;

		case WKC_BACKSPACE: case WKC_DELETE:
		case WKC_CTRL | WKC_BACKSPACE: case WKC_CTRL | WKC_DELETE:
			edited = this->DeleteChar(keycode);
			break;

		case WKC_LEFT: case WKC_RIGHT: case WKC_END: case WKC_HOME:
		case WKC_CTRL | WKC_LEFT: case WKC_CTRL | WKC_RIGHT:
			this->MovePos(keycode);
			break;

		default:
			if (IsValidChar(key, this->afilter)) {
				edited = this->InsertChar(key);
			} else {
				return HKPR_NOT_HANDLED;
			}
			break;
	}

	return edited ? HKPR_EDITING : HKPR_CURSOR;
}
