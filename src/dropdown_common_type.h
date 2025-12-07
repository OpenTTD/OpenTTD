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
#include "strings_func.h"
#include "window_gui.h"

#include "table/strings.h"

/**
 * Drop down divider component.
 * @tparam TBase Base component.
 * @tparam TFs Font size -- used to determine height.
 */
template <class TBase, FontSize TFs = FS_NORMAL>
class DropDownDivider : public TBase {
public:
	template <typename... Args>
	explicit DropDownDivider(Args&&... args) : TBase(std::forward<Args>(args)...) {}

	bool Selectable() const override { return false; }
	uint Height() const override { return std::max<uint>(GetCharacterHeight(TFs), this->TBase::Height()); }

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
 * @tparam TBase Base component.
 * @tparam TFs Font size.
 * @tparam TEnd Position string at end if true, or start if false.
 */
template <class TBase, FontSize TFs = FS_NORMAL, bool TEnd = false>
class DropDownString : public TBase {
	std::string string; ///< String to be drawn.
	Dimension dim; ///< Dimensions of string.
public:
	template <typename... Args>
	explicit DropDownString(std::string &&string, Args&&... args) : TBase(std::forward<Args>(args)...)
	{
		this->SetString(std::move(string));
	}

	void SetString(std::string &&string)
	{
		this->string = std::move(string);
		this->dim = GetStringBoundingBox(this->string, TFs);
	}

	uint Height() const override
	{
		return std::max<uint>(this->dim.height, this->TBase::Height());
	}

	uint Width() const override { return this->dim.width + this->TBase::Width(); }

	int OnClick(const Rect &r, const Point &pt) const override
	{
		bool rtl = TEnd ^ (_current_text_dir == TD_RTL);
		return this->TBase::OnClick(r.Indent(this->dim.width, rtl), pt);
	}

