 /*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_badge.cpp Functionality for NewGRF badges. */

#include "stdafx.h"
#include "dropdown_type.h"
#include "newgrf.h"
#include "newgrf_badge.h"
#include "newgrf_badge_type.h"
#include "newgrf_spritegroup.h"
#include "stringfilter_type.h"
#include "strings_func.h"
#include "timer/timer_game_calendar.h"
#include "window_gui.h"
#include "zoom_func.h"

#include "table/strings.h"

#include "dropdown_common_type.h"

#include "safeguards.h"

/** Separator to identify badge classes from a label. */
static constexpr char BADGE_CLASS_SEPARATOR = '/';

/** Global state for badge definitions. */
class Badges {
public:
	std::vector<BadgeID> classes; ///< List of known badge classes.
	std::vector<Badge> specs; ///< List of known badges.
};

/** Static instance of badge state. */
static Badges _badges = {};

/**
 * Assign a BadgeClassID to the given badge.
 * @param index Badge ID of badge that should be assigned.
 * @returns new or existing BadgeClassID.
 */
static BadgeClassID GetOrCreateBadgeClass(BadgeID index)
{
	auto it = std::ranges::find(_badges.classes, index);
	if (it == std::end(_badges.classes)) {
		it = _badges.classes.emplace(it, index);
	}

	return static_cast<BadgeClassID>(std::distance(std::begin(_badges.classes), it));
}

/**
 * Reset badges to the default state.
 */
void ResetBadges()
{
	_badges = {};
}

/**
 * Register a badge label and return its global index.
 * @param label Badge label to register.
 * @returns Global index of the badge.
 */
Badge &GetOrCreateBadge(std::string_view label)
{
	/* Check if the label exists. */
	auto it = std::ranges::find(_badges.specs, label, &Badge::label);
	if (it != std::end(_badges.specs)) return *it;

	BadgeClassID class_index;

	/* Extract class. */
	auto sep = label.find_first_of(BADGE_CLASS_SEPARATOR);
	if (sep != std::string_view::npos) {
		/* There is a separator, find (and create if necessary) the class label. */
		class_index = GetOrCreateBadge(label.substr(0, sep)).class_index;
		it = std::end(_badges.specs);
	}

	BadgeID index = BadgeID(std::distance(std::begin(_badges.specs), it));
	if (sep == std::string_view::npos) {
		/* There is no separator, so this badge is a class badge. */
		class_index = GetOrCreateBadgeClass(index);
	}

	it = _badges.specs.emplace(it, label, index, class_index);
	return *it;
}

/**
 * Get a badge if it exists.
 * @param index Index of badge.
 * @returns Badge with specified index, or nullptr if it does not exist.
 */
Badge *GetBadge(BadgeID index)
{
	if (index.base() >= std::size(_badges.specs)) return nullptr;
	return &_badges.specs[index.base()];
}

/**
 * Get a badge by label if it exists.
 * @param label Label of badge.
 * @returns Badge with specified label, or nullptr if it does not exist.
 */
Badge *GetBadgeByLabel(std::string_view label)
{
	auto it = std::ranges::find(_badges.specs, label, &Badge::label);
	if (it == std::end(_badges.specs)) return nullptr;

	return &*it;
}

/**
 * Get the badge class of a badge label.
 * @param label Label to get class of.
 * @returns Badge class index of label.
 */
Badge *GetClassBadge(BadgeClassID class_index)
{
	if (class_index.base() >= std::size(_badges.classes)) return nullptr;
	return GetBadge(_badges.classes[class_index.base()]);
}

/** Resolver for a badge scope. */
struct BadgeScopeResolver : public ScopeResolver {
	const Badge &badge;
	const std::optional<TimerGameCalendar::Date> introduction_date;

	/**
	 * Scope resolver of a badge.
	 * @param ro Surrounding resolver.
	 * @param badge Badge to resolve.
	 * @param introduction_date Introduction date of entity.
	 */
	BadgeScopeResolver(ResolverObject &ro, const Badge &badge, const std::optional<TimerGameCalendar::Date> introduction_date)
		: ScopeResolver(ro), badge(badge), introduction_date(introduction_date) { }

