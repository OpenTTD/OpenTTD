/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file dropdown_common_type.h Common drop down list components. */

#ifndef DROPDOWN_COMMON_TYPE_H
#define DROPDOWN_COMMON_TYPE_H

#include "dropdown_type.h"
#include "gfx_func.h"
#include "gfx_type.h"
#include "palette_func.h"
#include "settings_gui.h"
#include "string_func.h"
#include "stringfilter_type.h"
#include "strings_func.h"
#include "window_gui.h"

#include "table/strings.h"

/**
 * Drop down divider component.
 * @tparam Tbase Base component.
 * @tparam Tfs Font size -- used to determine height.
 */
template <class Tbase, FontSize Tfs = FontSize::Normal>
class DropDownDivider : public Tbase {
public:
	template <typename... Targs>
	explicit DropDownDivider(Targs&&... args) : Tbase(std::forward<Targs>(args)...) {}

	bool Selectable() const override { return false; }
	uint Height() const override { return std::max<uint>(GetCharacterHeight(Tfs), this->Tbase::Height()); }

	void Draw(const Rect &full, const Rect &, bool, int, Colours bg_colour) const override
	{
		PixelColour c1 = GetColourGradient(bg_colour, SHADE_DARK);
		PixelColour c2 = GetColourGradient(bg_colour, SHADE_LIGHTEST);

		int mid = CentreBounds(full.top, full.bottom, 0);
		GfxFillRect(full.WithY(mid - WidgetDimensions::scaled.bevel.bottom, mid - 1), c1);
		GfxFillRect(full.WithY(mid, mid + WidgetDimensions::scaled.bevel.top - 1), c2);
	}
};

/**
 * Drop down string component.
 * @tparam Tbase Base component.
 * @tparam Tfs Font size.
 * @tparam Tend Position string at end if true, or start if false.
 */
template <class Tbase, FontSize Tfs = FontSize::Normal, bool Tend = false>
class DropDownString : public Tbase {
	std::string string; ///< String to be drawn.
	Dimension dim; ///< Dimensions of string.
public:
	template <typename... Targs>
	explicit DropDownString(std::string &&string, Targs&&... args) : Tbase(std::forward<Targs>(args)...)
	{
		this->SetString(std::move(string));
	}

	/** @copydoc DropDownListItem::FilterText */
	void FilterText(StringFilter &string_filter) const override
	{
		string_filter.AddLine(this->string);
		this->Tbase::FilterText(string_filter);
	}

	void SetString(std::string &&string)
	{
		this->string = std::move(string);
		this->dim = GetStringBoundingBox(this->string, Tfs);
	}

	uint Height() const override
	{
		return std::max<uint>(this->dim.height, this->Tbase::Height());
	}

	uint Width() const override { return this->dim.width + this->Tbase::Width(); }

	int OnClick(const Rect &r, const Point &pt) const override
	{
		bool rtl = Tend ^ (_current_text_dir == TD_RTL);
		return this->Tbase::OnClick(r.Indent(this->dim.width, rtl), pt);
	}

	void Draw(const Rect &full, const Rect &r, bool sel, int click_result, Colours bg_colour) const override
	{
		bool rtl = Tend ^ (_current_text_dir == TD_RTL);
		DrawStringMultiLine(r.WithWidth(this->dim.width, rtl), this->string, this->GetColour(sel), SA_CENTER, false, Tfs);
		this->Tbase::Draw(full, r.Indent(this->dim.width, rtl), sel, click_result, bg_colour);
	}

	/**
	 * Natural sorting comparator function for DropDownList::sort().
	 * @param first Left side of comparison.
	 * @param second Right side of comparison.
	 * @return true if \a first precedes \a second.
	 * @warning All items in the list need to be derivates of DropDownListStringItem.
	 */
	static bool NatSortFunc(std::unique_ptr<const DropDownListItem> const &first, std::unique_ptr<const DropDownListItem> const &second)
	{
		const std::string &str1 = static_cast<const DropDownString*>(first.get())->string;
		const std::string &str2 = static_cast<const DropDownString*>(second.get())->string;
		return StrNaturalCompare(str1, str2) < 0;
	}
};

/**
 * Drop down icon component.
 * @tparam Tbase Base component.
 * @tparam Tend Position icon at end if true, or start if false.
 */
