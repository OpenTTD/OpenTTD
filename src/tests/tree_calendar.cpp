/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tree_calendar.cpp Tests for calendar-paced tree growth. */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

#include "../clear_map.h"
#include "../core/backup_type.hpp"
#include "../core/random_func.hpp"
#include "../landscape.h"
#include "../timer/timer_game_tick.h"
#include "../tree_map.h"

#include "../safeguards.h"

TEST_CASE("Tree growth follows calendar speed outside rainforests")
{
	AutoRestoreBackup calendar(_settings_game.construction.tree_growth_calendar, true);
	AutoRestoreBackup minutes(_settings_game.economy.minutes_per_calendar_year);
	AutoRestoreBackup climate(_settings_game.game_creation.landscape, LandscapeType::Tropic);
	AutoRestoreBackup placement(_settings_game.construction.extra_tree_placement, uint8_t{2}); // Grow and spread everywhere.
	AutoRestoreBackup ambient(_settings_client.sound.ambient, false);
	AutoRestoreBackup mode(_game_mode, GameMode::Normal);
	AutoRestoreBackup tick(TimerGameTick::counter);
	AutoRestoreBackup random(_random);
	Map::Allocate(64, 64);
	const TileIndex tile = TileXY(32, 32);
	MakeTree(tile, TREE_SUB_TROPICAL, 0, TreeGrowthStage::Growing1, TreeGround::Grass, 3);
	TimerGameTick::counter = 15 << 8; // A scheduled tree update for this tile.
	_random.SetSeed(42);

	SECTION("Pace of maturation")
	{
		auto count_growth = [&](uint16_t minutes_per_year) {
			_settings_game.economy.minutes_per_calendar_year = minutes_per_year;
			uint grown = 0;
			for (uint i = 0; i < 10000; ++i) {
				SetTreeGrowth(tile, TreeGrowthStage::Growing1);
				_tile_type_procs[TileType::Trees]->tile_loop_proc(tile);
				if (GetTreeGrowth(tile) == TreeGrowthStage::Growing2) ++grown;
			}
			return grown;
		};

		CHECK(count_growth(CalendarTime::DEF_MINUTES_PER_YEAR) == 10000);
		CHECK(count_growth(CalendarTime::FROZEN_MINUTES_PER_YEAR) == 0);
		uint grown = count_growth(120);
		CHECK(grown > 850);
		CHECK(grown < 1150);
		SetTropicZone(tile, TropicZone::Rainforest);
		CHECK(count_growth(CalendarTime::FROZEN_MINUTES_PER_YEAR) == 10000);
	}

	SECTION("Spread across a rainforest boundary follows the destination")
	{
		_settings_game.economy.minutes_per_calendar_year = CalendarTime::FROZEN_MINUTES_PER_YEAR;
		for (auto dir : EnumRange(Direction::End)) {
			TileIndex neighbour = tile + TileOffsByDir(dir);
			MakeClear(neighbour, ClearGround::Grass, 3);
			if (TileX(neighbour) > TileX(tile)) SetTropicZone(neighbour, TropicZone::Rainforest);
		}
		for (auto zone : {TropicZone::Normal, TropicZone::Rainforest}) {
			SetTropicZone(tile, zone);
			for (uint i = 0; i < 1000; ++i) {
				SetTreeGrowth(tile, TreeGrowthStage::Grown);
				_tile_type_procs[TileType::Trees]->tile_loop_proc(tile);
			}
			for (auto dir : EnumRange(Direction::End)) {
				TileIndex neighbour = tile + TileOffsByDir(dir);
				CHECK(IsTileType(neighbour, TileType::Trees) == (GetTropicZone(neighbour) == TropicZone::Rainforest));
				MakeClear(neighbour, ClearGround::Grass, 3);
			}
		}
	}
}