	uint32_t GetVariable(uint8_t variable, [[maybe_unused]] uint32_t parameter, bool &available) const override;
};

/* virtual */ uint32_t BadgeScopeResolver::GetVariable(uint8_t variable, [[maybe_unused]] uint32_t parameter, bool &available) const
{
	switch (variable) {
		case 0x40:
			if (this->introduction_date.has_value()) return this->introduction_date->base();
			return TimerGameCalendar::date.base();

		default: break;
	}

	available = false;
	return UINT_MAX;
}

/** Resolver of badges. */
struct BadgeResolverObject : public ResolverObject {
	BadgeScopeResolver self_scope;

	BadgeResolverObject(const Badge &badge, GrfSpecFeature feature, std::optional<TimerGameCalendar::Date> introduction_date, CallbackID callback = CBID_NO_CALLBACK, uint32_t callback_param1 = 0, uint32_t callback_param2 = 0);

	ScopeResolver *GetScope(VarSpriteGroupScope scope = VSG_SCOPE_SELF, uint8_t relative = 0) override
	{
		switch (scope) {
			case VSG_SCOPE_SELF: return &this->self_scope;
			default: return ResolverObject::GetScope(scope, relative);
		}
	}

	GrfSpecFeature GetFeature() const override;
	uint32_t GetDebugID() const override;
};

GrfSpecFeature BadgeResolverObject::GetFeature() const
{
	return GSF_BADGES;
}

uint32_t BadgeResolverObject::GetDebugID() const
{
	return this->self_scope.badge.index.base();
}

/**
 * Constructor of the badge resolver.
 * @param badge Badge being resolved.
 * @param feature GRF feature being used.
 * @param introduction_date Optional introduction date of entity.
 * @param callback Callback ID.
 * @param callback_param1 First parameter (var 10) of the callback.
 * @param callback_param2 Second parameter (var 18) of the callback.
 */
BadgeResolverObject::BadgeResolverObject(const Badge &badge, GrfSpecFeature feature, std::optional<TimerGameCalendar::Date> introduction_date, CallbackID callback, uint32_t callback_param1, uint32_t callback_param2)
		: ResolverObject(badge.grf_prop.grffile, callback, callback_param1, callback_param2), self_scope(*this, badge, introduction_date)
{
	assert(feature <= GSF_END);
	this->root_spritegroup = this->self_scope.badge.grf_prop.GetSpriteGroup(feature);
	if (this->root_spritegroup == nullptr) this->root_spritegroup = this->self_scope.badge.grf_prop.GetSpriteGroup(GSF_END);
}

/**
 * Test for a matching badge in a list of badges, returning the number of matching bits.
 * @param grffile GRF file of the current varaction.
 * @param badges List of badges to test.
 * @param parameter GRF-local badge index.
 * @returns true iff the badge is present.
 */
uint32_t GetBadgeVariableResult(const GRFFile &grffile, std::span<const BadgeID> badges, uint32_t parameter)
{
	if (parameter >= std::size(grffile.badge_list)) return UINT_MAX;

	BadgeID index = grffile.badge_list[parameter];
	return std::ranges::find(badges, index) != std::end(badges);
}

/**
 * Mark a badge a seen (used) by a feature.
 */
void MarkBadgeSeen(BadgeID index, GrfSpecFeature feature)
{
	Badge *b = GetBadge(index);
	assert(b != nullptr);
	b->features.Set(feature);
}

/**
 * Append copyable badges from a list onto another.
 * Badges must exist and be marked with the Copy flag.
 * @param dst Destination badge list.
 * @param src Source badge list.
 * @param feature Feature of list.
 */
void AppendCopyableBadgeList(std::vector<BadgeID> &dst, std::span<const BadgeID> src, GrfSpecFeature feature)
{
	for (const BadgeID &index : src) {
		/* Is badge already present? */
		if (std::ranges::find(dst, index) != std::end(dst)) continue;

		/* Is badge copyable? */
		Badge *badge = GetBadge(index);
		if (badge == nullptr) continue;
		if (!badge->flags.Test(BadgeFlag::Copy)) continue;

		dst.push_back(index);
		badge->features.Set(feature);
	}
}

