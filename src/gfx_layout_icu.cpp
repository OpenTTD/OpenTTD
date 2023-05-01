/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file gfx_layout_icu.cpp Handling of laying out text with ICU. */

#include "stdafx.h"

#include "gfx_layout_icu.h"

#include <unicode/ustring.h>

#include "safeguards.h"

/* Implementation details of LEFontInstance */

le_int32 Font::getUnitsPerEM() const
{
	return this->fc->GetUnitsPerEM();
}

le_int32 Font::getAscent() const
{
	return this->fc->GetAscender();
}

le_int32 Font::getDescent() const
{
	return -this->fc->GetDescender();
}

le_int32 Font::getLeading() const
{
	return this->fc->GetHeight();
}

float Font::getXPixelsPerEm() const
{
	return (float)this->fc->GetHeight();
}

float Font::getYPixelsPerEm() const
{
	return (float)this->fc->GetHeight();
}

float Font::getScaleFactorX() const
{
	return 1.0f;
}

float Font::getScaleFactorY() const
{
	return 1.0f;
}

const void *Font::getFontTable(LETag tableTag) const
{
	size_t length;
	return this->getFontTable(tableTag, length);
}

const void *Font::getFontTable(LETag tableTag, size_t &length) const
{
	return this->fc->GetFontTable(tableTag, length);
}

LEGlyphID Font::mapCharToGlyph(LEUnicode32 ch) const
{
	if (IsTextDirectionChar(ch)) return 0;
	return this->fc->MapCharToGlyph(ch);
}

void Font::getGlyphAdvance(LEGlyphID glyph, LEPoint &advance) const
{
	advance.fX = glyph == 0xFFFF ? 0 : this->fc->GetGlyphWidth(glyph);
	advance.fY = 0;
}

le_bool Font::getGlyphPoint(LEGlyphID glyph, le_int32 pointNumber, LEPoint &point) const
{
	return false;
}

/**
 * Wrapper for doing layouts with ICU.
 */
class ICUParagraphLayout : public ParagraphLayouter {
	icu::ParagraphLayout *p; ///< The actual ICU paragraph layout.
public:
	/** Visual run contains data about the bit of text with the same font. */
	class ICUVisualRun : public ParagraphLayouter::VisualRun {
		const icu::ParagraphLayout::VisualRun *vr; ///< The actual ICU vr.

	public:
		ICUVisualRun(const icu::ParagraphLayout::VisualRun *vr) : vr(vr) { }

		const Font *GetFont() const override          { return (const Font*)vr->getFont(); }
		int GetGlyphCount() const override            { return vr->getGlyphCount(); }
		const GlyphID *GetGlyphs() const override     { return vr->getGlyphs(); }
		const float *GetPositions() const override    { return vr->getPositions(); }
		int GetLeading() const override               { return vr->getLeading(); }
		const int *GetGlyphToCharMap() const override { return vr->getGlyphToCharMap(); }
	};

	/** A single line worth of VisualRuns. */
	class ICULine : public std::vector<ICUVisualRun>, public ParagraphLayouter::Line {
		icu::ParagraphLayout::Line *l; ///< The actual ICU line.

	public:
		ICULine(icu::ParagraphLayout::Line *l) : l(l)
		{
			for (int i = 0; i < l->countRuns(); i++) {
				this->emplace_back(l->getVisualRun(i));
			}
		}
		~ICULine() override { delete l; }

		int GetLeading() const override { return l->getLeading(); }
		int GetWidth() const override   { return l->getWidth(); }
		int CountRuns() const override  { return l->countRuns(); }
		const ParagraphLayouter::VisualRun &GetVisualRun(int run) const override { return this->at(run); }

		int GetInternalCharLength(WChar c) const override
		{
			/* ICU uses UTF-16 internally which means we need to account for surrogate pairs. */
			return Utf8CharLen(c) < 4 ? 1 : 2;
		}
	};

	ICUParagraphLayout(icu::ParagraphLayout *p) : p(p) { }
	~ICUParagraphLayout() override { delete p; }
	void Reflow() override  { p->reflow(); }

	std::unique_ptr<const Line> NextLine(int max_width) override
	{
		icu::ParagraphLayout::Line *l = p->nextLine(max_width);
		return std::unique_ptr<const Line>(l == nullptr ? nullptr : new ICULine(l));
	}
};

/* static */ ParagraphLayouter *ICUParagraphLayoutFactory::GetParagraphLayout(UChar *buff, UChar *buff_end, FontMap &fontMapping)
{
	int32 length = buff_end - buff;

	if (length == 0) {
		/* ICU's ParagraphLayout cannot handle empty strings, so fake one. */
		buff[0] = ' ';
		length = 1;
		fontMapping.back().first++;
	}

	/* Fill ICU's FontRuns with the right data. */
	icu::FontRuns runs(fontMapping.size());
	for (auto &pair : fontMapping) {
		runs.add(pair.second, pair.first);
	}

	LEErrorCode status = LE_NO_ERROR;
	/* ParagraphLayout does not copy "buff", so it must stay valid.
		* "runs" is copied according to the ICU source, but the documentation does not specify anything, so this might break somewhen. */
	icu::ParagraphLayout *p = new icu::ParagraphLayout(buff, length, &runs, nullptr, nullptr, nullptr, _current_text_dir == TD_RTL ? 1 : 0, false, status);
	if (status != LE_NO_ERROR) {
		delete p;
		return nullptr;
	}

	return new ICUParagraphLayout(p);
}

/* static */ size_t ICUParagraphLayoutFactory::AppendToBuffer(UChar *buff, const UChar *buffer_last, WChar c)
{
	/* Transform from UTF-32 to internal ICU format of UTF-16. */
	int32 length = 0;
	UErrorCode err = U_ZERO_ERROR;
	u_strFromUTF32(buff, buffer_last - buff, &length, (UChar32*)&c, 1, &err);
	return length;
}
