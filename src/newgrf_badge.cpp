 /*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_badge.cpp Functionality for NewGRF badges. */

#include "stdafx.h"
#include "house.h"
#include "industry_map.h"
#include "newgrf.h"
#include "newgrf_airporttiles.h"
#include "newgrf_badge.h"
#include "newgrf_badge_type.h"
#include "newgrf_object.h"
#include "newgrf_roadstop.h"
#include "newgrf_station.h"
#include "newgrf_spritegroup.h"
#include "rail.h"
#include "rail_map.h"
#include "station_map.h"
#include "stringfilter_type.h"
#include "strings_func.h"
#include "tile_map.h"
#include "timer/timer_game_calendar.h"
#include "town_map.h"
#include "tunnelbridge_map.h"

#include "table/strings.h"

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
 * Get a read-only view of badges.
 * @return Span of badges.
 */
std::span<const Badge> GetBadges()
{
	return _badges.specs;
}

/**
 * Get a read-only view of class badge index.
 * @return Span of badges.
 */
std::span<const BadgeID> GetClassBadges()
{
	return _badges.classes;
}

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
	this->root_spritegroup = this->self_scope.badge.grf_prop.GetFirstSpriteGroupOf({feature, GSF_DEFAULT});
}

/**
 * Test if a list of badges contains a badge.
 * @param badges List of badges.
 * @param badge Badge to find.
 * @returns true iff the badge appears in the list.
 */
static bool BadgesContains(std::span<const BadgeID> badges, BadgeID badge)
{
	return std::ranges::find(badges, badge) != std::end(badges);
}

/**
 * Test if a rail type has a badge.
 * @param rt Rail type to test.
 * @param badge Badge to find.
 * @returns true iff the rail type has the badge.
 */
static bool RailTypeHasBadge(RailType rt, BadgeID badge)
{
	return rt != INVALID_RAILTYPE && BadgesContains(GetRailTypeInfo(rt)->badges, badge);
}

/**
 * Test if a road type has a badge.
 * @param rt Road type to test.
 * @param badge Badge to find.
 * @returns true iff the road type has the badge.
 */
static bool RoadTypeHasBadge(RoadType rt, BadgeID badge)
{
	return rt != INVALID_ROADTYPE && BadgesContains(GetRoadTypeInfo(rt)->badges, badge);
}

using TileHasBadgeProc = bool(*)(TileIndex tile, BadgeID badge, GrfSpecFeatures features);

static bool TileHasBadge_Rail(TileIndex tile, BadgeID badge, GrfSpecFeatures features)
{
	if (features.Test(GrfSpecFeature::GSF_RAILTYPES) && RailTypeHasBadge(GetRailType(tile), badge)) return true;
	return false;
}

static bool TileHasBadge_Road(TileIndex tile, BadgeID badge, GrfSpecFeatures features)
{
	if (features.Test(GrfSpecFeature::GSF_ROADTYPES) && RoadTypeHasBadge(GetRoadTypeRoad(tile), badge)) return true;
	if (features.Test(GrfSpecFeature::GSF_TRAMTYPES) && RoadTypeHasBadge(GetRoadTypeTram(tile), badge)) return true;
	if (features.Test(GrfSpecFeature::GSF_RAILTYPES) && IsLevelCrossing(tile) && RailTypeHasBadge(GetRailType(tile), badge)) return true;
	return false;
}

static bool TileHasBadge_Town(TileIndex tile, BadgeID badge, GrfSpecFeatures features)
{
	if (features.Test(GrfSpecFeature::GSF_HOUSES) && BadgesContains(HouseSpec::Get(GetHouseType(tile))->badges, badge)) return true;
	return false;
}