/** Apply features from all badges to their badge classes. */
void ApplyBadgeFeaturesToClassBadges()
{
	for (const Badge &badge : _badges.specs) {
		Badge *class_badge = GetClassBadge(badge.class_index);
		assert(class_badge != nullptr);
		class_badge->features.Set(badge.features);
	}
}

static constexpr uint MAX_BADGE_HEIGHT = 12; ///< Maximal height of a badge sprite.
static constexpr uint MAX_BADGE_WIDTH = MAX_BADGE_HEIGHT * 2; ///< Maximal width.

/**
 * Get sprite for the given badge.
 * @param badge Badge being queried.
 * @param feature GRF feature being used.
 * @param introduction_date Introduction date of the item, if it has one.
 * @param remap Palette remap to use if the flag is company-coloured.
 * @returns Custom sprite to draw, or \c 0 if not available.
 */
static PalSpriteID GetBadgeSprite(const Badge &badge, GrfSpecFeature feature, std::optional<TimerGameCalendar::Date> introduction_date, PaletteID remap)
{
	BadgeResolverObject object(badge, feature, introduction_date);
	const SpriteGroup *group = object.Resolve();
	if (group == nullptr) return {0, PAL_NONE};

	PaletteID pal = badge.flags.Test(BadgeFlag::UseCompanyColour) ? remap : PAL_NONE;

	return {group->GetResult(), pal};
}

/**
 * Get the largest badge size (within limits) for a badge class.
 * @param badge_class Badge class.
 * @param feature Feature being used.
 * @returns Largest base size of the badge class for the feature.
 */
static Dimension GetBadgeMaximalDimension(BadgeClassID class_index, GrfSpecFeature feature)
{
	Dimension d = { 0, MAX_BADGE_HEIGHT };

	for (const auto &badge : _badges.specs) {
		if (badge.class_index != class_index) continue;

		PalSpriteID ps = GetBadgeSprite(badge, feature, std::nullopt, PAL_NONE);
		if (ps.sprite == 0) continue;

		d.width = std::max(d.width, GetSpriteSize(ps.sprite, nullptr, ZOOM_LVL_NORMAL).width);
		if (d.width > MAX_BADGE_WIDTH) break;
	}

	d.width = std::min(d.width, MAX_BADGE_WIDTH);
	return d;
}

/** Utility class to create a list of badge classes used by a feature. */
class UsedBadgeClasses {
public:
	/**
	 * Create a list of used badge classes for a feature.
	 * @param feature GRF feature being used.
	 */
	explicit UsedBadgeClasses(GrfSpecFeature feature)
	{
		for (auto index : _badges.classes) {
			Badge *class_badge = GetBadge(index);
			if (!class_badge->features.Test(feature)) continue;

			this->classes.push_back(class_badge->class_index);
		}

		std::ranges::sort(this->classes, [](const BadgeClassID &a, const BadgeClassID &b)
		{
			return GetClassBadge(a)->label < GetClassBadge(b)->label;
		});
	}

	std::span<const BadgeClassID> Classes() const { return this->classes; }

private:
	std::vector<BadgeClassID> classes; ///< List of badge classes.
};

static bool operator<(const GUIBadgeClasses::Element &a, const GUIBadgeClasses::Element &b)
{
	if (a.column_group != b.column_group) return a.column_group < b.column_group;
	if (a.sort_order != b.sort_order) return a.sort_order < b.sort_order;
	return a.label < b.label;
}

/**
 * Construct of list of badge classes and column groups to display.
 * @param feature feature being used.
 */