	void Draw(const Rect &full, const Rect &r, bool sel, int click_result, Colours bg_colour) const override
	{
		bool rtl = TEnd ^ (_current_text_dir == TD_RTL);
		DrawStringMultiLine(r.WithWidth(this->dim.width, rtl), this->string, this->GetColour(sel), SA_CENTER, false, TFs);
		this->TBase::Draw(full, r.Indent(this->dim.width, rtl), sel, click_result, bg_colour);
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
 * @tparam TBase Base component.
 * @tparam TEnd Position icon at end if true, or start if false.
 */
template <class TBase, bool TEnd = false>
class DropDownIcon : public TBase {
	SpriteID sprite; ///< Sprite ID to be drawn.
	PaletteID palette; ///< Palette ID to use.
	Dimension dsprite; ///< Bounding box dimensions of sprite.
	Dimension dbounds; ///< Bounding box dimensions of bounds.
public:
	template <typename... Args>
	explicit DropDownIcon(SpriteID sprite, PaletteID palette, Args&&... args) : TBase(std::forward<Args>(args)...), sprite(sprite), palette(palette)
	{
		this->dsprite = GetSpriteSize(this->sprite);
		this->dbounds = this->dsprite;
	}

	template <typename... Args>
	explicit DropDownIcon(const Dimension &dim, SpriteID sprite, PaletteID palette, Args&&... args) : TBase(std::forward<Args>(args)...), sprite(sprite), palette(palette), dbounds(dim)
	{
		this->dsprite = GetSpriteSize(this->sprite);
	}

	uint Height() const override { return std::max(this->dbounds.height, this->TBase::Height()); }
	uint Width() const override { return this->dbounds.width + WidgetDimensions::scaled.hsep_normal + this->TBase::Width(); }

	int OnClick(const Rect &r, const Point &pt) const override
	{
		bool rtl = TEnd ^ (_current_text_dir == TD_RTL);
		return this->TBase::OnClick(r.Indent(this->dbounds.width + WidgetDimensions::scaled.hsep_normal, rtl), pt);
	}

	void Draw(const Rect &full, const Rect &r, bool sel, int click_result, Colours bg_colour) const override
	{
		bool rtl = TEnd ^ (_current_text_dir == TD_RTL);
		Rect ir = r.WithWidth(this->dbounds.width, rtl);
		DrawSprite(this->sprite, this->palette, CentreBounds(ir.left, ir.right, this->dsprite.width), CentreBounds(r.top, r.bottom, this->dsprite.height));
		this->TBase::Draw(full, r.Indent(this->dbounds.width + WidgetDimensions::scaled.hsep_normal, rtl), sel, click_result, bg_colour);
	}
};

/**
 * Drop down checkmark component.
 * @tparam TBase Base component.
 * @tparam TFs Font size.
 * @tparam TEnd Position checkmark at end if true, or start if false.
 */
template <class TBase, bool TEnd = false, FontSize TFs = FS_NORMAL>
class DropDownCheck : public TBase {
	bool checked; ///< Is item checked.
	Dimension dim; ///< Dimension of checkmark.
public:
	template <typename... Args>
	explicit DropDownCheck(bool checked, Args&&... args) : TBase(std::forward<Args>(args)...), checked(checked)
	{
		this->dim = GetStringBoundingBox(STR_JUST_CHECKMARK, TFs);
	}

	uint Height() const override { return std::max<uint>(this->dim.height, this->TBase::Height()); }
	uint Width() const override { return this->dim.width + WidgetDimensions::scaled.hsep_wide + this->TBase::Width(); }

	int OnClick(const Rect &r, const Point &pt) const override
	{
		bool rtl = TEnd ^ (_current_text_dir == TD_RTL);
		return this->TBase::OnClick(r.Indent(this->dim.width + WidgetDimensions::scaled.hsep_wide, rtl), pt);
	}

	void Draw(const Rect &full, const Rect &r, bool sel, int click_result, Colours bg_colour) const override
	{
		bool rtl = TEnd ^ (_current_text_dir == TD_RTL);
		if (this->checked) {
			DrawStringMultiLine(r.WithWidth(this->dim.width, rtl), STR_JUST_CHECKMARK, this->GetColour(sel), SA_CENTER, false, TFs);
		}
		this->TBase::Draw(full, r.Indent(this->dim.width + WidgetDimensions::scaled.hsep_wide, rtl), sel, click_result, bg_colour);
	}
};

/**
 * Drop down boolean toggle component.
 * @tparam TBase Base component.
 * @tparam TEnd Position toggle at end if true, or start if false.
 */
template <class TBase, bool TEnd = false>
class DropDownToggle : public TBase {
	bool on; ///< Is item on.
	int click; ///< Click result when toggle used.
	Colours button_colour; ///< Colour of toggle button.
	Colours background_colour; ///< Colour of toggle background.
public:
	template <typename... Args>
	explicit DropDownToggle(bool on, int click, Colours button_colour, Colours background_colour, Args&&... args)
		: TBase(std::forward<Args>(args)...), on(on), click(click), button_colour(button_colour), background_colour(background_colour)
	{
	}

	uint Height() const override
	{
		return std::max<uint>(SETTING_BUTTON_HEIGHT + WidgetDimensions::scaled.vsep_normal, this->TBase::Height());
	}

	uint Width() const override
	{
		return SETTING_BUTTON_WIDTH + WidgetDimensions::scaled.hsep_wide + this->TBase::Width();
	}

	int OnClick(const Rect &r, const Point &pt) const override
	{
		bool rtl = TEnd ^ (_current_text_dir == TD_RTL);
		int w = SETTING_BUTTON_WIDTH;

		if (r.WithWidth(w, rtl).CentreToHeight(SETTING_BUTTON_HEIGHT).Contains(pt)) return this->click;

		return this->TBase::OnClick(r.Indent(w + WidgetDimensions::scaled.hsep_wide, rtl), pt);
	}

	void Draw(const Rect &full, const Rect &r, bool sel, int click_result, Colours bg_colour) const override
	{
		bool rtl = TEnd ^ (_current_text_dir == TD_RTL);
		int w = SETTING_BUTTON_WIDTH;

		Rect br = r.WithWidth(w, rtl).CentreToHeight(SETTING_BUTTON_HEIGHT);
		DrawBoolButton(br.left, br.top, this->button_colour, this->background_colour, this->on, true);

		this->TBase::Draw(full, r.Indent(w + WidgetDimensions::scaled.hsep_wide, rtl), sel, click_result, bg_colour);
	}
};

/**
 * Drop down indent component.
 * @tparam TBase Base component.
 * @tparam TEnd Position indent at end if true, or start if false.
 */
template <class TBase, bool TEnd = false>
class DropDownIndent : public TBase {
	uint indent;
public:
	template <typename... Args>
	explicit DropDownIndent(uint indent, Args&&... args) : TBase(std::forward<Args>(args)...), indent(indent) {}

	uint Width() const override { return this->indent * WidgetDimensions::scaled.hsep_indent + this->TBase::Width(); }

	int OnClick(const Rect &r, const Point &pt) const override
	{
		bool rtl = TEnd ^ (_current_text_dir == TD_RTL);
		return this->TBase::OnClick(r.Indent(this->indent * WidgetDimensions::scaled.hsep_indent, rtl), pt);
	}

	void Draw(const Rect &full, const Rect &r, bool sel, int click_result, Colours bg_colour) const override
	{
		bool rtl = TEnd ^ (_current_text_dir == TD_RTL);
		this->TBase::Draw(full, r.Indent(this->indent * WidgetDimensions::scaled.hsep_indent, rtl), sel, click_result, bg_colour);
	}
};

/**
 * Drop down spacer component.
 * @tparam TBase Base component.
 * @tparam TEnd Position space at end if true, or start if false.
 */
template <class TBase, bool TEnd = false>
class DropDownSpacer : public TBase {
public:
	template <typename... Args>
	explicit DropDownSpacer(Args&&... args) : TBase(std::forward<Args>(args)...) {}

	uint Width() const override { return WidgetDimensions::scaled.hsep_wide + this->TBase::Width(); }

	int OnClick(const Rect &r, const Point &pt) const override
	{
		bool rtl = TEnd ^ (_current_text_dir == TD_RTL);
		return this->TBase::OnClick(r.Indent(WidgetDimensions::scaled.hsep_wide, rtl), pt);
	}

	void Draw(const Rect &full, const Rect &r, bool sel, int click_result, Colours bg_colour) const override
	{
		bool rtl = TEnd ^ (_current_text_dir == TD_RTL);
		this->TBase::Draw(full, r.Indent(WidgetDimensions::scaled.hsep_wide, rtl), sel, click_result, bg_colour);
	}
};

/**
 * Drop down component that makes the item unselectable.
 * @tparam TBase Base component.
 */
template <class TBase, FontSize TFs = FS_NORMAL>
class DropDownUnselectable : public TBase {
public:
	template <typename... Args>
	explicit DropDownUnselectable(Args&&... args) : TBase(std::forward<Args>(args)...) {}

	bool Selectable() const override { return false; }
};

/* Commonly used drop down list items. */
using DropDownListDividerItem = DropDownDivider<DropDownListItem>;
using DropDownListStringItem = DropDownString<DropDownListItem>;
using DropDownListIconItem = DropDownIcon<DropDownString<DropDownListItem>>;
using DropDownListCheckedItem = DropDownIndent<DropDownCheck<DropDownString<DropDownListItem>>>;

#endif /* DROPDOWN_COMMON_TYPE_H */