static bool TileHasBadge_Station(TileIndex tile, BadgeID badge, GrfSpecFeatures features)
{
	switch (GetStationType(tile)) {
		case StationType::Rail:
		case StationType::RailWaypoint:
			if (features.Test(GrfSpecFeature::GSF_STATIONS)) {
				if (const StationSpec *spec = GetStationSpec(tile); spec != nullptr && BadgesContains(spec->badges, badge)) return true;
			}
			if (features.Test(GrfSpecFeature::GSF_RAILTYPES) && RailTypeHasBadge(GetRailType(tile), badge)) return true;
			return false;

		case StationType::Bus:
		case StationType::Truck:
		case StationType::RoadWaypoint:
			if (features.Test(GrfSpecFeature::GSF_ROADSTOPS)) {
				if (const RoadStopSpec *spec = GetRoadStopSpec(tile); spec != nullptr && BadgesContains(spec->badges, badge)) return true;
			}
			if (features.Test(GrfSpecFeature::GSF_ROADTYPES) && RoadTypeHasBadge(GetRoadTypeRoad(tile), badge)) return true;
			if (features.Test(GrfSpecFeature::GSF_TRAMTYPES) && RoadTypeHasBadge(GetRoadTypeTram(tile), badge)) return true;
			return false;

		case StationType::Airport:
			if (features.Test(GrfSpecFeature::GSF_AIRPORTTILES) && BadgesContains(AirportTileSpec::GetByTile(tile)->badges, badge)) return true;
			return false;

		default:
			return false;
	}
}

static bool TileHasBadge_Industry(TileIndex tile, BadgeID badge, GrfSpecFeatures features)
{
	if (features.Test(GrfSpecFeature::GSF_INDUSTRYTILES) && BadgesContains(GetIndustryTileSpec(GetIndustryGfx(tile))->badges, badge)) return true;
	if (features.Test(GrfSpecFeature::GSF_INDUSTRIES) && BadgesContains(GetIndustrySpec(GetIndustryType(tile))->badges, badge)) return true;
	return false;
}

static bool TileHasBadge_TunnelBridge(TileIndex tile, BadgeID badge, GrfSpecFeatures features)
{
	switch (GetTunnelBridgeTransportType(tile)) {
		case TransportType::TRANSPORT_RAIL:
			if (features.Test(GrfSpecFeature::GSF_RAILTYPES) && RailTypeHasBadge(GetRailType(tile), badge)) return true;
			return false;

		case TransportType::TRANSPORT_ROAD:
			if (features.Test(GrfSpecFeature::GSF_ROADTYPES) && RoadTypeHasBadge(GetRoadTypeRoad(tile), badge)) return true;
			if (features.Test(GrfSpecFeature::GSF_TRAMTYPES) && RoadTypeHasBadge(GetRoadTypeTram(tile), badge)) return true;
			return false;

		default:
			return false;
	}
}

static bool TileHasBadge_Object(TileIndex tile, BadgeID badge, GrfSpecFeatures features)
{
	if (features.Test(GrfSpecFeature::GSF_OBJECTS) && BadgesContains(ObjectSpec::GetByTile(tile)->badges, badge)) return true;
	return false;
}

/**
 * Test if a tile has an item containing the specified badge.
 * @param tile Tile to query.
 * @param badge Badge to search for.
 * @param features GRF features to include in the test.
 * @returns true iff the badge 'is on' the tile.
 */
static bool TileHasBadge(TileIndex tile, BadgeID badge, GrfSpecFeatures features)
{
	/* Per-tiletype functions for badge testing. Like _tile_type_procs, this is 16 entries as GetTileType() reads 4 bits. */
	static constexpr std::array<TileHasBadgeProc, 16> tile_procs = {
		nullptr,
		TileHasBadge_Rail,
		TileHasBadge_Road,
		TileHasBadge_Town,
		nullptr,
		TileHasBadge_Station,
		nullptr,
		nullptr,
		TileHasBadge_Industry,
		TileHasBadge_TunnelBridge,
		TileHasBadge_Object,
	};

	TileHasBadgeProc proc = tile_procs[GetTileType(tile)];
	return proc != nullptr && proc(tile, badge, features);
}