template <class Tbase, bool Tend = false>
class DropDownIcon : public Tbase {
	SpriteID sprite; ///< Sprite ID to be drawn.
	PaletteID palette; ///< Palette ID to use.
	Dimension dsprite; ///< Bounding box dimensions of sprite.
	Dimension dbounds; ///< Bounding box dimensions of bounds.
public:
	template <typename... Targs>
	explicit DropDownIcon(SpriteID sprite, PaletteID palette, Targs&&... args) : Tbase(std::forward<Targs>(args)...), sprite(sprite), palette(palette)
	{
		this->dsprite = GetSpriteSize(this->sprite);
		this->dbounds = this->dsprite;
	}

	template <typename... Targs>
	explicit DropDownIcon(const Dimension &dim, SpriteID sprite, PaletteID palette, Targs&&... args) : Tbase(std::forward<Targs>(args)...), sprite(sprite), palette(palette), dbounds(dim)
	{
		this->dsprite = GetSpriteSize(this->sprite);
	}

	uint Height() const override { return std::max(this->dbounds.height, this->Tbase::Height()); }
	uint Width() const override { return this->dbounds.width + WidgetDimensions::scaled.hsep_normal + this->Tbase::Width(); }

	int OnClick(const Rect &r, const Point &pt) const override
	{
		bool rtl = Tend ^ (_current_text_dir == TD_RTL);
		return this->Tbase::OnClick(r.Indent(this->dbounds.width + WidgetDimensions::scaled.hsep_normal, rtl), pt);
	}

	void Draw(const Rect &full, const Rect &r, bool sel, int click_result, Colours bg_colour) const override
	{
		bool rtl = Tend ^ (_current_text_dir == TD_RTL);
		Rect ir = r.WithWidth(this->dbounds.width, rtl);
		DrawSprite(this->sprite, this->palette, CentreBounds(ir.left, ir.right, this->dsprite.width), CentreBounds(r.top, r.bottom, this->dsprite.height));
		this->Tbase::Draw(full, r.Indent(this->dbounds.width + WidgetDimensions::scaled.hsep_normal, rtl), sel, click_result, bg_colour);
	}
};

/**
 * Drop down checkmark component.
 * @tparam Tbase Base component.
 * @tparam Tend Position checkmark at end if true, or start if false.
 * @tparam Tfs Font size.
 */
template <class Tbase, bool Tend = false, FontSize Tfs = FontSize::Normal>
class DropDownCheck : public Tbase {
	bool checked; ///< Is item checked.
	Dimension dim; ///< Dimension of checkmark.
public:
	template <typename... Targs>
	explicit DropDownCheck(bool checked, Targs&&... args) : Tbase(std::forward<Targs>(args)...), checked(checked)
	{
		this->dim = GetStringBoundingBox(STR_JUST_CHECKMARK, Tfs);
	}

	uint Height() const override { return std::max<uint>(this->dim.height, this->Tbase::Height()); }
	uint Width() const override { return this->dim.width + WidgetDimensions::scaled.hsep_wide + this->Tbase::Width(); }

	int OnClick(const Rect &r, const Point &pt) const override
	{
		bool rtl = Tend ^ (_current_text_dir == TD_RTL);
		return this->Tbase::OnClick(r.Indent(this->dim.width + WidgetDimensions::scaled.hsep_wide, rtl), pt);
	}

	void Draw(const Rect &full, const Rect &r, bool sel, int click_result, Colours bg_colour) const override
	{
		bool rtl = Tend ^ (_current_text_dir == TD_RTL);
		if (this->checked) {
			DrawStringMultiLine(r.WithWidth(this->dim.width, rtl), STR_JUST_CHECKMARK, this->GetColour(sel), SA_CENTER, false, Tfs);
		}
		this->Tbase::Draw(full, r.Indent(this->dim.width + WidgetDimensions::scaled.hsep_wide, rtl), sel, click_result, bg_colour);
	}
};

/**
 * Drop down boolean toggle component.
 * @tparam Tbase Base component.
 * @tparam Tend Position toggle at end if true, or start if false.
 */
template <class Tbase, bool Tend = false>
class DropDownToggle : public Tbase {
	bool on; ///< Is item on.
	int click; ///< Click result when toggle used.
	Colours button_colour; ///< Colour of toggle button.
	Colours background_colour; ///< Colour of toggle background.
public:
	template <typename... Targs>
	explicit DropDownToggle(bool on, int click, Colours button_colour, Colours background_colour, Targs&&... args)
		: Tbase(std::forward<Targs>(args)...), on(on), click(click), button_colour(button_colour), background_colour(background_colour)
	{
	}

	uint Height() const override
	{
		return std::max<uint>(SETTING_BUTTON_HEIGHT + WidgetDimensions::scaled.vsep_normal, this->Tbase::Height());
	}

