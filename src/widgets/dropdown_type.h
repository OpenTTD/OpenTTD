/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file dropdown_type.h Types related to the drop down widget. */

#ifndef WIDGETS_DROPDOWN_TYPE_H
#define WIDGETS_DROPDOWN_TYPE_H

#include "../window_type.h"
#include "../gfx_func.h"
#include "../gfx_type.h"
#include "../palette_func.h"
#include "../string_func.h"
#include "../strings_func.h"
#include "../table/strings.h"
#include "../window_gui.h"

/**
 * Base list item class from which others are derived.
 */
class DropDownListItem {
public:
	int result; ///< Result value to return to window on selection.
	bool masked; ///< Masked and unselectable item.
	bool shaded; ///< Shaded item, affects text colour.

	explicit DropDownListItem(int result, bool masked = false, bool shaded = false) : result(result), masked(masked), shaded(shaded) {}
	virtual ~DropDownListItem() = default;

	virtual bool Selectable() const { return true; }
	virtual uint Height() const { return 0; }
	virtual uint Width() const { return 0; }

	virtual void Draw(const Rect &full, const Rect &, bool, Colours bg_colour) const
	{
		if (this->masked) GfxFillRect(full, _colour_gradient[bg_colour][5], FILLRECT_CHECKER);
	}

	TextColour GetColour(bool sel) const
	{
		if (this->shaded) return (sel ? TC_SILVER : TC_GREY) | TC_NO_SHADE;
		return sel ? TC_WHITE : TC_BLACK;
	}
};

/**
 * Drop down divider component.
 * @tparam TBase Base component.
 * @tparam TFs Font size -- used to determine height.
 */
template<class TBase, FontSize TFs = FS_NORMAL>
class DropDownDivider : public TBase {
public:
	template <typename... Args>
	explicit DropDownDivider(Args&&... args) : TBase(std::forward<Args>(args)...) {}

	bool Selectable() const override { return false; }
	uint Height() const override { return std::max<uint>(GetCharacterHeight(TFs), this->TBase::Height()); }

	void Draw(const Rect &full, const Rect &, bool, Colours bg_colour) const override
	{
		uint8_t c1 = _colour_gradient[bg_colour][3];
		uint8_t c2 = _colour_gradient[bg_colour][7];

		int mid = CenterBounds(full.top, full.bottom, 0);
		GfxFillRect(full.left, mid - WidgetDimensions::scaled.bevel.bottom, full.right, mid - 1, c1);
		GfxFillRect(full.left, mid, full.right, mid + WidgetDimensions::scaled.bevel.top - 1, c2);
	}
};

/**
 * Drop down string component.
 * @tparam TBase Base component.
 * @tparam TFs Font size.
 * @tparam TEnd Position string at end if true, or start if false.
 */
template<class TBase, FontSize TFs = FS_NORMAL, bool TEnd = false>
class DropDownString : public TBase {
	std::string string; ///< String to be drawn.
	Dimension dim; ///< Dimensions of string.
public:
	template <typename... Args>
	explicit DropDownString(StringID string, Args&&... args) : TBase(std::forward<Args>(args)...)
	{
		this->SetString(GetString(string));
	}

	template <typename... Args>
	explicit DropDownString(const std::string &string, Args&&... args) : TBase(std::forward<Args>(args)...)
	{
		SetDParamStr(0, string);
		this->SetString(GetString(STR_JUST_RAW_STRING));
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

	void Draw(const Rect &full, const Rect &r, bool sel, Colours bg_colour) const override
	{
		bool rtl = TEnd ^ (_current_text_dir == TD_RTL);
		DrawStringMultiLine(r.WithWidth(this->dim.width, rtl), this->string, this->GetColour(sel), SA_CENTER, false, TFs);
		this->TBase::Draw(full, r.Indent(this->dim.width, rtl), sel, bg_colour);
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
template<class TBase, bool TEnd = false>
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

	void Draw(const Rect &full, const Rect &r, bool sel, Colours bg_colour) const override
	{
		bool rtl = TEnd ^ (_current_text_dir == TD_RTL);
		Rect ir = r.WithWidth(this->dbounds.width, rtl);
		DrawSprite(this->sprite, this->palette, CenterBounds(ir.left, ir.right, this->dsprite.width), CenterBounds(r.top, r.bottom, this->dsprite.height));
		this->TBase::Draw(full, r.Indent(this->dbounds.width + WidgetDimensions::scaled.hsep_normal, rtl), sel, bg_colour);
	}
};

/**
 * Drop down checkmark component.
 * @tparam TBase Base component.
 * @tparam TFs Font size.
 * @tparam TEnd Position checkmark at end if true, or start if false.
 */
template<class TBase, bool TEnd = false, FontSize TFs = FS_NORMAL>
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

	void Draw(const Rect &full, const Rect &r, bool sel, Colours bg_colour) const override
	{
		bool rtl = TEnd ^ (_current_text_dir == TD_RTL);
		if (this->checked) {
			DrawStringMultiLine(r.WithWidth(this->dim.width, rtl), STR_JUST_CHECKMARK, this->GetColour(sel), SA_CENTER, false, TFs);
		}
		this->TBase::Draw(full, r.Indent(this->dim.width + WidgetDimensions::scaled.hsep_wide, rtl), sel, bg_colour);
	}
};

/* Commonly used drop down list items. */
using DropDownListDividerItem = DropDownDivider<DropDownListItem>;
using DropDownListStringItem = DropDownString<DropDownListItem>;
using DropDownListIconItem = DropDownIcon<DropDownString<DropDownListItem>>;
using DropDownListCheckedItem = DropDownCheck<DropDownString<DropDownListItem>>;

/**
 * A drop down list is a collection of drop down list items.
 */
typedef std::vector<std::unique_ptr<const DropDownListItem>> DropDownList;

void ShowDropDownListAt(Window *w, DropDownList &&list, int selected, WidgetID button, Rect wi_rect, Colours wi_colour, bool instant_close = false);

void ShowDropDownList(Window *w, DropDownList &&list, int selected, WidgetID button, uint width = 0, bool instant_close = false);

Dimension GetDropDownListDimension(const DropDownList &list);

#endif /* WIDGETS_DROPDOWN_TYPE_H */