/**
 * Test for a matching badge 'on' a specific map tile.
 * @param grffile GRF file of the current varaction.
 * @param tile Tile to test.
 * @param parameter GRF-local badge index.
 * @param features GRF features to include in the test.
 * @returns true iff the badge is present.
 */
uint32_t GetNearbyBadgeVariableResult(const GRFFile &grffile, TileIndex tile, const ResolverObject &object)
{
	GrfSpecFeatures features = static_cast<GrfSpecFeatures>(object.GetRegister(0x101));
	if (features.None()) return 0;

	uint32_t parameter = object.GetRegister(0x100);
	if (parameter >= std::size(grffile.badge_list)) return UINT_MAX;

	/* NewGRF cannot be expected to know the bounds of the map. If the tile is invalid it doesn't have the queried badge. */
	if (!IsValidTile(tile)) return 0;

	BadgeID index = grffile.badge_list[parameter];
	return TileHasBadge(tile, index, features);
}

/**
 * Test for a matching badge in a list of badges.
 * @param grffile GRF file of the current varaction.
 * @param badges List of badges to test.
 * @param parameter GRF-local badge index.
 * @returns true iff the badge is present.
 */
uint32_t GetBadgeVariableResult(const GRFFile &grffile, std::span<const BadgeID> badges, uint32_t parameter)
{
	if (parameter >= std::size(grffile.badge_list)) return UINT_MAX;

	BadgeID index = grffile.badge_list[parameter];
	return BadgesContains(badges, index);
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
	for (const Badge &badge : GetBadges()) {
		Badge *class_badge = GetClassBadge(badge.class_index);
		assert(class_badge != nullptr);
		class_badge->features.Set(badge.features);
		if (badge.name != STR_NULL) class_badge->flags.Set(BadgeFlag::HasText);
	}
}

/**
 * Get sprite for the given badge.
 * @param badge Badge being queried.
 * @param feature GRF feature being used.
 * @param introduction_date Introduction date of the item, if it has one.
 * @param remap Palette remap to use if the flag is company-coloured.
 * @returns Custom sprite to draw, or \c 0 if not available.
 */
PalSpriteID GetBadgeSprite(const Badge &badge, GrfSpecFeature feature, std::optional<TimerGameCalendar::Date> introduction_date, PaletteID remap)
{
	BadgeResolverObject object(badge, feature, introduction_date);
	const auto *group = object.Resolve<ResultSpriteGroup>();
	if (group == nullptr || group->num_sprites == 0) return {0, PAL_NONE};

	PaletteID pal = badge.flags.Test(BadgeFlag::UseCompanyColour) ? remap : PAL_NONE;

	return {group->sprite, pal};
}

/**
 * Create a list of used badge classes for a feature.
 * @param feature GRF feature being used.
 */
UsedBadgeClasses::UsedBadgeClasses(GrfSpecFeature feature) : feature(feature)
{
	for (auto index : _badges.classes) {
		Badge *class_badge = GetBadge(index);
		if (!class_badge->features.Test(feature)) continue;

		this->classes.push_back(class_badge->class_index);
	}

	std::ranges::sort(this->classes, [](const BadgeClassID &a, const BadgeClassID &b) { return GetClassBadge(a)->label < GetClassBadge(b)->label; });
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
	for (const auto &badge : GetBadges()) {
		if (badge.name == STR_NULL) continue;
		if (!badge.features.Test(feature)) continue;

		filter.ResetState();
		filter.AddLine(GetString(badge.name));
		if (!filter.GetState()) continue;

		this->badges.insert(badge.index);
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
 * Test if the given badges matches the filtered badge list.
 * @param badges List of badges.
 * @return true iff all required badges are present in the provided list.
 */
bool BadgeDropdownFilter::Filter(std::span<const BadgeID> badges) const
{
	if (this->badges.empty()) return true;

	/* We want all filtered badges to match. */
	return std::ranges::all_of(this->badges, [&badges](const auto &badge) { return std::ranges::find(badges, badge.second) != std::end(badges); });
}
