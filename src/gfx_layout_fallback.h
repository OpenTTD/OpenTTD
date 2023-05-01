/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file gfx_layout_fallback.h Functions related to laying out the texts as fallback. */

#ifndef GFX_LAYOUT_FALLBACK_H
#define GFX_LAYOUT_FALLBACK_H

#include "gfx_layout.h"

/**
 * Helper class to construct a new #FallbackParagraphLayout.
 */
class FallbackParagraphLayoutFactory {
public:
	/** Helper for GetLayouter, to get the right type. */
	typedef WChar CharType;
	/** Helper for GetLayouter, to get whether the layouter supports RTL. */
	static const bool SUPPORTS_RTL = false;

	static ParagraphLayouter *GetParagraphLayout(WChar *buff, WChar *buff_end, FontMap &fontMapping);
	static size_t AppendToBuffer(WChar *buff, const WChar *buffer_last, WChar c);
};


#endif /* GFX_LAYOUT_FALLBACK_H */
