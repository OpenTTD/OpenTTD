/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file gfx_layout_icu.h Functions related to laying out the texts with ICU. */

#ifndef GFX_LAYOUT_ICU_H
#define GFX_LAYOUT_ICU_H

#include "gfx_layout.h"

#include <unicode/ustring.h>

/**
 * Helper class to construct a new #ICUParagraphLayout.
 */
class ICUParagraphLayoutFactory {
public:
	/** Helper for GetLayouter, to get the right type. */
	typedef UChar CharType;
	/** Helper for GetLayouter, to get whether the layouter supports RTL. */
	static const bool SUPPORTS_RTL = true;

	static ParagraphLayouter *GetParagraphLayout(UChar *buff, UChar *buff_end, FontMap &fontMapping);
	static size_t AppendToBuffer(UChar *buff, const UChar *buffer_last, char32_t c);
};

#endif /* GFX_LAYOUT_ICU_H */