GUIBadgeClasses::GUIBadgeClasses(GrfSpecFeature feature)
{
	/* Get list of classes used by feature. */
	UsedBadgeClasses used(feature);

	uint max_column = 0;
	for (BadgeClassID class_index : used.Classes()) {
		Dimension size = GetBadgeMaximalDimension(class_index, feature);
		if (size.width == 0) continue;

		uint8_t column = 0;
		bool visible = true;
		uint sort_order = UINT_MAX;

		std::string_view label = GetClassBadge(class_index)->label;

		this->gui_classes.emplace_back(class_index, column, visible, sort_order, size, label);
		if (visible) max_column = std::max<uint>(max_column, column);
	}

	std::sort(std::begin(this->gui_classes), std::end(this->gui_classes));

	/* Determine total width of visible badge columns. */
	this->column_widths.resize(max_column + 1);
	for (const auto &el : this->gui_classes) {
		if (!el.visible) continue;
		this->column_widths[el.column_group] += ScaleGUITrad(el.size.width) + WidgetDimensions::scaled.hsep_normal;
	}

	/* Replace trailing `hsep_normal` spacer with wider `hsep_wide` spacer. */
	for (uint &badge_width : this->column_widths) {
		if (badge_width == 0) continue;
		badge_width = badge_width - WidgetDimensions::scaled.hsep_normal + WidgetDimensions::scaled.hsep_wide;
	}
}

/**
 * Get total width of all columns.
 * @returns sum of all column widths.
 */
uint GUIBadgeClasses::GetTotalColumnsWidth() const
{
	return std::accumulate(std::begin(this->column_widths), std::end(this->column_widths), 0U);
}

/**
 * Construct a badge text filter.
 * @param filter string filter.
 * @param feature feature being used.
 */
BadgeTextFilter::BadgeTextFilter(StringFilter &filter, GrfSpecFeature feature)
{
	/* Do not filter if the filter text box is empty */
	if (filter.IsEmpty()) return;

	/* Pre-build list of badges that match by string. */
	for (const auto &badge : _badges.specs) {
		if (badge.name == STR_NULL) continue;
		if (!badge.features.Test(feature)) continue;

		filter.ResetState();
		filter.AddLine(GetString(badge.name));
		if (!filter.GetState()) continue;

		auto it = std::ranges::lower_bound(this->badges, badge.index);
		if (it != std::end(this->badges) && *it == badge.index) continue;

		this->badges.insert(it, badge.index);
	}
}

/**
 * Test if any of the given badges matches the filtered badge list.
 * @param badges List of badges.
 * @returns true iff at least one badge in badges is present.
 */
bool BadgeTextFilter::Filter(std::span<const BadgeID> badges) const
{
	return std::ranges::any_of(badges, [this](const BadgeID &badge) { return std::ranges::binary_search(this->badges, badge); });
}

/**
 * Draw names for a list of badge labels.
 * @param r Rect to draw in.
 * @param badges List of badges.
 * @param feature GRF feature being used.
 * @returns Vertical position after drawing is complete.
 */
int DrawBadgeNameList(Rect r, std::span<const BadgeID> badges, GrfSpecFeature)
{
	if (badges.empty()) return r.top;

	std::set<BadgeClassID> classes;
	for (const BadgeID &index : badges) classes.insert(GetBadge(index)->class_index);

	std::string_view list_separator = GetListSeparator();
	for (const BadgeClassID &class_index : classes) {
		const Badge *class_badge = GetClassBadge(class_index);
		if (class_badge == nullptr || class_badge->name == STR_NULL) continue;

		std::string s;
		for (const BadgeID &index : badges) {
			const Badge *badge = GetBadge(index);
			if (badge == nullptr || badge->name == STR_NULL) continue;
			if (badge->class_index != class_index) continue;

			if (!s.empty()) {
				if (badge->flags.Test(BadgeFlag::NameListFirstOnly)) continue;
				s += list_separator;
			}
			AppendStringInPlace(s, badge->name);
			if (badge->flags.Test(BadgeFlag::NameListStop)) break;
		}

		if (s.empty()) continue;

		r.top = DrawStringMultiLine(r, GetString(STR_BADGE_NAME_LIST, class_badge->name, std::move(s)), TC_BLACK);
	}

	return r.top;
}

/**
 * Draw a badge column group.
 * @param r rect to draw within.
 * @param column_group column to draw.
 * @param badge_classes badge classes.
 * @param badges badges to draw.
 * @param feature feature being used.
 * @param introduction_date introduction date of item.
 * @param remap palette remap to for company-coloured badges.
 */