	uint Width() const override
	{
		return SETTING_BUTTON_WIDTH + WidgetDimensions::scaled.hsep_wide + this->Tbase::Width();
	}

	int OnClick(const Rect &r, const Point &pt) const override
	{
		bool rtl = Tend ^ (_current_text_dir == TD_RTL);
		int w = SETTING_BUTTON_WIDTH;

		if (r.WithWidth(w, rtl).CentreToHeight(SETTING_BUTTON_HEIGHT).Contains(pt)) return this->click;

		return this->Tbase::OnClick(r.Indent(w + WidgetDimensions::scaled.hsep_wide, rtl), pt);
	}

	void Draw(const Rect &full, const Rect &r, bool sel, int click_result, Colours bg_colour) const override
	{
		bool rtl = Tend ^ (_current_text_dir == TD_RTL);
		int w = SETTING_BUTTON_WIDTH;

		Rect br = r.WithWidth(w, rtl).CentreToHeight(SETTING_BUTTON_HEIGHT);
		DrawBoolButton(br.left, br.top, this->button_colour, this->background_colour, this->on, true);

		this->Tbase::Draw(full, r.Indent(w + WidgetDimensions::scaled.hsep_wide, rtl), sel, click_result, bg_colour);
	}
};

/**
 * Drop down indent component.
 * @tparam Tbase Base component.
 * @tparam Tend Position indent at end if true, or start if false.
 */
template <class Tbase, bool Tend = false>
class DropDownIndent : public Tbase {
	uint indent;
public:
	template <typename... Args>
	explicit DropDownIndent(uint indent, Args&&... args) : Tbase(std::forward<Args>(args)...), indent(indent) {}

	uint Width() const override { return this->indent * WidgetDimensions::scaled.hsep_indent + this->Tbase::Width(); }

	int OnClick(const Rect &r, const Point &pt) const override
	{
		bool rtl = Tend ^ (_current_text_dir == TD_RTL);
		return this->Tbase::OnClick(r.Indent(this->indent * WidgetDimensions::scaled.hsep_indent, rtl), pt);
	}

	void Draw(const Rect &full, const Rect &r, bool sel, int click_result, Colours bg_colour) const override
	{
		bool rtl = Tend ^ (_current_text_dir == TD_RTL);
		this->Tbase::Draw(full, r.Indent(this->indent * WidgetDimensions::scaled.hsep_indent, rtl), sel, click_result, bg_colour);
	}
};

/**
 * Drop down spacer component.
 * @tparam Tbase Base component.
 * @tparam Tend Position space at end if true, or start if false.
 */
template <class Tbase, bool Tend = false>
class DropDownSpacer : public Tbase {
public:
	template <typename... Targs>
	explicit DropDownSpacer(Targs&&... args) : Tbase(std::forward<Targs>(args)...) {}

	uint Width() const override { return WidgetDimensions::scaled.hsep_wide + this->Tbase::Width(); }

	int OnClick(const Rect &r, const Point &pt) const override
	{
		bool rtl = Tend ^ (_current_text_dir == TD_RTL);
		return this->Tbase::OnClick(r.Indent(WidgetDimensions::scaled.hsep_wide, rtl), pt);
	}

	void Draw(const Rect &full, const Rect &r, bool sel, int click_result, Colours bg_colour) const override
	{
		bool rtl = Tend ^ (_current_text_dir == TD_RTL);
		this->Tbase::Draw(full, r.Indent(WidgetDimensions::scaled.hsep_wide, rtl), sel, click_result, bg_colour);
	}
};

/**
 * Drop down component that makes the item unselectable.
 * @tparam Tbase Base component.
 */
template <class Tbase>
class DropDownUnselectable : public Tbase {
public:
	template <typename... Targs>
	explicit DropDownUnselectable(Targs&&... args) : Tbase(std::forward<Targs>(args)...) {}

	bool Selectable() const override { return false; }
};

using DropDownListDividerItem = DropDownDivider<DropDownListItem>; ///< Drop down list item that divides list horizontally into two parts.
using DropDownListStringItem = DropDownString<DropDownListItem>; ///< Drop down list item that contains a single string.
using DropDownListIconItem = DropDownIcon<DropDownString<DropDownListItem>>; ///< Drop down list item that contains a single string and an icon.
using DropDownListCheckedItem = DropDownIndent<DropDownCheck<DropDownString<DropDownListItem>>>; ///< Drop down list item with a single string and a space for tick.

#endif /* DROPDOWN_COMMON_TYPE_H */
