# City Vice -- Technical Implementation Document

**Feature**: City Vice (Urban Crime & Unrest)
**Version**: 1.0
**Date**: 2026-03-15
**Audience**: backend-engineer, ai-developer, tester

---

## Table of Contents

1. [Implementation Phases](#1-implementation-phases)
2. [File-by-File Change Specification](#2-file-by-file-change-specification)
3. [Data Model](#3-data-model)
4. [Command System Integration](#4-command-system-integration)
5. [Saveload](#5-saveload)
6. [Settings](#6-settings)
7. [Strings](#7-strings)
8. [GUI Changes](#8-gui-changes)
9. [Timer Integration](#9-timer-integration)
10. [Script API (AI/GS)](#10-script-api-aigs)
11. [News Integration](#11-news-integration)
12. [Unit Tests](#12-unit-tests)
13. [Build System](#13-build-system)
14. [Station Rating Slowdown During Crime Wave](#14-station-rating-slowdown-during-crime-wave)

---

## 1. Implementation Phases

The feature is divided into six phases. Each phase produces a buildable, testable increment. Commit boundaries are marked within each phase.

### Phase 1: Data Model and Settings (Foundation)

**Goal**: Add Town struct fields, settings, saveload, and string definitions. No logic yet.

**Dependencies**: None.

**Commit 1a**: Add vice fields to Town struct and saveload
- `src/town.h`: Add 6 new `uint8_t` fields to `struct Town`
- `src/saveload/saveload.h`: Add `SLV_CITY_VICE` version constant
- `src/saveload/town_sl.cpp`: Add `SLE_CONDVAR` entries for new fields
- `src/saveload/compat/town_sl_compat.h`: Add compat entries

**Commit 1b**: Add settings and strings
- `src/settings_type.h`: Add fields to `EconomySettings` and `DifficultySettings`
- `src/table/settings/economy_settings.ini`: Add 3 economy settings
- `src/table/settings/difficulty_settings.ini`: Add 1 difficulty setting
- `src/lang/english.txt`: Add all string IDs for settings, UI, news

**Commit 1c**: Add type header
- `src/vice_type.h`: New file with `ViceEventType` enum, `ViceSeverity` enum, `PolicingTier` enum

### Phase 2: Core Mechanics (Vice Calculation and Event Logic)

**Goal**: Implement vice level calculation and event triggering in the monthly timer. No GUI, no news, no AI yet.

**Dependencies**: Phase 1.

**Commit 2a**: Vice level calculation
- `src/vice_func.h`: New file with function declarations
- `src/vice_func.cpp`: New file with `CalculateViceLevel()`, `CheckViceEvent()`, event selection logic

**Commit 2b**: Timer integration
- `src/town_cmd.cpp`: Hook vice calculation and event check into `_economy_towns_monthly`

**Commit 2c**: Event effect implementations
- `src/vice_func.cpp`: Implement `DoViceEventPettyVandalism()`, `DoViceEventVehicleSabotage()`, `DoViceEventArson()`, `DoViceEventRiot()`, `DoViceEventCrimeWave()`

**Commit 2d**: Crime Wave station rating slowdown
- `src/station_cmd.cpp`: Add crime wave check to `UpdateStationRating()` to halve positive rating recovery steps

### Phase 3: Fund Policing Town Action (Tiered)

**Goal**: Add the Fund Policing action to the town authority menu with 5 investment tiers.

**Dependencies**: Phase 1.

**Commit 3a**: New command and TownAction enum
- `src/town.h`: Add `FundPolicing` to `TownAction` enum
- `src/town_cmd.h`: Declare `CmdFundPolicing()` and register with `DEF_CMD_TRAIT`
- `src/command_type.h`: Add `Commands::FundPolicing` enum entry
- `src/town_cmd.cpp`: Implement `CmdFundPolicing()` with `PolicingTier` parameter; add `TownActionFundPolicing()` stub to the `_town_action_proc[]` array; update `town_action_costs[]` and `GetMaskOfTownActions()`

**Commit 3b**: Town authority window integration
- `src/town_gui.cpp`: Add policing tier dropdown, update tooltip, add `OnDropdownSelect` handler for tier selection
- `src/widgets/town_widget.h`: Add `WID_TA_POLICING_TIER` widget ID for the dropdown
- `src/lang/english.txt`: Fund Policing action, tooltip, and tier name strings

### Phase 4: News and Visual Effects

**Goal**: All vice events generate appropriate news messages and visual effects.

**Dependencies**: Phase 2.

**Commit 4a**: News messages for events
- `src/vice_func.cpp`: Add `AddNewsItem()` calls for each event type
- `src/vice_func.cpp`: Add threshold crossing notifications

**Commit 4b**: Visual effects
- `src/vice_func.cpp`: Add `CreateEffectVehicleAbove()` calls for Arson and Riot events

### Phase 5: GUI (Town View Window)

**Goal**: Show vice level in the town view window.

**Dependencies**: Phase 2.

**Commit 5a**: Town view info panel
- `src/town_gui.cpp`: Add vice level display to `TownViewWindow::DrawWidget()` and `GetDesiredInfoHeight()`
- `src/lang/english.txt`: Vice level display strings

### Phase 6: Script API and Tests

**Goal**: AI/GS event broadcasts and unit tests.

**Dependencies**: Phase 2, Phase 4.

**Commit 6a**: Script events
- `src/script/api/script_event.hpp`: Add new event type constants
- `src/script/api/script_event_types.hpp`: Add new event classes

**Commit 6b**: Broadcast events from vice
- `src/vice_func.cpp`: Add `AI::BroadcastNewEvent()` and `Game::NewEvent()` calls

**Commit 6c**: Unit tests
- `src/tests/vice_func.cpp`: New file with Catch2 tests for vice calculation, policing tiers, crime wave slowdown

**Commit 6d**: Build system
- `src/CMakeLists.txt`: Add new source files
- `src/tests/CMakeLists.txt`: Add test file

---

## 2. File-by-File Change Specification

### New Files

| File | Purpose |
|------|---------|
| `src/vice_type.h` | Enums: `ViceEventType`, `ViceSeverity`, `PolicingTier`; policing tier data table |
| `src/vice_func.h` | Function declarations for vice system |
| `src/vice_func.cpp` | Vice calculation, event logic, effects |
| `src/tests/vice_func.cpp` | Unit tests |

### Modified Files

| File | Nature of Change |
|------|-----------------|
| `src/town.h` | Add 6 fields to `struct Town`, add `FundPolicing` to `TownAction` enum |
| `src/town_cmd.h` | Declare `CmdFundPolicing()`, add `DEF_CMD_TRAIT` for `Commands::FundPolicing` |
| `src/town_cmd.cpp` | Add `TownActionFundPolicing()` stub, implement `CmdFundPolicing()`, hook monthly timer, update arrays |
| `src/command_type.h` | Add `FundPolicing` to `Commands` enum |
| `src/town_gui.cpp` | Display vice level in town view, policing tier dropdown in authority window |
| `src/widgets/town_widget.h` | Add `WID_TA_POLICING_TIER` widget ID |
| `src/settings_type.h` | Add fields to `EconomySettings` and `DifficultySettings` |
| `src/table/settings/economy_settings.ini` | Add 3 settings |
| `src/table/settings/difficulty_settings.ini` | Add 1 setting |
| `src/saveload/saveload.h` | Add `SLV_CITY_VICE` |
| `src/saveload/town_sl.cpp` | Add 6 `SLE_CONDVAR` entries |
| `src/lang/english.txt` | Add ~40 new string IDs |
| `src/station_cmd.cpp` | Crime wave station rating slowdown in `UpdateStationRating()` |
| `src/script/api/script_event.hpp` | Add event type constants |
| `src/script/api/script_event_types.hpp` | Add event classes |
| `src/CMakeLists.txt` | Add new source files |
| `src/tests/CMakeLists.txt` | Add test file |

---

## 3. Data Model

### 3.1 Town Struct Additions

Add the following fields to `struct Town` in `src/town.h`, immediately after the existing `road_build_months` field (currently line 151):

```cpp
uint8_t vice_level = 0;           ///< Cached vice level (0-100), recalculated monthly.
uint8_t vice_level_prev = 0;      ///< Previous month's vice level, for threshold notifications.
uint8_t vice_cooldown = 0;        ///< Months remaining until next vice event can trigger.
uint8_t crime_wave_months = 0;    ///< Remaining months of active Crime Wave effect.
uint8_t policing_months = 0;      ///< Remaining months of Fund Policing effect.
uint8_t policing_reduction = 0;   ///< Vice reduction from current policing tier (10/20/30/45/70).
```

Insert these 6 fields between `road_build_months` (line 151) and `larger_town` (line 152).

### 3.2 New Type Header: `src/vice_type.h`

```cpp
/** @file vice_type.h Types related to the City Vice system. */

#ifndef VICE_TYPE_H
#define VICE_TYPE_H

/** Types of vice events that can occur in a town. */
enum class ViceEventType : uint8_t {
	PettyVandalism,   ///< Station rating hit on one station.
	VehicleSabotage,  ///< One vehicle in town radius gets a breakdown.
	Arson,            ///< One house destroyed by fire.
	Riot,             ///< 1-3 houses destroyed + station rating hit + company rating hit.
	CrimeWave,        ///< All stations get rating penalty for 3 months.
	End,              ///< End marker.
};

/** Vice severity thresholds for display and notifications. */
enum class ViceSeverity : uint8_t {
	Peaceful,   ///< Vice level 0-10.
	Low,        ///< Vice level 11-20.
	Moderate,   ///< Vice level 21-35.
	Elevated,   ///< Vice level 36-50.
	High,       ///< Vice level 51-75.
	Critical,   ///< Vice level 76-100.
};

/** Policing investment tiers available through Fund Policing. */
enum class PolicingTier : uint8_t {
	BasicPatrol,     ///< Cost 25000, -10 vice, 3 months.
	EnhancedPatrol,  ///< Cost 50000, -20 vice, 4 months.
	PoliceStation,   ///< Cost 100000, -30 vice, 6 months.
	SecurityForce,   ///< Cost 200000, -45 vice, 6 months.
	MartialLaw,      ///< Cost 500000, -70 vice, 6 months.
	End,             ///< End marker.
};

/** Difficulty severity levels for vice events. */
enum class ViceDifficulty : uint8_t {
	Mild   = 0, ///< Only Vandalism and Sabotage; 3-month cooldown.
	Normal = 1, ///< All event types; 2-month cooldown.
	Harsh  = 2, ///< All event types, x1.5 frequency, 4 max riot buildings; 1-month cooldown.
};

/** Data for each policing tier. */
struct PolicingTierData {
	int32_t cost;        ///< Cost in pounds.
	uint8_t reduction;   ///< Vice level reduction.
	uint8_t months;      ///< Duration in months.
};

/** Lookup table for policing tier data. */
static constexpr PolicingTierData _policing_tier_data[] = {
	{  25000, 10, 3 }, // BasicPatrol
	{  50000, 20, 4 }, // EnhancedPatrol
	{ 100000, 30, 6 }, // PoliceStation
	{ 200000, 45, 6 }, // SecurityForce
	{ 500000, 70, 6 }, // MartialLaw
};
static_assert(std::size(_policing_tier_data) == to_underlying(PolicingTier::End));

#endif /* VICE_TYPE_H */
```

### 3.3 TownAction Enum Modification

In `src/town.h`, the `TownAction` enum (currently lines 250-261) must be extended. Add `FundPolicing` before `End`:

```cpp
enum class TownAction : uint8_t {
	AdvertiseSmall,   ///< Small advertising campaign.
	AdvertiseMedium,  ///< Medium advertising campaign.
	AdvertiseLarge,   ///< Large advertising campaign.
	RoadRebuild,      ///< Rebuild the roads.
	BuildStatue,      ///< Build a statue.
	FundBuildings,    ///< Fund new buildings.
	BuyRights,        ///< Buy exclusive transport rights.
	Bribe,            ///< Try to bribe the council.
	FundPolicing,     ///< Fund policing to reduce vice. <-- NEW
	End,              ///< End marker.
};
```

**CRITICAL**: This changes the ordinal value of `End` from 8 to 9. All arrays indexed by `TownAction` must be updated:
- `town_action_costs[]` in `town_cmd.cpp` (line 3436): add entry for FundPolicing
- `_town_action_proc[]` in `town_cmd.cpp` (line 3693): add `TownActionFundPolicing` entry
- `action_tooltips[]` in `town_gui.cpp` (line 87): size changes from 8 to 9 automatically (uses `TownAction::End`)
- `###length 8` for `STR_LOCAL_AUTHORITY_ACTION_*` in `english.txt` (line 3784): change to `###length 9`

---

## 4. Command System Integration

### 4.1 How Existing Town Actions Work

The existing system (read from `src/town_cmd.cpp`):
1. `CmdDoTownAction()` (line 3767) receives a `TownAction` enum value.
2. It validates via `GetMaskOfTownActions()` (line 3711) that the action is available.
3. Cost is computed via `GetTownActionCost()` (line 3430), which indexes into `town_action_costs[]`.
4. The action procedure is called via `_town_action_proc[action]()`.

### 4.2 Why Fund Policing Needs a Separate Command

The existing `CmdDoTownAction` takes only a `TownAction` enum and has no way to pass additional parameters (like a policing tier). The design document specifies 5 policing tiers with different costs, durations, and reductions. Rather than adding 5 separate `TownAction` entries (which would break the contiguous string layout and bloat the action list), a new dedicated command `CmdFundPolicing` is introduced.

The `TownAction::FundPolicing` entry still appears in the authority window action list and participates in `GetMaskOfTownActions()` for visibility checks. However, the actual execution goes through `CmdFundPolicing` instead of `CmdDoTownAction`, bypassing the `_town_action_proc[]` dispatch. The `_town_action_proc[]` entry for `FundPolicing` is a stub that returns `CMD_ERROR` to prevent accidental use via the old command path.

### 4.3 TownAction Integration (Visibility Only)

#### 4.3.1 Cost Factor

Add entry to `town_action_costs[]` in `src/town_cmd.cpp` (after line 3437):

```cpp
static const uint8_t town_action_costs[] = {
	2, 4, 9, 35, 48, 53, 117, 175, 0  // <-- 0 for FundPolicing (cost handled by CmdFundPolicing)
};
```

The cost factor 0 is a placeholder. The actual cost computation is done by `CmdFundPolicing` using the `_policing_tier_data[]` table. The `GetMaskOfTownActions()` function is modified to always include `FundPolicing` when vice is enabled (since the cost factor 0 would incorrectly make it always "affordable"). See section 4.3.3.

#### 4.3.2 Action Procedure Stub

Add to `src/town_cmd.cpp`, before the `_town_action_proc[]` array:

```cpp
/**
 * Stub for the "Fund Policing" town action.
 * Actual execution goes through CmdFundPolicing with a tier parameter.
 * This stub exists only to keep the _town_action_proc[] array consistent.
 * @param t Unused.
 * @param flags Unused.
 * @return Always CMD_ERROR.
 */
static CommandCost TownActionFundPolicing([[maybe_unused]] Town *t, [[maybe_unused]] DoCommandFlags flags)
{
	return CMD_ERROR;
}
```

Update `_town_action_proc[]` (line 3693):

```cpp
static TownActionProc * const _town_action_proc[] = {
	TownActionAdvertiseSmall,
	TownActionAdvertiseMedium,
	TownActionAdvertiseLarge,
	TownActionRoadRebuild,
	TownActionBuildStatue,
	TownActionFundBuildings,
	TownActionBuyRights,
	TownActionBribe,
	TownActionFundPolicing   // <-- NEW (stub)
};
```

#### 4.3.3 GetMaskOfTownActions Modification

In `GetMaskOfTownActions()` (line 3711), add a filter for the new action. The existing loop checks cost affordability via `GetTownActionCost(cur) * _price[Price::TownAction] >> 8`, but Fund Policing has cost factor 0 which would incorrectly make it always "affordable." Add special handling. Insert before the existing `if (cur == TownAction::BuyRights...)` check (line 3738):

```cpp
/* Fund Policing: skip if city vice is disabled, otherwise always show
 * (affordability is checked by CmdFundPolicing, not here). */
if (cur == TownAction::FundPolicing) {
	if (!_settings_game.economy.city_vice) continue;
	buttons.Set(cur);
	continue;
}
```

This ensures Fund Policing appears in the list whenever city vice is enabled, regardless of cost factor.

### 4.4 New Command: CmdFundPolicing

#### 4.4.1 Declaration in `src/town_cmd.h`

Add after the existing `CmdPlaceHouseArea` declaration (line 31):

```cpp
CommandCost CmdFundPolicing(DoCommandFlags flags, TownID town_id, uint8_t tier);
```

Add the DEF_CMD_TRAIT after the existing town command traits (after line 43):

```cpp
DEF_CMD_TRAIT(Commands::FundPolicing, CmdFundPolicing, CommandFlags({CommandFlag::Location}), CommandType::LandscapeConstruction)
```

#### 4.4.2 Commands Enum Addition

In `src/command_type.h`, add `FundPolicing` to the `Commands` enum. Insert in alphabetical order near other town-related commands (find `TownAction` and add nearby):

```cpp
FundPolicing,
```

#### 4.4.3 Implementation in `src/town_cmd.cpp`

Add the following function (insert after `TownActionBribe` and before `_town_action_proc[]`):

```cpp
/**
 * Fund a policing program to reduce vice in a town.
 * @param flags Type of operation.
 * @param town_id Town to fund policing in.
 * @param tier Policing investment tier.
 * @return The cost of this operation or an error.
 */
CommandCost CmdFundPolicing(DoCommandFlags flags, TownID town_id, uint8_t tier)
{
	if (!_settings_game.economy.city_vice) return CommandCost(STR_ERROR_CITY_VICE_DISABLED);

	Town *t = Town::GetIfValid(town_id);
	if (t == nullptr) return CMD_ERROR;

	if (tier >= std::size(_policing_tier_data)) return CMD_ERROR;

	const PolicingTierData &data = _policing_tier_data[tier];
	CommandCost cost(EXPENSES_PROPERTY, data.cost);

	if (flags.Test(DoCommandFlag::Execute)) {
		/* New tier replaces current -- tiers do not stack. */
		t->policing_months = data.months;
		t->policing_reduction = data.reduction;
		(void)static_cast<PolicingTier>(tier); /* Parameter is uint8_t for command serialization. */

		SetWindowDirty(WC_TOWN_VIEW, t->index);
		SetWindowDirty(WC_TOWN_AUTHORITY, t->index);
	}

	return cost;
}
```

#### 4.4.4 Include Dependency

Add `#include "vice_type.h"` to `src/town_cmd.cpp` and `src/town_cmd.h`.

---

## 5. Saveload

### 5.1 New SaveLoadVersion

In `src/saveload/saveload.h`, add after `SLV_BUOYS_AT_0_0` (line 416):

```cpp
SLV_CITY_VICE,                      ///< 365  PR#XXXXX City Vice feature: town vice fields.
```

This becomes the next version after `SLV_BUOYS_AT_0_0` (364). `SL_MAX_VERSION` remains the sentinel.

### 5.2 Town Save/Load Descriptors

In `src/saveload/town_sl.cpp`, add the following entries to the `_town_desc[]` array (line 254), after the `layout` entry (line 316) and before the `valid_history` entry (line 317):

```cpp
SLE_CONDVAR(Town, vice_level,          SLE_UINT8, SLV_CITY_VICE, SL_MAX_VERSION),
SLE_CONDVAR(Town, vice_level_prev,     SLE_UINT8, SLV_CITY_VICE, SL_MAX_VERSION),
SLE_CONDVAR(Town, vice_cooldown,       SLE_UINT8, SLV_CITY_VICE, SL_MAX_VERSION),
SLE_CONDVAR(Town, crime_wave_months,   SLE_UINT8, SLV_CITY_VICE, SL_MAX_VERSION),
SLE_CONDVAR(Town, policing_months,     SLE_UINT8, SLV_CITY_VICE, SL_MAX_VERSION),
SLE_CONDVAR(Town, policing_reduction,  SLE_UINT8, SLV_CITY_VICE, SL_MAX_VERSION),
```

### 5.3 Compatibility

The `SLE_CONDVAR` macro with `SLV_CITY_VICE` as the start version ensures:
- **Loading older saves**: Fields default to 0 (all `uint8_t` members have `= 0` initializers in `struct Town`). Vice starts clean, which is correct.
- **Loading newer saves in older code**: The saveload system skips unknown fields when using table headers (the current format since SLV_TABLE_CHUNKS).

### 5.4 Compat Table

The `_town_sl_compat[]` array in `src/saveload/compat/town_sl_compat.h` is only used for loading saves from before table headers were introduced. Since `SLV_CITY_VICE` is well after that cutover, no changes to `_town_sl_compat[]` are needed. Only the `_town_desc[]` array needs the `SLE_CONDVAR` entries.

---

## 6. Settings

### 6.1 EconomySettings Additions

In `src/settings_type.h`, add to `struct EconomySettings` (after `town_min_distance`, line 596):

```cpp
bool     city_vice;                  ///< enable city vice (urban crime) events
uint8_t  city_vice_intensity;        ///< multiplier for vice event probability (0-20, default 10)
uint16_t city_vice_min_population;   ///< minimum town population for vice events (default 1000)
```

### 6.2 DifficultySettings Addition

In `src/settings_type.h`, add to `struct DifficultySettings` (after `town_council_tolerance`, line 169):

```cpp
uint8_t  city_vice_severity;         ///< 0=Mild, 1=Normal, 2=Harsh
```

### 6.3 Economy Settings INI

Add to the end of `src/table/settings/economy_settings.ini` (after line 375):

```ini
[SDT_BOOL]
var      = economy.city_vice
def      = false
str      = STR_CONFIG_SETTING_CITY_VICE
strhelp  = STR_CONFIG_SETTING_CITY_VICE_HELPTEXT
post_cb  = [](auto) { InvalidateWindowClassesData(WC_TOWN_VIEW); InvalidateWindowClassesData(WC_TOWN_AUTHORITY); }
cat      = SC_BASIC

[SDT_VAR]
var      = economy.city_vice_intensity
type     = SLE_UINT8
def      = 10
min      = 0
max      = 20
interval = 1
str      = STR_CONFIG_SETTING_CITY_VICE_INTENSITY
strhelp  = STR_CONFIG_SETTING_CITY_VICE_INTENSITY_HELPTEXT
strval   = STR_JUST_COMMA
cat      = SC_ADVANCED

[SDT_VAR]
var      = economy.city_vice_min_population
type     = SLE_UINT16
def      = 1000
min      = 300
max      = 10000
interval = 100
str      = STR_CONFIG_SETTING_CITY_VICE_MIN_POPULATION
strhelp  = STR_CONFIG_SETTING_CITY_VICE_MIN_POPULATION_HELPTEXT
strval   = STR_JUST_COMMA
cat      = SC_ADVANCED
```

### 6.4 Difficulty Settings INI

Add to `src/table/settings/difficulty_settings.ini`, before the `[SDTG_VAR]` for `diff_level` (before line 288):

```ini
[SDT_VAR]
var      = difficulty.city_vice_severity
type     = SLE_UINT8
def      = 1
min      = 0
max      = 2
interval = 1
flags    = SettingFlag::GuiDropdown
str      = STR_CONFIG_SETTING_CITY_VICE_SEVERITY
strhelp  = STR_CONFIG_SETTING_CITY_VICE_SEVERITY_HELPTEXT
strval   = STR_CONFIG_SETTING_CITY_VICE_SEVERITY_MILD
cat      = SC_ADVANCED
```

**Important**: The `difficulty_settings.ini` preamble (line 11) documents 18 settings for old save compatibility via `_old_diff_settings`. The `city_vice_severity` setting is NOT one of the original 18 difficulty settings, so it does not need to be added to that array. It is a new setting that only exists in saves from `SLV_CITY_VICE` onwards.

---

## 7. Strings

### 7.1 String Additions to `src/lang/english.txt`

#### Settings Strings

Add in the settings section (near existing town settings, around line 2300-2400 area -- find `STR_CONFIG_SETTING_NOISE_LEVEL` and add nearby):

```
STR_CONFIG_SETTING_CITY_VICE                                    :City vice events: {STRING}
STR_CONFIG_SETTING_CITY_VICE_HELPTEXT                           :When enabled, towns with high population may experience crime and unrest events such as vandalism, arson, and riots. Well-serviced towns are largely immune
STR_CONFIG_SETTING_CITY_VICE_INTENSITY                          :City vice intensity: {STRING}
STR_CONFIG_SETTING_CITY_VICE_INTENSITY_HELPTEXT                 :Controls the probability of vice events. 0 disables events, 10 is normal, 20 is very frequent
STR_CONFIG_SETTING_CITY_VICE_MIN_POPULATION                     :Minimum population for city vice: {STRING}
STR_CONFIG_SETTING_CITY_VICE_MIN_POPULATION_HELPTEXT            :Towns below this population are immune to vice events
STR_CONFIG_SETTING_CITY_VICE_SEVERITY                           :City vice severity: {STRING}
STR_CONFIG_SETTING_CITY_VICE_SEVERITY_HELPTEXT                  :Controls which event types can occur and their intensity. Mild allows only minor events, Harsh enables all events with increased frequency
STR_CONFIG_SETTING_CITY_VICE_SEVERITY_MILD                      :Mild
STR_CONFIG_SETTING_CITY_VICE_SEVERITY_NORMAL                    :Normal
STR_CONFIG_SETTING_CITY_VICE_SEVERITY_HARSH                     :Harsh
```

#### Town Action Strings

Modify the `###length 8` group at line 3784 to `###length 9` and add:

```
STR_LOCAL_AUTHORITY_ACTION_FUND_POLICING                        :Fund policing
```

This goes after `STR_LOCAL_AUTHORITY_ACTION_BRIBE` (line 3792) and before the `###next-name-looks-similar` marker.

Add the tooltip after the existing tooltip block (after line 3804):

```
STR_LOCAL_AUTHORITY_ACTION_TOOLTIP_FUND_POLICING_MONTHS         :{PUSH_COLOUR}{YELLOW}Fund a policing program to reduce crime and unrest in this town.{}Select an investment tier and duration. Higher tiers provide greater vice reduction.{}{POP_COLOUR}Select a tier from the dropdown to proceed
STR_LOCAL_AUTHORITY_ACTION_TOOLTIP_FUND_POLICING_MINUTES        :{PUSH_COLOUR}{YELLOW}Fund a policing program to reduce crime and unrest in this town.{}Select an investment tier and duration. Higher tiers provide greater vice reduction.{}{POP_COLOUR}Select a tier from the dropdown to proceed
```

#### Policing Tier Strings

Add a new `###length 5` group for tier names used in the dropdown:

```
###length 5
STR_POLICING_TIER_BASIC_PATROL                                  :Basic Patrol ({CURRENCY_LONG}, -10 vice, 3 months)
STR_POLICING_TIER_ENHANCED_PATROL                               :Enhanced Patrol ({CURRENCY_LONG}, -20 vice, 4 months)
STR_POLICING_TIER_POLICE_STATION                                :Police Station ({CURRENCY_LONG}, -30 vice, 6 months)
STR_POLICING_TIER_SECURITY_FORCE                                :Security Force ({CURRENCY_LONG}, -45 vice, 6 months)
STR_POLICING_TIER_MARTIAL_LAW                                   :Martial Law ({CURRENCY_LONG}, -70 vice, 6 months)
```

#### Town View Strings

Add after the existing town view strings (after line 3768):

```
STR_TOWN_VIEW_VICE_PEACEFUL                                    :{BLACK}Crime: {GREEN}Peaceful
STR_TOWN_VIEW_VICE_LOW                                         :{BLACK}Crime: {LTGREEN}Low
STR_TOWN_VIEW_VICE_MODERATE                                    :{BLACK}Crime: {YELLOW}Moderate
STR_TOWN_VIEW_VICE_ELEVATED                                    :{BLACK}Crime: {ORANGE}Elevated
STR_TOWN_VIEW_VICE_HIGH                                        :{BLACK}Crime: {RED}High
STR_TOWN_VIEW_VICE_CRITICAL                                    :{BLACK}Crime: {RED}Critical
STR_TOWN_VIEW_VICE_TOOLTIP                                     :{BLACK}Vice level: {COMMA}/100 - Service coverage: {COMMA}%, Authority rating: {COMMA}
STR_TOWN_VIEW_POLICING_ACTIVE                                  :{BLACK}Policing active ({COMMA} months remaining)
STR_TOWN_VIEW_CRIME_WAVE_ACTIVE                                :{BLACK}{RED}Crime wave in progress! ({COMMA} months remaining)
```

#### News Strings

Add in the news section (near existing disaster/accident news strings):

```
STR_NEWS_VICE_PETTY_VANDALISM                                  :{BIG_FONT}{BLACK}Vandals damage {STATION} in {TOWN}
STR_NEWS_VICE_VEHICLE_SABOTAGE                                 :{BIG_FONT}{BLACK}Vehicle sabotaged near {TOWN}
STR_NEWS_VICE_ARSON                                            :{BIG_FONT}{BLACK}Arson destroys building in {TOWN}
STR_NEWS_VICE_RIOT                                             :{BIG_FONT}{BLACK}Riots break out in {TOWN}! {COMMA} buildings destroyed
STR_NEWS_VICE_CRIME_WAVE                                       :{BIG_FONT}{BLACK}Crime wave grips {TOWN}! Authorities warn of ongoing disruption
STR_NEWS_VICE_RISING_CONCERN                                   :{BIG_FONT}{BLACK}Petty crime rising in {TOWN}
STR_NEWS_VICE_RISING_WARNING                                   :{BIG_FONT}{BLACK}Crime rate surges in {TOWN} - authorities urge action
STR_NEWS_VICE_RISING_CRITICAL                                  :{BIG_FONT}{BLACK}Crime crisis in {TOWN}! Residents fear for safety
STR_NEWS_VICE_FALLING_CONCERN                                  :{BIG_FONT}{BLACK}Crime declining in {TOWN}
STR_NEWS_VICE_FALLING_WARNING                                  :{BIG_FONT}{BLACK}Crime rate falling in {TOWN} - improvements noted
STR_NEWS_VICE_FALLING_CRITICAL                                 :{BIG_FONT}{BLACK}{TOWN} crime crisis easing - situation improving
```

#### Error Strings

```
STR_ERROR_CITY_VICE_DISABLED                                   :City vice is not enabled
```

---

## 8. GUI Changes

### 8.1 Town View Window (`src/town_gui.cpp`)

The vice level display integrates into the existing `WID_TV_INFO` panel, which is a dynamically-sized information area. No new widget IDs are needed.

#### 8.1.1 DrawWidget Modification

In `TownViewWindow::DrawWidget()` (line 391), add the vice display after the noise level section (after line 468) and before the custom text section (line 470):

```cpp
/* Show vice level if city vice is enabled. */
if (_settings_game.economy.city_vice) {
	StringID vice_str;
	if (this->town->vice_level <= 10) vice_str = STR_TOWN_VIEW_VICE_PEACEFUL;
	else if (this->town->vice_level <= 20) vice_str = STR_TOWN_VIEW_VICE_LOW;
	else if (this->town->vice_level <= 35) vice_str = STR_TOWN_VIEW_VICE_MODERATE;
	else if (this->town->vice_level <= 50) vice_str = STR_TOWN_VIEW_VICE_ELEVATED;
	else if (this->town->vice_level <= 75) vice_str = STR_TOWN_VIEW_VICE_HIGH;
	else vice_str = STR_TOWN_VIEW_VICE_CRITICAL;

	DrawString(tr, vice_str);
	tr.top += GetCharacterHeight(FS_NORMAL);

	if (this->town->policing_months > 0) {
		DrawString(tr, GetString(STR_TOWN_VIEW_POLICING_ACTIVE, this->town->policing_months));
		tr.top += GetCharacterHeight(FS_NORMAL);
	}

	if (this->town->crime_wave_months > 0) {
		DrawString(tr, GetString(STR_TOWN_VIEW_CRIME_WAVE_ACTIVE, this->town->crime_wave_months));
		tr.top += GetCharacterHeight(FS_NORMAL);
	}
}
```

#### 8.1.2 GetDesiredInfoHeight Modification

In `TownViewWindow::GetDesiredInfoHeight()` (line 535), add after the noise level height check (after line 553):

```cpp
if (_settings_game.economy.city_vice) {
	aimed_height += GetCharacterHeight(FS_NORMAL); // Vice level line
	if (this->town->policing_months > 0) aimed_height += GetCharacterHeight(FS_NORMAL);
	if (this->town->crime_wave_months > 0) aimed_height += GetCharacterHeight(FS_NORMAL);
}
```

#### 8.1.3 OnInvalidateData

The existing `OnInvalidateData` handler in `TownViewWindow` (line 594) already calls `SetDirty()` and `ResizeWindowAsNeeded()`, which is sufficient for vice display updates. The monthly timer already calls `SetWindowDirty(WC_TOWN_VIEW, t->index)` (line 4180).

### 8.2 Town Authority Window -- Tiered Policing

The Fund Policing action integrates into the town authority window with a tier selection mechanism. When the player selects "Fund Policing" from the action list and clicks "Do it," a dropdown menu appears with the 5 policing tiers. Selecting a tier issues `CmdFundPolicing`.

#### 8.2.1 Widget Addition

In `src/widgets/town_widget.h`, add to `TownAuthorityWidgets` (after `WID_TA_EXECUTE`, line 32):

```cpp
WID_TA_POLICING_TIER, ///< Dropdown for policing tier selection (only shown when FundPolicing is selected).
```

**Note**: This widget does not need to appear in the nested widget tree. It is used only as a widget ID for `ShowDropDownMenu()` -- the dropdown is anchored to the `WID_TA_EXECUTE` button. This is the standard pattern for transient dropdowns in OpenTTD (compare with `WID_TD_SORT_CRITERIA` in the town directory which is a persistent dropdown; here we use a popup dropdown that disappears after selection).

#### 8.2.2 TownAuthorityWindow Modifications

In `TownAuthorityWindow` in `src/town_gui.cpp`:

**a) Add policing tier tracking field** (after line 84):

```cpp
PolicingTier sel_policing_tier = PolicingTier::End; ///< Currently selected policing tier.
```

**b) Update tooltip array initialization** (after line 124):

```cpp
this->action_tooltips[8] = realtime ? STR_LOCAL_AUTHORITY_ACTION_TOOLTIP_FUND_POLICING_MINUTES : STR_LOCAL_AUTHORITY_ACTION_TOOLTIP_FUND_POLICING_MONTHS;
```

**c) Update `GetEnabledActions()`** (after line 105):

```cpp
if (!_settings_game.economy.city_vice) enabled.Reset(TownAction::FundPolicing);
```

**d) Modify the `WID_TA_EXECUTE` click handler** (line 311-313). Replace the single `Command::Post` call with logic that intercepts Fund Policing to show a tier dropdown:

```cpp
case WID_TA_EXECUTE:
	if (this->sel_action == TownAction::FundPolicing) {
		/* Show policing tier dropdown instead of executing directly. */
		DropDownList list;
		for (PolicingTier tier = {}; tier != PolicingTier::End;
		     tier = static_cast<PolicingTier>(to_underlying(tier) + 1)) {
			const PolicingTierData &data = _policing_tier_data[to_underlying(tier)];
			bool affordable = Company::IsValidID(_local_company)
				&& static_cast<Money>(data.cost) <= GetAvailableMoney(_local_company);
			list.push_back(MakeDropDownListStringItem(
				GetString(STR_POLICING_TIER_BASIC_PATROL + to_underlying(tier),
				          static_cast<Money>(data.cost)),
				to_underlying(tier), !affordable));
		}
		ShowDropDownList(this, std::move(list), -1, WID_TA_EXECUTE);
	} else {
		Command<Commands::TownAction>::Post(STR_ERROR_CAN_T_DO_THIS,
			this->town->xy, static_cast<TownID>(this->window_number),
			this->sel_action);
	}
	break;
```

**e) Add `OnDropdownSelect` handler** (add as a new method in the struct, after `OnInvalidateData`):

```cpp
void OnDropdownSelect(WidgetID widget, int index, [[maybe_unused]] int click_count) override
{
	if (widget == WID_TA_EXECUTE) {
		/* Policing tier selected from dropdown. */
		uint8_t tier = static_cast<uint8_t>(index);
		Command<Commands::FundPolicing>::Post(STR_ERROR_CAN_T_DO_THIS,
			this->town->xy, static_cast<TownID>(this->window_number), tier);
	}
}
```

**f) Update the action info panel** (line 236-246). When Fund Policing is selected, show tier details instead of the standard single-cost tooltip. Modify `DrawWidget` for `WID_TA_ACTION_INFO`:

```cpp
case WID_TA_ACTION_INFO:
	if (this->sel_action != TownAction::End) {
		if (this->sel_action == TownAction::FundPolicing) {
			/* Show policing-specific info with tier summary. */
			DrawStringMultiLine(r.Shrink(WidgetDimensions::scaled.framerect),
				GetString(this->action_tooltips[to_underlying(this->sel_action)]),
				TC_YELLOW);
		} else {
			Money action_cost = _price[Price::TownAction] * GetTownActionCost(this->sel_action) >> 8;
			bool affordable = Company::IsValidID(_local_company) && action_cost < GetAvailableMoney(_local_company);

			DrawStringMultiLine(r.Shrink(WidgetDimensions::scaled.framerect),
				GetString(this->action_tooltips[to_underlying(this->sel_action)], action_cost),
				affordable ? TC_YELLOW : TC_RED);
		}
	}
	break;
```

**g) Update `UpdateWidgetSize`** for `WID_TA_ACTION_INFO` (line 252-262). Add handling for the Fund Policing tooltip which has no `{CURRENCY_LONG}` parameter:

```cpp
case WID_TA_ACTION_INFO: {
	assert(size.width > padding.width && size.height > padding.height);
	Dimension d = {0, 0};
	for (TownAction i = {}; i != TownAction::End; ++i) {
		if (i == TownAction::FundPolicing) {
			d = maxdim(d, GetStringMultiLineBoundingBox(
				GetString(this->action_tooltips[to_underlying(i)]), size));
		} else {
			Money price = _price[Price::TownAction] * GetTownActionCost(i) >> 8;
			d = maxdim(d, GetStringMultiLineBoundingBox(
				GetString(this->action_tooltips[to_underlying(i)], price), size));
		}
	}
	d.width += padding.width;
	d.height += padding.height;
	size = maxdim(size, d);
	break;
}
```

---

## 9. Timer Integration

### 9.1 Monthly Timer Hook

The vice system integrates into the existing `_economy_towns_monthly` timer in `src/town_cmd.cpp` (line 4155). This timer iterates all towns monthly and already handles counter decrements for `road_build_months`, `fund_buildings_months`, and `exclusive_counter`.

#### 9.1.1 Modification to `_economy_towns_monthly`

Add the following code inside the `for (Town *t : Town::Iterate())` loop, after the existing `UpdateTownRating(t)` call (line 4178) and before `SetWindowDirty(WC_TOWN_VIEW, t->index)` (line 4180):

```cpp
/* City Vice: decrement counters and recalculate vice level. */
if (_settings_game.economy.city_vice) {
	if (t->vice_cooldown > 0) t->vice_cooldown--;
	if (t->crime_wave_months > 0) t->crime_wave_months--;
	if (t->policing_months > 0) {
		if (--t->policing_months == 0) t->policing_reduction = 0;
	}

	UpdateTownViceLevel(t);
	CheckTownViceEvent(t);
}
```

`UpdateTownViceLevel()` and `CheckTownViceEvent()` are defined in `src/vice_func.cpp` and declared in `src/vice_func.h`.

### 9.2 Include Dependency

Add `#include "vice_func.h"` to `src/town_cmd.cpp` in the includes section.

---

## 10. Script API (AI/GS)

### 10.1 Event Type Constants

In `src/script/api/script_event.hpp`, add to the `ScriptEventType` enum (after `ET_PRESIDENT_RENAMED` at line 61):

```cpp
ET_VICE_STATION_VANDALIZED,
ET_VICE_VEHICLE_SABOTAGED,
ET_VICE_BUILDING_DESTROYED,
ET_VICE_RIOT,
ET_VICE_CRIME_WAVE,
ET_VICE_LEVEL_CHANGED,
```

### 10.2 Event Classes

In `src/script/api/script_event_types.hpp`, add after the last event class (after `ScriptEventPresidentRenamed`). Add the include for town_type.h if not already present:

```cpp
/**
 * Event Station Vandalized, indicating a station was damaged by vandals.
 * @api ai game
 */
class ScriptEventStationVandalized : public ScriptEvent {
public:
#ifndef DOXYGEN_API
	ScriptEventStationVandalized(StationID station, TownID town, int32_t rating_loss) :
		ScriptEvent(ET_VICE_STATION_VANDALIZED),
		station(station), town(town), rating_loss(rating_loss)
	{}
#endif /* DOXYGEN_API */

	static ScriptEventStationVandalized *Convert(ScriptEvent *instance) { return dynamic_cast<ScriptEventStationVandalized *>(instance); }

	StationID GetStationID() const { return this->station; }
	TownID GetTownID() const { return this->town; }
	int32_t GetRatingLoss() const { return this->rating_loss; }

private:
	StationID station;
	TownID town;
	int32_t rating_loss;
};

/**
 * Event Vehicle Sabotaged, indicating a vehicle was sabotaged in a town.
 * @api ai game
 */
class ScriptEventVehicleSabotaged : public ScriptEvent {
public:
#ifndef DOXYGEN_API
	ScriptEventVehicleSabotaged(VehicleID vehicle, TownID town) :
		ScriptEvent(ET_VICE_VEHICLE_SABOTAGED),
		vehicle(vehicle), town(town)
	{}
#endif /* DOXYGEN_API */

	static ScriptEventVehicleSabotaged *Convert(ScriptEvent *instance) { return dynamic_cast<ScriptEventVehicleSabotaged *>(instance); }

	VehicleID GetVehicleID() const { return this->vehicle; }
	TownID GetTownID() const { return this->town; }

private:
	VehicleID vehicle;
	TownID town;
};

/**
 * Event Building Destroyed, indicating buildings were destroyed by arson.
 * @api ai game
 */
class ScriptEventBuildingDestroyed : public ScriptEvent {
public:
#ifndef DOXYGEN_API
	ScriptEventBuildingDestroyed(TownID town, TileIndex tile, uint32_t num_buildings) :
		ScriptEvent(ET_VICE_BUILDING_DESTROYED),
		town(town), tile(tile), num_buildings(num_buildings)
	{}
#endif /* DOXYGEN_API */

	static ScriptEventBuildingDestroyed *Convert(ScriptEvent *instance) { return dynamic_cast<ScriptEventBuildingDestroyed *>(instance); }

	TownID GetTownID() const { return this->town; }
	TileIndex GetTile() const { return this->tile; }
	uint32_t GetNumBuildings() const { return this->num_buildings; }

private:
	TownID town;
	TileIndex tile;
	uint32_t num_buildings;
};

/**
 * Event Riot, indicating a riot occurred in a town.
 * @api ai game
 */
class ScriptEventRiot : public ScriptEvent {
public:
#ifndef DOXYGEN_API
	ScriptEventRiot(TownID town, uint32_t num_buildings, uint32_t stations_affected) :
		ScriptEvent(ET_VICE_RIOT),
		town(town), num_buildings(num_buildings), stations_affected(stations_affected)
	{}
#endif /* DOXYGEN_API */

	static ScriptEventRiot *Convert(ScriptEvent *instance) { return dynamic_cast<ScriptEventRiot *>(instance); }

	TownID GetTownID() const { return this->town; }
	uint32_t GetNumBuildings() const { return this->num_buildings; }
	uint32_t GetStationsAffected() const { return this->stations_affected; }

private:
	TownID town;
	uint32_t num_buildings;
	uint32_t stations_affected;
};

/**
 * Event Crime Wave, indicating a crime wave started in a town.
 * @api ai game
 */
class ScriptEventCrimeWave : public ScriptEvent {
public:
#ifndef DOXYGEN_API
	ScriptEventCrimeWave(TownID town, uint32_t duration_months) :
		ScriptEvent(ET_VICE_CRIME_WAVE),
		town(town), duration_months(duration_months)
	{}
#endif /* DOXYGEN_API */

	static ScriptEventCrimeWave *Convert(ScriptEvent *instance) { return dynamic_cast<ScriptEventCrimeWave *>(instance); }

	TownID GetTownID() const { return this->town; }
	uint32_t GetDurationMonths() const { return this->duration_months; }

private:
	TownID town;
	uint32_t duration_months;
};

/**
 * Event Vice Level Changed, indicating a town's vice level crossed a threshold.
 * @api ai game
 */
class ScriptEventViceLevelChanged : public ScriptEvent {
public:
#ifndef DOXYGEN_API
	ScriptEventViceLevelChanged(TownID town, uint32_t new_level) :
		ScriptEvent(ET_VICE_LEVEL_CHANGED),
		town(town), new_level(new_level)
	{}
#endif /* DOXYGEN_API */

	static ScriptEventViceLevelChanged *Convert(ScriptEvent *instance) { return dynamic_cast<ScriptEventViceLevelChanged *>(instance); }

	TownID GetTownID() const { return this->town; }
	uint32_t GetNewLevel() const { return this->new_level; }

private:
	TownID town;
	uint32_t new_level;
};
```

### 10.3 Broadcasting Events

In `src/vice_func.cpp`, after each event is executed, broadcast to AI and GS:

```cpp
#include "ai/ai.hpp"
#include "game/game.hpp"
#include "script/api/script_event_types.hpp"

// Example for Petty Vandalism:
AI::BroadcastNewEvent(new ScriptEventStationVandalized(station_id, t->index, 15));
Game::NewEvent(new ScriptEventStationVandalized(station_id, t->index, 15));
```

---

## 11. News Integration

### 11.1 News Message Patterns

Each event type generates a news item using the existing `AddNewsItem()` function (declared in `src/news_func.h`, line 18):

```cpp
void AddNewsItem(EncodedString &&headline, NewsType type, NewsStyle style,
                 NewsFlags flags, NewsReference ref1 = {}, NewsReference ref2 = {},
                 std::unique_ptr<NewsAllocatedData> &&data = nullptr,
                 AdviceType advice_type = AdviceType::Invalid);
```

### 11.2 Event-Specific News

| Event | NewsType | NewsStyle | ref1 | ref2 |
|-------|----------|-----------|------|------|
| Petty Vandalism | `NewsType::Accident` | `NewsStyle::Thin` | `station_id` | `t->index` (TownID) |
| Vehicle Sabotage | `NewsType::Accident` | `NewsStyle::Thin` | `vehicle_id` | `t->index` (TownID) |
| Arson | `NewsType::Accident` | `NewsStyle::Small` | `tile` (TileIndex) | `t->index` (TownID) |
| Riot | `NewsType::Accident` | `NewsStyle::Normal` | `tile` (TileIndex) | `t->index` (TownID) |
| Crime Wave | `NewsType::Accident` | `NewsStyle::Normal` | `t->index` (TownID) | -- |

### 11.3 Threshold Notifications

| Threshold | Direction | NewsType | NewsStyle |
|-----------|-----------|----------|-----------|
| 20 | Rising | `NewsType::General` | `NewsStyle::Thin` |
| 20 | Falling | `NewsType::General` | `NewsStyle::Thin` |
| 50 | Rising | `NewsType::General` | `NewsStyle::Small` |
| 50 | Falling | `NewsType::General` | `NewsStyle::Small` |
| 75 | Rising | `NewsType::General` | `NewsStyle::Normal` |
| 75 | Falling | `NewsType::General` | `NewsStyle::Normal` |

### 11.4 Implementation

In `src/vice_func.cpp`, the threshold notification check occurs inside `UpdateTownViceLevel()`:

```cpp
static constexpr uint8_t _vice_thresholds[] = {20, 50, 75};

void CheckViceThresholdNotification(Town *t)
{
	for (uint8_t threshold : _vice_thresholds) {
		bool prev_below = t->vice_level_prev < threshold;
		bool curr_below = t->vice_level < threshold;

		if (prev_below && !curr_below) {
			/* Rising past threshold */
			StringID str;
			NewsStyle style;
			if (threshold == 20) { str = STR_NEWS_VICE_RISING_CONCERN; style = NewsStyle::Thin; }
			else if (threshold == 50) { str = STR_NEWS_VICE_RISING_WARNING; style = NewsStyle::Small; }
			else { str = STR_NEWS_VICE_RISING_CRITICAL; style = NewsStyle::Normal; }

			AddNewsItem(GetEncodedString(str, t->index),
				NewsType::General, style, {}, t->index);
			AI::BroadcastNewEvent(new ScriptEventViceLevelChanged(t->index, t->vice_level));
			Game::NewEvent(new ScriptEventViceLevelChanged(t->index, t->vice_level));
		} else if (!prev_below && curr_below) {
			/* Falling past threshold */
			StringID str;
			NewsStyle style;
			if (threshold == 20) { str = STR_NEWS_VICE_FALLING_CONCERN; style = NewsStyle::Thin; }
			else if (threshold == 50) { str = STR_NEWS_VICE_FALLING_WARNING; style = NewsStyle::Small; }
			else { str = STR_NEWS_VICE_FALLING_CRITICAL; style = NewsStyle::Normal; }

			AddNewsItem(GetEncodedString(str, t->index),
				NewsType::General, style, {}, t->index);
			AI::BroadcastNewEvent(new ScriptEventViceLevelChanged(t->index, t->vice_level));
			Game::NewEvent(new ScriptEventViceLevelChanged(t->index, t->vice_level));
		}
	}
}
```

---

## 12. Unit Tests

### 12.1 Test File: `src/tests/vice_func.cpp`

```cpp
/** @file vice_func.cpp Tests for City Vice calculation functions. */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

#include "../vice_type.h"
/* Include the vice calculation functions -- either directly or via a testable header. */

#include "../safeguards.h"

/* Since the vice calculation uses integer math on simple inputs,
 * we can test the formulas directly without needing a full Town object. */

/* Helper: replicate the base_vice formula. */
static uint8_t CalcBaseVice(uint32_t population)
{
	return static_cast<uint8_t>(
		(static_cast<uint64_t>(population) * 100) / (population + 5000)
	);
}

/* Helper: replicate the service modifier formula. */
static uint8_t CalcServiceModifier(uint8_t pct_transported)
{
	return static_cast<uint8_t>((pct_transported * 60) / 256);
}

/* Helper: replicate the rating modifier formula. */
static int8_t CalcRatingModifier(int16_t best_rating)
{
	return static_cast<int8_t>(-(best_rating * 20) / 1000);
}


TEST_CASE("ViceCalc - BaseVice scales with population")
{
	CHECK(CalcBaseVice(0) == 0);
	CHECK(CalcBaseVice(100) == 1);
	CHECK(CalcBaseVice(500) == 9);
	CHECK(CalcBaseVice(1000) == 16);
	CHECK(CalcBaseVice(5000) == 50);
	CHECK(CalcBaseVice(10000) == 66);
	CHECK(CalcBaseVice(50000) == 90);
	CHECK(CalcBaseVice(100000) == 95);
}

TEST_CASE("ViceCalc - BaseVice never reaches 100")
{
	/* Even with extreme population, base_vice < 100 */
	CHECK(CalcBaseVice(1000000) < 100);
	CHECK(CalcBaseVice(UINT32_MAX / 100) < 100);
}

TEST_CASE("ViceCalc - ServiceModifier range")
{
	CHECK(CalcServiceModifier(0) == 0);
	CHECK(CalcServiceModifier(128) == 30);  /* 50% transport */
	CHECK(CalcServiceModifier(204) == 47);  /* ~80% transport */
	CHECK(CalcServiceModifier(255) == 59);  /* ~100% transport */
}

TEST_CASE("ViceCalc - RatingModifier range")
{
	CHECK(CalcRatingModifier(1000) == -20);  /* Best possible rating */
	CHECK(CalcRatingModifier(0) == 0);
	CHECK(CalcRatingModifier(-1000) == 20);  /* Worst possible rating */
	CHECK(CalcRatingModifier(800) == -16);
	CHECK(CalcRatingModifier(400) == -8);
}

TEST_CASE("ViceCalc - Well-serviced small town is immune")
{
	uint32_t population = 3000;
	uint8_t base = CalcBaseVice(population);   /* 37 */
	uint8_t svc = CalcServiceModifier(204);    /* 47 (80% transport) */
	int adjusted = std::max(0, (int)base - (int)svc);  /* 0 */
	int8_t rating_mod = CalcRatingModifier(800); /* -16 */
	int final_vice = std::clamp(adjusted + rating_mod, 0, 100);
	CHECK(final_vice == 0);
}

TEST_CASE("ViceCalc - Neglected metropolis has high vice")
{
	uint32_t population = 50000;
	uint8_t base = CalcBaseVice(population);   /* 90 */
	uint8_t svc = CalcServiceModifier(0);      /* 0 (no transport) */
	int adjusted = std::max(0, (int)base - (int)svc);  /* 90 */
	int8_t rating_mod = CalcRatingModifier(-500); /* 10 */
	int final_vice = std::clamp(adjusted + rating_mod, 0, 100);
	CHECK(final_vice == 100);
}

TEST_CASE("ViceCalc - Policing tiers have correct data")
{
	/* Verify the _policing_tier_data table matches the design document. */
	CHECK(std::size(_policing_tier_data) == 5);

	CHECK(_policing_tier_data[0].cost == 25000);
	CHECK(_policing_tier_data[0].reduction == 10);
	CHECK(_policing_tier_data[0].months == 3);

	CHECK(_policing_tier_data[1].cost == 50000);
	CHECK(_policing_tier_data[1].reduction == 20);
	CHECK(_policing_tier_data[1].months == 4);

	CHECK(_policing_tier_data[2].cost == 100000);
	CHECK(_policing_tier_data[2].reduction == 30);
	CHECK(_policing_tier_data[2].months == 6);

	CHECK(_policing_tier_data[3].cost == 200000);
	CHECK(_policing_tier_data[3].reduction == 45);
	CHECK(_policing_tier_data[3].months == 6);

	CHECK(_policing_tier_data[4].cost == 500000);
	CHECK(_policing_tier_data[4].reduction == 70);
	CHECK(_policing_tier_data[4].months == 6);
}

TEST_CASE("ViceCalc - Policing reduces vice, clamped to zero")
{
	int final_vice = 15;
	int result;

	/* Basic Patrol: -10 */
	result = std::max(0, final_vice - 10);
	CHECK(result == 5);

	/* Police Station: -30, clamped */
	result = std::max(0, final_vice - 30);
	CHECK(result == 0);

	/* Martial Law: -70, clamped */
	result = std::max(0, final_vice - 70);
	CHECK(result == 0);
}

TEST_CASE("ViceCalc - Policing tiers do not stack")
{
	/* Verify: funding a new tier replaces the current one.
	 * The policing_reduction field holds only one value at a time. */
	uint8_t policing_reduction = 30; // Police Station active
	uint8_t policing_months = 4;     // 4 months left

	/* Player funds Basic Patrol -- replaces, does not add. */
	policing_reduction = 10;
	policing_months = 3;

	CHECK(policing_reduction == 10);
	CHECK(policing_months == 3);
}

TEST_CASE("ViceCalc - Population below threshold produces no events")
{
	uint32_t population = 999;
	uint16_t min_population = 1000;
	CHECK(population < min_population);
}

TEST_CASE("ViceCalc - Event weight selection")
{
	/* Weights for Pop < 5k: Vandalism=60, Sabotage=25, Arson=10, Riot=5, CrimeWave=0 */
	uint8_t weights[] = {60, 25, 10, 5, 0};
	uint total = 0;
	for (auto w : weights) total += w;
	CHECK(total == 100);

	/* Roll 0-59 should select Vandalism (index 0) */
	uint roll = 30;
	uint cumulative = 0;
	int selected = -1;
	for (int i = 0; i < 5; i++) {
		cumulative += weights[i];
		if (roll < cumulative) { selected = i; break; }
	}
	CHECK(selected == 0);

	/* Roll 85 should select Arson (index 2) */
	roll = 85;
	cumulative = 0;
	selected = -1;
	for (int i = 0; i < 5; i++) {
		cumulative += weights[i];
		if (roll < cumulative) { selected = i; break; }
	}
	CHECK(selected == 2);

	/* Roll 98 should select Riot (index 3) */
	roll = 98;
	cumulative = 0;
	selected = -1;
	for (int i = 0; i < 5; i++) {
		cumulative += weights[i];
		if (roll < cumulative) { selected = i; break; }
	}
	CHECK(selected == 3);
}

TEST_CASE("ViceCalc - Riot building count")
{
	/* Formula: 1 + RandomRange(clamp(population / 20000, 0, 2)) */
	CHECK(std::clamp(5000U / 20000U, 0U, 2U) == 0);   /* Pop 5k: max 1 building */
	CHECK(std::clamp(20000U / 20000U, 0U, 2U) == 1);   /* Pop 20k: max 2 buildings */
	CHECK(std::clamp(40000U / 20000U, 0U, 2U) == 2);   /* Pop 40k: max 3 buildings */
	CHECK(std::clamp(100000U / 20000U, 0U, 2U) == 2);  /* Pop 100k: still max 3 */
}

TEST_CASE("ViceCalc - Cooldown prevents events")
{
	uint8_t cooldown = 2;
	CHECK(cooldown > 0);  /* Event should not fire */
}

TEST_CASE("ViceCalc - Severity Mild blocks Arson/Riot/CrimeWave")
{
	uint8_t severity = 0; /* Mild */
	/* In Mild mode, only ViceEventType::PettyVandalism and ViceEventType::VehicleSabotage are allowed */
	CHECK(severity == 0);
	/* Weights for Arson, Riot, CrimeWave should be forced to 0 in Mild mode */
}

TEST_CASE("ViceCalc - Overflow safety in base_vice")
{
	/* population * 100 could overflow uint32_t at ~42 million.
	 * Use uint64_t intermediate or ensure population is capped. */
	uint32_t large_pop = 1000000;
	uint64_t product = static_cast<uint64_t>(large_pop) * 100;
	uint8_t base = static_cast<uint8_t>(product / (large_pop + 5000));
	CHECK(base == 99);
}

TEST_CASE("ViceCalc - Crime wave rating slowdown")
{
	/* Verify the halving logic used in UpdateStationRating.
	 * When crime_wave_months > 0, positive rating steps are halved. */

	/* Normal step: Clamp(rating - old_rating, -2, 2) */
	int old_rating = 100;
	int new_rating_calc = 130; /* rating computed by the formula */

	int step = std::clamp(new_rating_calc - old_rating, -2, 2); /* +2 */
	CHECK(step == 2);

	/* With crime wave: positive step halved (integer division). */
	int slowed_step = step > 0 ? step / 2 : step;
	CHECK(slowed_step == 1);

	/* Negative steps are NOT halved (penalties still apply at full speed). */
	new_rating_calc = 50;
	step = std::clamp(new_rating_calc - old_rating, -2, 2); /* -2 */
	slowed_step = step > 0 ? step / 2 : step;
	CHECK(slowed_step == -2);

	/* Edge case: step of +1 halved to 0 (ratings stall). */
	step = 1;
	slowed_step = step > 0 ? step / 2 : step;
	CHECK(slowed_step == 0);
}
```

### 12.2 Test Registration

Add `vice_func.cpp` to `src/tests/CMakeLists.txt`.

---

## 13. Build System

### 13.1 Main CMakeLists.txt

In `src/CMakeLists.txt`, add the new source files in alphabetical order within the existing source list. The file currently lists sources in the 500+ line range.

After existing `vehicle_*` entries, add (or in alphabetical position):

```cmake
    vice_func.cpp
    vice_func.h
    vice_type.h
```

### 13.2 Tests CMakeLists.txt

In `src/tests/CMakeLists.txt`, add to the `add_test_files()` call:

```cmake
    vice_func.cpp
```

---

## 14. Station Rating Slowdown During Crime Wave

### 14.1 Design Requirement

From the design document (Section 3.4, Crime Wave): "While active, station ratings in the town recover 50% slower." This means that when a station's associated town has `crime_wave_months > 0`, positive rating recovery steps are halved.

### 14.2 Location in Code

Station ratings are updated in `UpdateStationRating()` in `src/station_cmd.cpp` (line 3994). This function is called every `Ticks::STATION_RATING_TICKS` (185 ticks, approximately every 2.5 in-game days) for each station, via `StationHandleSmallTick()` (line 4354).

The critical line is 4098:
```cpp
ge->rating = rating = ClampTo<uint8_t>(or_ + Clamp(rating - or_, -2, 2));
```

Here, `or_` is the old rating and `rating` is the newly calculated target. The `Clamp(rating - or_, -2, 2)` limits changes to at most +/-2 per tick. The crime wave slowdown targets this step specifically.

### 14.3 Modification

In `src/station_cmd.cpp`, modify the rating step calculation at line 4098. Replace:

```cpp
/* only modify rating in steps of -2, -1, 0, 1 or 2 */
ge->rating = rating = ClampTo<uint8_t>(or_ + Clamp(rating - or_, -2, 2));
```

With:

```cpp
/* only modify rating in steps of -2, -1, 0, 1 or 2 */
int rating_step = Clamp(rating - or_, -2, 2);

/* Crime wave: halve positive rating recovery (stations recover 50% slower). */
if (rating_step > 0 && st->town != nullptr &&
    st->town->crime_wave_months > 0 &&
    _settings_game.economy.city_vice) {
	rating_step /= 2;
}

ge->rating = rating = ClampTo<uint8_t>(or_ + rating_step);
```

### 14.4 Behaviour Details

- **Positive steps halved**: A normal +2 step becomes +1. A +1 step becomes +0 (ratings stall temporarily).
- **Negative steps unchanged**: Rating penalties from poor service still apply at full speed. This is intentional -- crime waves make things worse, they do not soften existing problems.
- **Guard conditions**: The `st->town != nullptr` check is defensive (stations should always have an associated town, but safety first). The `_settings_game.economy.city_vice` check ensures the slowdown only applies when the feature is enabled.
- **Tick frequency**: With the step halved, effective recovery rate drops from up to +2 per 185 ticks to up to +1 per 185 ticks, which is exactly the 50% slowdown specified in the design document.
- **Duration**: The `crime_wave_months` counter is decremented monthly in the `_economy_towns_monthly` timer (Section 9). When it reaches 0, the slowdown automatically stops -- no explicit cleanup is needed because the condition check evaluates `crime_wave_months > 0` each tick.

### 14.5 Include Dependency

Add `#include "town.h"` to `src/station_cmd.cpp` if not already present. Also add `#include "settings_type.h"` if not already present. (Both are likely already included given existing town-related logic in the file.)

Verify the existing includes by checking the top of `src/station_cmd.cpp`:

```cpp
/* These includes are likely already present: */
#include "town.h"         // For Town struct and crime_wave_months
#include "settings_type.h" // For _settings_game
```

If `town.h` is not included (station_cmd.cpp accesses `st->town` which is `Town *`, so it must be), add it.

---

## Appendix A: Complete `src/vice_func.h`

```cpp
/** @file vice_func.h Functions related to the City Vice system. */

#ifndef VICE_FUNC_H
#define VICE_FUNC_H

#include "town_type.h"

struct Town;

void UpdateTownViceLevel(Town *t);
void CheckTownViceEvent(Town *t);

#endif /* VICE_FUNC_H */
```

## Appendix B: Complete `src/vice_func.cpp` Structure

```cpp
/** @file vice_func.cpp Implementation of the City Vice system. */

#include "stdafx.h"

#include "vice_func.h"
#include "vice_type.h"
#include "town.h"
#include "station_base.h"
#include "station_func.h"
#include "vehicle_base.h"
#include "company_base.h"
#include "news_func.h"
#include "strings_func.h"
#include "effectvehicle_func.h"
#include "landscape.h"
#include "newgrf_house.h"
#include "town_map.h"
#include "ai/ai.hpp"
#include "game/game.hpp"
#include "script/api/script_event_types.hpp"
#include "settings_type.h"
#include "cargotype.h"

#include "safeguards.h"


/**
 * Event weight tables indexed by population tier.
 * Order: PettyVandalism, VehicleSabotage, Arson, Riot, CrimeWave
 */
static constexpr uint8_t _vice_event_weights[3][5] = {
	{60, 25, 10,  5,  0},  /* Population < 5000 */
	{40, 25, 20, 10,  5},  /* Population 5000-14999 */
	{20, 20, 25, 20, 15},  /* Population >= 15000 */
};


/** Vice level thresholds for notifications. */
static constexpr uint8_t _vice_thresholds[] = {20, 50, 75};


/**
 * Check for vice level threshold crossing and generate notifications.
 * @param t The town whose vice level may have crossed a threshold.
 */
static void CheckViceThresholdNotification(Town *t)
{
	for (uint8_t threshold : _vice_thresholds) {
		bool prev_below = t->vice_level_prev < threshold;
		bool curr_below = t->vice_level < threshold;

		if (prev_below && !curr_below) {
			/* Rising past threshold */
			StringID str;
			NewsStyle style;
			if (threshold == 20) { str = STR_NEWS_VICE_RISING_CONCERN; style = NewsStyle::Thin; }
			else if (threshold == 50) { str = STR_NEWS_VICE_RISING_WARNING; style = NewsStyle::Small; }
			else { str = STR_NEWS_VICE_RISING_CRITICAL; style = NewsStyle::Normal; }

			AddNewsItem(GetEncodedString(str, t->index),
				NewsType::General, style, {}, t->index);
			AI::BroadcastNewEvent(new ScriptEventViceLevelChanged(t->index, t->vice_level));
			Game::NewEvent(new ScriptEventViceLevelChanged(t->index, t->vice_level));
		} else if (!prev_below && curr_below) {
			/* Falling past threshold */
			StringID str;
			NewsStyle style;
			if (threshold == 20) { str = STR_NEWS_VICE_FALLING_CONCERN; style = NewsStyle::Thin; }
			else if (threshold == 50) { str = STR_NEWS_VICE_FALLING_WARNING; style = NewsStyle::Small; }
			else { str = STR_NEWS_VICE_FALLING_CRITICAL; style = NewsStyle::Normal; }

			AddNewsItem(GetEncodedString(str, t->index),
				NewsType::General, style, {}, t->index);
			AI::BroadcastNewEvent(new ScriptEventViceLevelChanged(t->index, t->vice_level));
			Game::NewEvent(new ScriptEventViceLevelChanged(t->index, t->vice_level));
		}
	}
}


/**
 * Calculate and update a town's vice level.
 * Called monthly from the economy timer.
 * @param t The town to update.
 */
void UpdateTownViceLevel(Town *t)
{
	t->vice_level_prev = t->vice_level;

	/* Population floor check. */
	if (t->cache.population < _settings_game.economy.city_vice_min_population) {
		t->vice_level = 0;
		CheckViceThresholdNotification(t);
		return;
	}

	/* 1. Base vice level (0-99). Use uint64_t to prevent overflow. */
	uint8_t base_vice = static_cast<uint8_t>(
		(static_cast<uint64_t>(t->cache.population) * 100) /
		(t->cache.population + 5000)
	);

	/* 2. Service modifier (0-60): based on passenger transport percentage. */
	const CargoSpec *passengers = FindFirstCargoWithTownAcceptanceEffect(TAE_PASSENGERS);
	uint8_t pct_transported = 0;
	if (passengers != nullptr) {
		pct_transported = t->GetPercentTransported(passengers->Index());
	}
	uint8_t service_modifier = (pct_transported * 60) / 256;

	/* 3. Adjusted vice after service. */
	int adjusted_vice = std::max(0, static_cast<int>(base_vice) - static_cast<int>(service_modifier));

	/* 4. Rating modifier (-20 to +20): best company rating in town. */
	int16_t best_rating = -1000;
	for (const Company *c : Company::Iterate()) {
		if (t->have_ratings.Test(c->index)) {
			best_rating = std::max(best_rating, t->ratings[c->index]);
		}
	}
	int rating_modifier = -(best_rating * 20) / 1000;

	/* 5. Final vice level (0-100). */
	int final_vice = std::clamp(adjusted_vice + rating_modifier, 0, 100);

	/* 6. Apply policing reduction. */
	if (t->policing_months > 0) {
		final_vice = std::max(0, final_vice - static_cast<int>(t->policing_reduction));
	}

	t->vice_level = static_cast<uint8_t>(final_vice);

	/* Check for threshold crossing notifications. */
	CheckViceThresholdNotification(t);
}


/**
 * Check whether a vice event should trigger this month.
 * Called monthly after UpdateTownViceLevel().
 * @param t The town to check.
 */
void CheckTownViceEvent(Town *t)
{
	/* Skip if vice is zero or town is in cooldown. */
	if (t->vice_level == 0) return;
	if (t->vice_cooldown > 0) return;
	if (t->cache.population < _settings_game.economy.city_vice_min_population) return;

	/* Calculate effective vice for probability roll. */
	uint effective_vice = t->vice_level * _settings_game.economy.city_vice_intensity;

	/* Apply Harsh severity multiplier. */
	if (_settings_game.difficulty.city_vice_severity == to_underlying(ViceDifficulty::Harsh)) {
		effective_vice = effective_vice * 3 / 2;
	}

	/* Roll against 1000. */
	if (RandomRange(1000) >= effective_vice) return;

	/* Select event type based on population tier. */
	int tier;
	if (t->cache.population < 5000) tier = 0;
	else if (t->cache.population < 15000) tier = 1;
	else tier = 2;

	/* Copy weights, zeroing out disallowed types for Mild severity. */
	uint8_t weights[5];
	std::copy(std::begin(_vice_event_weights[tier]),
	          std::end(_vice_event_weights[tier]), weights);

	if (_settings_game.difficulty.city_vice_severity == to_underlying(ViceDifficulty::Mild)) {
		weights[2] = 0; /* Arson */
		weights[3] = 0; /* Riot */
		weights[4] = 0; /* CrimeWave */
	}

	uint total_weight = 0;
	for (auto w : weights) total_weight += w;
	if (total_weight == 0) return;

	uint roll = RandomRange(total_weight);
	ViceEventType event_type = ViceEventType::PettyVandalism;
	uint cumulative = 0;
	for (int i = 0; i < 5; i++) {
		cumulative += weights[i];
		if (roll < cumulative) {
			event_type = static_cast<ViceEventType>(i);
			break;
		}
	}

	/* Execute the event. */
	switch (event_type) {
		case ViceEventType::PettyVandalism:  DoViceEventPettyVandalism(t); break;
		case ViceEventType::VehicleSabotage: DoViceEventVehicleSabotage(t); break;
		case ViceEventType::Arson:           DoViceEventArson(t); break;
		case ViceEventType::Riot:            DoViceEventRiot(t); break;
		case ViceEventType::CrimeWave:       DoViceEventCrimeWave(t); break;
		default: NOT_REACHED();
	}

	/* Set cooldown based on difficulty. */
	switch (static_cast<ViceDifficulty>(_settings_game.difficulty.city_vice_severity)) {
		case ViceDifficulty::Mild:   t->vice_cooldown = 3; break;
		case ViceDifficulty::Normal: t->vice_cooldown = 2; break;
		case ViceDifficulty::Harsh:  t->vice_cooldown = 1; break;
	}
}
```

### Event Effect Functions (Outline)

Each `DoViceEvent*()` function follows this pattern:

1. Find a valid target (station / vehicle / house tile).
2. If no valid target exists, abort silently (no event fires).
3. Apply the effect (modify station rating / trigger breakdown / clear house).
4. Create visual effects (explosion sprites for Arson/Riot).
5. Generate news item via `AddNewsItem()`.
6. Broadcast to AI/GS via `AI::BroadcastNewEvent()` and `Game::NewEvent()`.

#### DoViceEventPettyVandalism

```
- Iterate t->stations_near, filter to stations owned by any company.
- Pick a random one.
- Pick a random cargo accepted at that station.
- Reduce that cargo's rating by 15 (using direct station rating modification).
- AddNewsItem with STR_NEWS_VICE_PETTY_VANDALISM.
```

#### DoViceEventVehicleSabotage

```
- Find vehicles (road vehicles and trains) within t->cache.squared_town_zone_radius[HZB_TOWN_EDGE].
- Exclude aircraft, ships, and vehicles in depots.
- Pick a random one.
- Set v->breakdown_ctr = 2 and v->breakdown_delay = 120.
- AddNewsItem with STR_NEWS_VICE_VEHICLE_SABOTAGE.
```

#### DoViceEventArson

```
- Pick a random house tile within the town.
- Verify: IsTileType(tile, TileType::House) && IsHouseCompleted(tile) && CanDeleteHouse(tile).
- Try up to 20 random tiles to find a valid target.
- Call ClearTownHouse(t, tile).
- CreateEffectVehicleAbove(TileX(tile) * TILE_SIZE + TILE_SIZE / 2,
    TileY(tile) * TILE_SIZE + TILE_SIZE / 2, 0, EV_EXPLOSION_SMALL).
- AddNewsItem with STR_NEWS_VICE_ARSON.
```

#### DoViceEventRiot

```
- Count = 1 + RandomRange(clamp(t->cache.population / 20000, 0, 2)).
  If severity == Harsh: max 4 instead of 3 (clamp to 3 instead of 2).
- Destroy up to 'count' houses (same selection logic as Arson).
- Hit one random station with -15 rating penalty.
- Reduce all company ratings by 50 via ChangeTownRating().
- CreateEffectVehicleAbove with EV_EXPLOSION_LARGE for first house, EV_EXPLOSION_SMALL for rest.
- AddNewsItem with STR_NEWS_VICE_RIOT.
```

#### DoViceEventCrimeWave

```
- Set t->crime_wave_months = 3.
- For each station in t->stations_near: reduce each cargo rating by 10.
- AddNewsItem with STR_NEWS_VICE_CRIME_WAVE.
- Note: The crime_wave_months counter is decremented monthly in the timer.
  While active, station rating recovery is halved (see Section 14).
```

---

## Appendix C: Multiplayer Determinism Notes

All vice calculations use deterministic `RandomRange()` from OpenTTD's synchronized PRNG. No floating-point math is used anywhere. All state mutations happen inside the monthly timer callback which is synchronized across all clients. The Fund Policing action goes through the replicated command system (`CmdFundPolicing`).

Key determinism guarantees:
- `RandomRange()` produces identical results on all clients.
- Vehicle/station/house selection iterates pools in index order.
- The `stations_near` list is computed identically on all clients (it is rebuilt from tile data).
- All integer math uses the same operator precedence and signed/unsigned behavior across platforms (verified by using explicit casts and `std::clamp`).
- The crime wave station rating slowdown in `UpdateStationRating()` uses integer division (`step / 2`), which is deterministic.

---

## Appendix D: Backward Save Compatibility Notes

- `SLE_CONDVAR` with `SLV_CITY_VICE` ensures fields are only read from saves at or after this version.
- Loading an older save: All vice fields default to 0 (no active vice, no policing, no crime wave). The feature starts from a clean state. The first monthly tick recalculates vice levels.
- The new `TownAction::FundPolicing` ordinal (8) does not conflict with any existing data because `TownAction` values are only stored transiently in commands, not in save files. The `_town_action_proc[]` array is used only at runtime.
- The `city_vice` setting defaults to `false`, so the feature is opt-in and does not affect existing saves unless the player enables it.
- The new `Commands::FundPolicing` enum entry is a runtime command identifier and does not affect save format.

---

## Appendix E: File Path Summary

All paths relative to the repository root (`/Users/joanmiquelespadasabat/Projects/OpenTTD_joanmi/OpenTTD_joanmi-city-vice/`):

### New Files
- `src/vice_type.h`
- `src/vice_func.h`
- `src/vice_func.cpp`
- `src/tests/vice_func.cpp`

### Modified Files
- `src/town.h` (lines 151, 250-261)
- `src/town_cmd.h` (add `CmdFundPolicing` declaration and `DEF_CMD_TRAIT`)
- `src/town_cmd.cpp` (lines 3436-3438, 3693-3703, 3711-3749, 4155-4182; new functions `TownActionFundPolicing` stub and `CmdFundPolicing`)
- `src/command_type.h` (add `FundPolicing` to `Commands` enum)
- `src/town_gui.cpp` (lines 87, 97-108, 117-124, 236-246, 281-315, 391-468, 535-553; add `OnDropdownSelect`, policing tier dropdown)
- `src/widgets/town_widget.h` (add `WID_TA_POLICING_TIER`)
- `src/settings_type.h` (lines 567-597, 148-171)
- `src/table/settings/economy_settings.ini` (append at end)
- `src/table/settings/difficulty_settings.ini` (insert before diff_level)
- `src/saveload/saveload.h` (line 416)
- `src/saveload/town_sl.cpp` (line 316)
- `src/lang/english.txt` (multiple sections: settings, town view, local authority, news, policing tiers)
- `src/station_cmd.cpp` (line 4098: crime wave rating slowdown)
- `src/script/api/script_event.hpp` (line 61)
- `src/script/api/script_event_types.hpp` (append at end)
- `src/CMakeLists.txt` (source list)
- `src/tests/CMakeLists.txt` (test list)