void DrawBadgeColumn(Rect r, int column_group, const GUIBadgeClasses &badge_classes, std::span<const BadgeID> badges, GrfSpecFeature feature, std::optional<TimerGameCalendar::Date> introduction_date, PaletteID remap)
{
	bool rtl = _current_text_dir == TD_RTL;
	for (const auto &badge_class : badge_classes.GetClasses()) {
		if (badge_class.column_group != column_group) continue;
		if (!badge_class.visible) continue;

		int width = ScaleGUITrad(badge_class.size.width);
		for (const BadgeID &index : badges) {
			const Badge &badge = *GetBadge(index);
			if (badge.class_index != badge_class.badge_class) continue;

			PalSpriteID ps = GetBadgeSprite(badge, feature, introduction_date, remap);
			if (ps.sprite == 0) continue;

			DrawSpriteIgnorePadding(ps.sprite, ps.pal, r.WithWidth(width, rtl), SA_CENTER);
			break;
		}

		r = r.Indent(width + WidgetDimensions::scaled.hsep_normal, rtl);
	}
}

/** Drop down element that draws a list of badges. */
template <class TBase, bool TEnd = true, FontSize TFs = FS_NORMAL>
class DropDownBadges : public TBase {
public:
	template <typename... Args>
	explicit DropDownBadges(const std::shared_ptr<GUIBadgeClasses> &badge_classes, std::span<const BadgeID> badges, GrfSpecFeature feature, std::optional<TimerGameCalendar::Date> introduction_date, Args&&... args)
		: TBase(std::forward<Args>(args)...), badge_classes(badge_classes), badges(badges), feature(feature), introduction_date(introduction_date)
	{
		for (const auto &badge_class : badge_classes->GetClasses()) {
			if (badge_class.column_group != 0) continue;
			dim.width += badge_class.size.width + WidgetDimensions::scaled.hsep_normal;
			dim.height = std::max(dim.height, badge_class.size.height);
		}
	}

	uint Height() const override { return std::max<uint>(this->dim.height, this->TBase::Height()); }
	uint Width() const override { return this->dim.width + WidgetDimensions::scaled.hsep_wide + this->TBase::Width(); }

	void Draw(const Rect &full, const Rect &r, bool sel, Colours bg_colour) const override
	{
		bool rtl = TEnd ^ (_current_text_dir == TD_RTL);

		DrawBadgeColumn(r.WithWidth(this->dim.width, rtl), 0, *this->badge_classes, this->badges, this->feature, this->introduction_date, PAL_NONE);

		this->TBase::Draw(full, r.Indent(this->dim.width + WidgetDimensions::scaled.hsep_wide, rtl), sel, bg_colour);
	}

private:
	std::shared_ptr<GUIBadgeClasses> badge_classes;

	const std::span<const BadgeID> badges;
	const GrfSpecFeature feature;
	const std::optional<TimerGameCalendar::Date> introduction_date;

	Dimension dim{};

};

using DropDownListBadgeItem = DropDownBadges<DropDownListStringItem>;
using DropDownListBadgeIconItem = DropDownBadges<DropDownListIconItem>;

std::unique_ptr<DropDownListItem> MakeDropDownListBadgeItem(const std::shared_ptr<GUIBadgeClasses> &badge_classes, std::span<const BadgeID> badges, GrfSpecFeature feature, std::optional<TimerGameCalendar::Date> introduction_date, std::string &&str, int value, bool masked, bool shaded)
{
	return std::make_unique<DropDownListBadgeItem>(badge_classes, badges, feature, introduction_date, std::move(str), value, masked, shaded);
}

std::unique_ptr<DropDownListItem> MakeDropDownListBadgeIconItem(const std::shared_ptr<GUIBadgeClasses> &badge_classes, std::span<const BadgeID> badges, GrfSpecFeature feature, std::optional<TimerGameCalendar::Date> introduction_date, const Dimension &dim, SpriteID sprite, PaletteID palette, std::string &&str, int value, bool masked, bool shaded)
{
	return std::make_unique<DropDownListBadgeIconItem>(badge_classes, badges, feature, introduction_date, dim, sprite, palette, std::move(str), value, masked, shaded);
}
