/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file build_industry.h Tables with default industry layouts and behaviours. */

#ifndef BUILD_INDUSTRY_H
#define BUILD_INDUSTRY_H

static const IndustryTileLayout _tile_table_coal_mine_0 {
	{{1, 1}, 0},
	{{1, 2}, 2},
	{{0, 0}, 5},
	{{1, 0}, 6},
	{{2, 0}, 3},
	{{2, 2}, 3},
};

static const IndustryTileLayout _tile_table_coal_mine_1 {
	{{1, 1}, 0},
	{{1, 2}, 2},
	{{2, 0}, 0},
	{{2, 1}, 2},
	{{1, 0}, 3},
	{{0, 0}, 3},
	{{0, 1}, 4},
	{{0, 2}, 4},
	{{2, 2}, 4},
};

static const IndustryTileLayout _tile_table_coal_mine_2 {
	{{0, 0}, 0},
	{{0, 1}, 2},
	{{0, 2}, 5},
	{{1, 0}, 3},
	{{1, 1}, 3},
	{{1, 2}, 6},
};

static const IndustryTileLayout _tile_table_coal_mine_3 {
	{{0, 1}, 0},
	{{0, 2}, 2},
	{{0, 3}, 4},
	{{1, 0}, 5},
	{{1, 1}, 0},
	{{1, 2}, 2},
	{{1, 3}, 3},
	{{2, 0}, 6},
	{{2, 1}, 4},
	{{2, 2}, 3},
};

static const std::vector<IndustryTileLayout> _tile_table_coal_mine {
	_tile_table_coal_mine_0,
	_tile_table_coal_mine_1,
	_tile_table_coal_mine_2,
	_tile_table_coal_mine_3,
};

static const IndustryTileLayout _tile_table_power_station_0 {
	{{0, 0}, 7},
	{{0, 1}, 9},
	{{1, 0}, 7},
	{{1, 1}, 8},
	{{2, 0}, 7},
	{{2, 1}, 8},
	{{3, 0}, 10},
	{{3, 1}, 10},
};

static const IndustryTileLayout _tile_table_power_station_1 {
	{{0, 1}, 7},
	{{0, 2}, 7},
	{{1, 0}, 8},
	{{1, 1}, 8},
	{{1, 2}, 7},
	{{2, 0}, 9},
	{{2, 1}, 10},
	{{2, 2}, 9},
};

static const IndustryTileLayout _tile_table_power_station_2 {
	{{0, 0}, 7},
	{{0, 1}, 7},
	{{1, 0}, 9},
	{{1, 1}, 8},
	{{2, 0}, 10},
	{{2, 1}, 9},
};

static const std::vector<IndustryTileLayout> _tile_table_power_station {
	_tile_table_power_station_0,
	_tile_table_power_station_1,
	_tile_table_power_station_2,
};

static const IndustryTileLayout _tile_table_sawmill_0 {
	{{1, 0}, 14},
	{{1, 1}, 12},
	{{1, 2}, 11},
	{{2, 0}, 14},
	{{2, 1}, 13},
	{{0, 0}, 15},
	{{0, 1}, 15},
	{{0, 2}, 12},
};

static const IndustryTileLayout _tile_table_sawmill_1 {
	{{0, 0}, 15},
	{{0, 1}, 11},
	{{0, 2}, 14},
	{{1, 0}, 15},
	{{1, 1}, 13},
	{{1, 2}, 12},
	{{2, 0}, 11},
	{{2, 1}, 13},
};

static const std::vector<IndustryTileLayout> _tile_table_sawmill {
	_tile_table_sawmill_0,
	_tile_table_sawmill_1,
};

static const IndustryTileLayout _tile_table_forest_0 {
	{{0, 0}, 16},
	{{0, 1}, 16},
	{{0, 2}, 16},
	{{0, 3}, 16},
	{{1, 0}, 16},
	{{1, 1}, 16},
	{{1, 2}, 16},
	{{1, 3}, 16},
	{{2, 0}, 16},
	{{2, 1}, 16},
	{{2, 2}, 16},
	{{2, 3}, 16},
	{{3, 0}, 16},
	{{3, 1}, 16},
	{{3, 2}, 16},
	{{3, 3}, 16},
	{{1, 4}, 16},
	{{2, 4}, 16},
};

static const IndustryTileLayout _tile_table_forest_1 {
	{{0, 0}, 16},
	{{1, 0}, 16},
	{{2, 0}, 16},
	{{3, 0}, 16},
	{{4, 0}, 16},
	{{0, 1}, 16},
	{{1, 1}, 16},
	{{2, 1}, 16},
	{{3, 1}, 16},
	{{4, 1}, 16},
	{{0, 2}, 16},
	{{1, 2}, 16},
	{{2, 2}, 16},
	{{3, 2}, 16},
	{{4, 2}, 16},
	{{0, 3}, 16},
	{{1, 3}, 16},
	{{2, 3}, 16},
	{{3, 3}, 16},
	{{4, 3}, 16},
	{{1, 4}, 16},
	{{2, 4}, 16},
	{{3, 4}, 16},
};

static const std::vector<IndustryTileLayout> _tile_table_forest {
	_tile_table_forest_0,
	_tile_table_forest_1,
};

static const IndustryTileLayout _tile_table_oil_refinery_0 {
	{{0, 0}, 20},
	{{0, 1}, 21},
	{{0, 2}, 22},
	{{0, 3}, 21},
	{{1, 0}, 20},
	{{1, 1}, 19},
	{{1, 2}, 22},
	{{1, 3}, 20},
	{{2, 1}, 18},
	{{2, 2}, 18},
	{{2, 3}, 18},
	{{3, 2}, 18},
	{{3, 3}, 18},
	{{2, 0}, 23},
	{{3, 1}, 23},
};

static const IndustryTileLayout _tile_table_oil_refinery_1 {
	{{0, 0}, 18},
	{{0, 1}, 18},
	{{0, 2}, 21},
	{{0, 3}, 22},
	{{0, 4}, 20},
	{{1, 0}, 18},
	{{1, 1}, 18},
	{{1, 2}, 19},
	{{1, 3}, 20},
	{{2, 0}, 18},
	{{2, 1}, 18},
	{{2, 2}, 19},
	{{2, 3}, 22},
	{{1, 4}, 23},
	{{2, 4}, 23},
};

static const std::vector<IndustryTileLayout> _tile_table_oil_refinery {
	_tile_table_oil_refinery_0,
	_tile_table_oil_refinery_1,
};

static const IndustryTileLayout _tile_table_oil_rig_0 {
	{{0, 0}, 24},
	{{0, 1}, 24},
	{{0, 2}, 25},
	{{1, 0}, 26},
	{{1, 1}, 27},
	{{1, 2}, 28},
	{{-4, -4}, GFX_WATERTILE_SPECIALCHECK},
	{{-4, -3}, GFX_WATERTILE_SPECIALCHECK},
	{{-4, -2}, GFX_WATERTILE_SPECIALCHECK},
	{{-4, -1}, GFX_WATERTILE_SPECIALCHECK},
	{{-4, 0}, GFX_WATERTILE_SPECIALCHECK},
	{{-4, 1}, GFX_WATERTILE_SPECIALCHECK},
	{{-4, 2}, GFX_WATERTILE_SPECIALCHECK},
	{{-4, 3}, GFX_WATERTILE_SPECIALCHECK},
	{{-4, 4}, GFX_WATERTILE_SPECIALCHECK},
	{{-4, 5}, GFX_WATERTILE_SPECIALCHECK},
	{{-4, 6}, GFX_WATERTILE_SPECIALCHECK},
	{{-3, 6}, GFX_WATERTILE_SPECIALCHECK},
	{{-2, 6}, GFX_WATERTILE_SPECIALCHECK},
	{{-1, 6}, GFX_WATERTILE_SPECIALCHECK},
	{{0, 6}, GFX_WATERTILE_SPECIALCHECK},
	{{1, 6}, GFX_WATERTILE_SPECIALCHECK},
	{{2, 6}, GFX_WATERTILE_SPECIALCHECK},
	{{3, 6}, GFX_WATERTILE_SPECIALCHECK},
	{{4, 6}, GFX_WATERTILE_SPECIALCHECK},
	{{5, 6}, GFX_WATERTILE_SPECIALCHECK},
	{{5, 5}, GFX_WATERTILE_SPECIALCHECK},
	{{5, 4}, GFX_WATERTILE_SPECIALCHECK},
	{{5, 3}, GFX_WATERTILE_SPECIALCHECK},
	{{5, 2}, GFX_WATERTILE_SPECIALCHECK},
	{{5, 1}, GFX_WATERTILE_SPECIALCHECK},
	{{5, 0}, GFX_WATERTILE_SPECIALCHECK},
	{{5, -1}, GFX_WATERTILE_SPECIALCHECK},
	{{5, -2}, GFX_WATERTILE_SPECIALCHECK},
	{{5, -3}, GFX_WATERTILE_SPECIALCHECK},
	{{5, -4}, GFX_WATERTILE_SPECIALCHECK},
	{{4, -4}, GFX_WATERTILE_SPECIALCHECK},
	{{3, -4}, GFX_WATERTILE_SPECIALCHECK},
	{{2, -4}, GFX_WATERTILE_SPECIALCHECK},
	{{1, -4}, GFX_WATERTILE_SPECIALCHECK},
	{{0, -4}, GFX_WATERTILE_SPECIALCHECK},
	{{-1, -4}, GFX_WATERTILE_SPECIALCHECK},
	{{-2, -4}, GFX_WATERTILE_SPECIALCHECK},
	{{-3, -4}, GFX_WATERTILE_SPECIALCHECK},
	{{2, 0}, GFX_WATERTILE_SPECIALCHECK},
	{{2, -1}, GFX_WATERTILE_SPECIALCHECK},
	{{1, -1}, GFX_WATERTILE_SPECIALCHECK},
	{{0, -1}, GFX_WATERTILE_SPECIALCHECK},
	{{-1, -1}, GFX_WATERTILE_SPECIALCHECK},
	{{-1, 0}, GFX_WATERTILE_SPECIALCHECK},
	{{-1, 1}, GFX_WATERTILE_SPECIALCHECK},
	{{-1, 2}, GFX_WATERTILE_SPECIALCHECK},
	{{-1, 3}, GFX_WATERTILE_SPECIALCHECK},
	{{0, 3}, GFX_WATERTILE_SPECIALCHECK},
	{{1, 3}, GFX_WATERTILE_SPECIALCHECK},
	{{2, 3}, GFX_WATERTILE_SPECIALCHECK},
	{{2, 2}, GFX_WATERTILE_SPECIALCHECK},
	{{2, 1}, GFX_WATERTILE_SPECIALCHECK},
};

static const std::vector<IndustryTileLayout> _tile_table_oil_rig {
	_tile_table_oil_rig_0,
};

static const IndustryTileLayout _tile_table_factory_0 {
	{{0, 0}, 39},
	{{0, 1}, 40},
	{{1, 0}, 41},
	{{1, 1}, 42},
	{{0, 2}, 39},
	{{0, 3}, 40},
	{{1, 2}, 41},
	{{1, 3}, 42},
	{{2, 1}, 39},
	{{2, 2}, 40},
	{{3, 1}, 41},
	{{3, 2}, 42},
};

static const IndustryTileLayout _tile_table_factory_1 {
	{{0, 0}, 39},
	{{0, 1}, 40},
	{{1, 0}, 41},
	{{1, 1}, 42},
	{{2, 0}, 39},
	{{2, 1}, 40},
	{{3, 0}, 41},
	{{3, 1}, 42},
	{{1, 2}, 39},
	{{1, 3}, 40},
	{{2, 2}, 41},
	{{2, 3}, 42},
};

static const std::vector<IndustryTileLayout> _tile_table_factory {
	_tile_table_factory_0,
	_tile_table_factory_1,
};

static const IndustryTileLayout _tile_table_printing_works_0 {
	{{0, 0}, 43},
	{{0, 1}, 44},
	{{1, 0}, 45},
	{{1, 1}, 46},
	{{0, 2}, 43},
	{{0, 3}, 44},
	{{1, 2}, 45},
	{{1, 3}, 46},
	{{2, 1}, 43},
	{{2, 2}, 44},
	{{3, 1}, 45},
	{{3, 2}, 46},
};

static const IndustryTileLayout _tile_table_printing_works_1 {
	{{0, 0}, 43},
	{{0, 1}, 44},
	{{1, 0}, 45},
	{{1, 1}, 46},
	{{2, 0}, 43},
	{{2, 1}, 44},
	{{3, 0}, 45},
	{{3, 1}, 46},
	{{1, 2}, 43},
	{{1, 3}, 44},
	{{2, 2}, 45},
	{{2, 3}, 46},
};

static const std::vector<IndustryTileLayout> _tile_table_printing_works {
	_tile_table_printing_works_0,
	_tile_table_printing_works_1,
};

static const IndustryTileLayout _tile_table_steel_mill_0 {
	{{2, 1}, 52},
	{{2, 2}, 53},
	{{3, 1}, 54},
	{{3, 2}, 55},
	{{0, 0}, 56},
	{{1, 0}, 57},
	{{0, 1}, 56},
	{{1, 1}, 57},
	{{0, 2}, 56},
	{{1, 2}, 57},
	{{2, 0}, 56},
	{{3, 0}, 57},
};

static const IndustryTileLayout _tile_table_steel_mill_1 {
	{{0, 0}, 52},
	{{0, 1}, 53},
	{{1, 0}, 54},
	{{1, 1}, 55},
	{{2, 0}, 52},
	{{2, 1}, 53},
	{{3, 0}, 54},
	{{3, 1}, 55},
	{{0, 2}, 56},
	{{1, 2}, 57},
	{{2, 2}, 56},
	{{3, 2}, 57},
	{{1, 3}, 56},
	{{2, 3}, 57},
};

static const std::vector<IndustryTileLayout> _tile_table_steel_mill {
	_tile_table_steel_mill_0,
	_tile_table_steel_mill_1,
};

static const IndustryTileLayout _tile_table_farm_0 {
	{{1, 0}, 33},
	{{1, 1}, 34},
	{{1, 2}, 36},
	{{0, 0}, 37},
	{{0, 1}, 37},
	{{0, 2}, 36},
	{{2, 0}, 35},
	{{2, 1}, 38},
	{{2, 2}, 38},
};

static const IndustryTileLayout _tile_table_farm_1 {
	{{1, 1}, 33},
	{{1, 2}, 34},
	{{0, 0}, 35},
	{{0, 1}, 36},
	{{0, 2}, 36},
	{{0, 3}, 35},
	{{1, 0}, 37},
	{{1, 3}, 38},
	{{2, 0}, 37},
	{{2, 1}, 37},
	{{2, 2}, 38},
	{{2, 3}, 38},
};

static const IndustryTileLayout _tile_table_farm_2 {
	{{2, 0}, 33},
	{{2, 1}, 34},
	{{0, 0}, 36},
	{{0, 1}, 36},
	{{0, 2}, 37},
	{{0, 3}, 37},
	{{1, 0}, 35},
	{{1, 1}, 38},
	{{1, 2}, 38},
	{{1, 3}, 37},
	{{2, 2}, 37},
	{{2, 3}, 35},
};

static const std::vector<IndustryTileLayout> _tile_table_farm {
	_tile_table_farm_0,
	_tile_table_farm_1,
	_tile_table_farm_2,
};

static const IndustryTileLayout _tile_table_copper_mine_0 {
	{{0, 0}, 47},
	{{0, 1}, 49},
	{{0, 2}, 51},
	{{1, 0}, 47},
	{{1, 1}, 49},
	{{1, 2}, 50},
	{{2, 0}, 51},
	{{2, 1}, 51},
};

static const IndustryTileLayout _tile_table_copper_mine_1 {
	{{0, 0}, 50},
	{{0, 1}, 47},
	{{0, 2}, 49},
	{{1, 0}, 47},
	{{1, 1}, 49},
	{{1, 2}, 51},
	{{2, 0}, 51},
	{{2, 1}, 47},
	{{2, 2}, 49},
};

static const std::vector<IndustryTileLayout> _tile_table_copper_mine {
	_tile_table_copper_mine_0,
	_tile_table_copper_mine_1,
};

static const IndustryTileLayout _tile_table_oil_well_0 {
	{{0, 0}, 29},
	{{1, 0}, 29},
	{{2, 0}, 29},
	{{0, 1}, 29},
	{{0, 2}, 29},
};

static const IndustryTileLayout _tile_table_oil_well_1 {
	{{0, 0}, 29},
	{{1, 0}, 29},
	{{1, 1}, 29},
	{{2, 2}, 29},
	{{2, 3}, 29},
};

static const std::vector<IndustryTileLayout> _tile_table_oil_well {
	_tile_table_oil_well_0,
	_tile_table_oil_well_1,
};

static const IndustryTileLayout _tile_table_bank_0 {
	{{0, 0}, 58},
	{{1, 0}, 59},
};

static const std::vector<IndustryTileLayout> _tile_table_bank {
	_tile_table_bank_0,
};

static const IndustryTileLayout _tile_table_food_process_0 {
	{{0, 0}, 60},
	{{1, 0}, 60},
	{{2, 0}, 60},
	{{0, 1}, 60},
	{{1, 1}, 60},
	{{2, 1}, 60},
	{{0, 2}, 61},
	{{1, 2}, 61},
	{{2, 2}, 63},
	{{0, 3}, 62},
	{{1, 3}, 62},
	{{2, 3}, 63},
};

static const IndustryTileLayout _tile_table_food_process_1 {
	{{0, 0}, 61},
	{{1, 0}, 60},
	{{2, 0}, 61},
	{{3, 0}, 61},
	{{0, 1}, 62},
	{{1, 1}, 63},
	{{2, 1}, 63},
	{{3, 1}, 63},
	{{0, 2}, 60},
	{{1, 2}, 60},
	{{2, 2}, 60},
	{{3, 2}, 60},
	{{0, 3}, 62},
	{{1, 3}, 62},
};

static const std::vector<IndustryTileLayout> _tile_table_food_process {
	_tile_table_food_process_0,
	_tile_table_food_process_1,
};

static const IndustryTileLayout _tile_table_paper_mill_0 {
	{{0, 0}, 64},
	{{1, 0}, 65},
	{{2, 0}, 66},
	{{3, 0}, 67},
	{{0, 1}, 68},
	{{1, 1}, 69},
	{{2, 1}, 67},
	{{3, 1}, 67},
	{{0, 2}, 66},
	{{1, 2}, 71},
	{{2, 2}, 71},
	{{3, 2}, 70},
};

static const std::vector<IndustryTileLayout> _tile_table_paper_mill {
	_tile_table_paper_mill_0,
};

static const IndustryTileLayout _tile_table_gold_mine_0 {
	{{0, 0}, 72},
	{{0, 1}, 73},
	{{0, 2}, 74},
	{{0, 3}, 75},
	{{1, 0}, 76},
	{{1, 1}, 77},
	{{1, 2}, 78},
	{{1, 3}, 79},
	{{2, 0}, 80},
	{{2, 1}, 81},
	{{2, 2}, 82},
	{{2, 3}, 83},
	{{3, 0}, 84},
	{{3, 1}, 85},
	{{3, 2}, 86},
	{{3, 3}, 87},
};

static const std::vector<IndustryTileLayout> _tile_table_gold_mine {
	_tile_table_gold_mine_0,
};

static const IndustryTileLayout _tile_table_bank2_0 {
	{{0, 0}, 89},
	{{1, 0}, 90},
};

static const std::vector<IndustryTileLayout> _tile_table_bank2 {
	_tile_table_bank2_0,
};

static const IndustryTileLayout _tile_table_diamond_mine_0 {
	{{0, 0}, 91},
	{{0, 1}, 92},
	{{0, 2}, 93},
	{{1, 0}, 94},
	{{1, 1}, 95},
	{{1, 2}, 96},
	{{2, 0}, 97},
	{{2, 1}, 98},
	{{2, 2}, 99},
};

static const std::vector<IndustryTileLayout> _tile_table_diamond_mine {
	_tile_table_diamond_mine_0,
};

static const IndustryTileLayout _tile_table_iron_mine_0 {
	{{0, 0}, 100},
	{{0, 1}, 101},
	{{0, 2}, 102},
	{{0, 3}, 103},
	{{1, 0}, 104},
	{{1, 1}, 105},
	{{1, 2}, 106},
	{{1, 3}, 107},
	{{2, 0}, 108},
	{{2, 1}, 109},
	{{2, 2}, 110},
	{{2, 3}, 111},
	{{3, 0}, 112},
	{{3, 1}, 113},
	{{3, 2}, 114},
	{{3, 3}, 115},
};

static const std::vector<IndustryTileLayout> _tile_table_iron_mine {
	_tile_table_iron_mine_0,
};

static const IndustryTileLayout _tile_table_fruit_plantation_0 {
	{{0, 0}, 116},
	{{0, 1}, 116},
	{{0, 2}, 116},
	{{0, 3}, 116},
	{{1, 0}, 116},
	{{1, 1}, 116},
	{{1, 2}, 116},
	{{1, 3}, 116},
	{{2, 0}, 116},
	{{2, 1}, 116},
	{{2, 2}, 116},
	{{2, 3}, 116},
	{{3, 0}, 116},
	{{3, 1}, 116},
	{{3, 2}, 116},
	{{3, 3}, 116},
	{{4, 0}, 116},
	{{4, 1}, 116},
	{{4, 2}, 116},
	{{4, 3}, 116},
};

static const std::vector<IndustryTileLayout> _tile_table_fruit_plantation {
	_tile_table_fruit_plantation_0,
};

static const IndustryTileLayout _tile_table_rubber_plantation_0 {
	{{0, 0}, 117},
	{{0, 1}, 117},
	{{0, 2}, 117},
	{{0, 3}, 117},
	{{1, 0}, 117},
	{{1, 1}, 117},
	{{1, 2}, 117},
	{{1, 3}, 117},
	{{2, 0}, 117},
	{{2, 1}, 117},
	{{2, 2}, 117},
	{{2, 3}, 117},
	{{3, 0}, 117},
	{{3, 1}, 117},
	{{3, 2}, 117},
	{{3, 3}, 117},
	{{4, 0}, 117},
	{{4, 1}, 117},
	{{4, 2}, 117},
	{{4, 3}, 117},
};

static const std::vector<IndustryTileLayout> _tile_table_rubber_plantation {
	_tile_table_rubber_plantation_0,
};

static const IndustryTileLayout _tile_table_water_supply_0 {
	{{0, 0}, 118},
	{{0, 1}, 119},
	{{1, 0}, 118},
	{{1, 1}, 119},
};

static const std::vector<IndustryTileLayout> _tile_table_water_supply {
	_tile_table_water_supply_0,
};

static const IndustryTileLayout _tile_table_water_tower_0 {
	{{0, 0}, 120},
};

static const std::vector<IndustryTileLayout> _tile_table_water_tower {
	_tile_table_water_tower_0,
};

static const IndustryTileLayout _tile_table_factory2_0 {
	{{0, 0}, 121},
	{{0, 1}, 122},
	{{1, 0}, 123},
	{{1, 1}, 124},
	{{0, 2}, 121},
	{{0, 3}, 122},
	{{1, 2}, 123},
	{{1, 3}, 124},
};

static const IndustryTileLayout _tile_table_factory2_1 {
	{{0, 0}, 121},
	{{0, 1}, 122},
	{{1, 0}, 123},
	{{1, 1}, 124},
	{{2, 0}, 121},
	{{2, 1}, 122},
	{{3, 0}, 123},
	{{3, 1}, 124},
};

static const std::vector<IndustryTileLayout> _tile_table_factory2 {
	_tile_table_factory2_0,
	_tile_table_factory2_1,
};

static const IndustryTileLayout _tile_table_farm2_0 {
	{{1, 0}, 33},
	{{1, 1}, 34},
	{{1, 2}, 36},
	{{0, 0}, 37},
	{{0, 1}, 37},
	{{0, 2}, 36},
	{{2, 0}, 35},
	{{2, 1}, 38},
	{{2, 2}, 38},
};

static const IndustryTileLayout _tile_table_farm2_1 {
	{{1, 1}, 33},
	{{1, 2}, 34},
	{{0, 0}, 35},
	{{0, 1}, 36},
	{{0, 2}, 36},
	{{0, 3}, 35},
	{{1, 0}, 37},
	{{1, 3}, 38},
	{{2, 0}, 37},
	{{2, 1}, 37},
	{{2, 2}, 38},
	{{2, 3}, 38},
};

static const IndustryTileLayout _tile_table_farm2_2 {
	{{2, 0}, 33},
	{{2, 1}, 34},
	{{0, 0}, 36},
	{{0, 1}, 36},
	{{0, 2}, 37},
	{{0, 3}, 37},
	{{1, 0}, 35},
	{{1, 1}, 38},
	{{1, 2}, 38},
	{{1, 3}, 37},
	{{2, 2}, 37},
	{{2, 3}, 35},
};

static const std::vector<IndustryTileLayout> _tile_table_farm2 {
	_tile_table_farm2_0,
	_tile_table_farm2_1,
	_tile_table_farm2_2,
};

static const IndustryTileLayout _tile_table_lumber_mill_0 {
	{{0, 0}, 125},
	{{0, 1}, 126},
	{{1, 0}, 127},
	{{1, 1}, 128},
};

static const std::vector<IndustryTileLayout> _tile_table_lumber_mill {
	_tile_table_lumber_mill_0,
};

static const IndustryTileLayout _tile_table_cotton_candy_0 {
	{{0, 0}, 129},
	{{0, 1}, 129},
	{{0, 2}, 129},
	{{0, 3}, 129},
	{{1, 0}, 129},
	{{1, 1}, 129},
	{{1, 2}, 129},
	{{1, 3}, 129},
	{{2, 0}, 129},
	{{2, 1}, 129},
	{{2, 2}, 129},
	{{2, 3}, 129},
	{{3, 0}, 129},
	{{3, 1}, 129},
	{{3, 2}, 129},
	{{3, 3}, 129},
	{{1, 4}, 129},
	{{2, 4}, 129},
};

static const IndustryTileLayout _tile_table_cotton_candy_1 {
	{{0, 0}, 129},
	{{1, 0}, 129},
	{{2, 0}, 129},
	{{3, 0}, 129},
	{{4, 0}, 129},
	{{0, 1}, 129},
	{{1, 1}, 129},
	{{2, 1}, 129},
	{{3, 1}, 129},
	{{4, 1}, 129},
	{{0, 2}, 129},
	{{1, 2}, 129},
	{{2, 2}, 129},
	{{3, 2}, 129},
	{{4, 2}, 129},
	{{0, 3}, 129},
	{{1, 3}, 129},
	{{2, 3}, 129},
	{{3, 3}, 129},
	{{4, 3}, 129},
	{{1, 4}, 129},
	{{2, 4}, 129},
	{{3, 4}, 129},
};

static const std::vector<IndustryTileLayout> _tile_table_cotton_candy {
	_tile_table_cotton_candy_0,
	_tile_table_cotton_candy_1,
};

static const IndustryTileLayout _tile_table_candy_factory_0 {
	{{0, 0}, 131},
	{{0, 1}, 132},
	{{1, 0}, 133},
	{{1, 1}, 134},
	{{0, 2}, 131},
	{{0, 3}, 132},
	{{1, 2}, 133},
	{{1, 3}, 134},
	{{2, 1}, 131},
	{{2, 2}, 132},
	{{3, 1}, 133},
	{{3, 2}, 134},
};

static const IndustryTileLayout _tile_table_candy_factory_1 {
	{{0, 0}, 131},
	{{0, 1}, 132},
	{{1, 0}, 133},
	{{1, 1}, 134},
	{{2, 0}, 131},
	{{2, 1}, 132},
	{{3, 0}, 133},
	{{3, 1}, 134},
	{{1, 2}, 131},
	{{1, 3}, 132},
	{{2, 2}, 133},
	{{2, 3}, 134},
};

static const std::vector<IndustryTileLayout> _tile_table_candy_factory {
	_tile_table_candy_factory_0,
	_tile_table_candy_factory_1,
};

static const IndustryTileLayout _tile_table_battery_farm_0 {
	{{0, 0}, 135},
	{{0, 1}, 135},
	{{0, 2}, 135},
	{{0, 3}, 135},
	{{1, 0}, 135},
	{{1, 1}, 135},
	{{1, 2}, 135},
	{{1, 3}, 135},
	{{2, 0}, 135},
	{{2, 1}, 135},
	{{2, 2}, 135},
	{{2, 3}, 135},
	{{3, 0}, 135},
	{{3, 1}, 135},
	{{3, 2}, 135},
	{{3, 3}, 135},
	{{4, 0}, 135},
	{{4, 1}, 135},
	{{4, 2}, 135},
	{{4, 3}, 135},
};

static const std::vector<IndustryTileLayout> _tile_table_battery_farm {
	_tile_table_battery_farm_0,
};

static const IndustryTileLayout _tile_table_cola_wells_0 {
	{{0, 0}, 137},
	{{0, 1}, 137},
	{{0, 2}, 137},
	{{1, 0}, 137},
	{{1, 1}, 137},
	{{1, 2}, 137},
	{{2, 1}, 137},
	{{2, 2}, 137},
};

static const IndustryTileLayout _tile_table_cola_wells_1 {
	{{0, 1}, 137},
	{{0, 2}, 137},
	{{0, 3}, 137},
	{{1, 0}, 137},
	{{1, 1}, 137},
	{{1, 2}, 137},
	{{2, 1}, 137},
};

static const std::vector<IndustryTileLayout> _tile_table_cola_wells {
	_tile_table_cola_wells_0,
	_tile_table_cola_wells_1,
};

static const IndustryTileLayout _tile_table_toy_shop_0 {
	{{0, 0}, 138},
	{{0, 1}, 139},
	{{1, 0}, 140},
	{{1, 1}, 141},
};

static const std::vector<IndustryTileLayout> _tile_table_toy_shop {
	_tile_table_toy_shop_0,
};

static const IndustryTileLayout _tile_table_toy_factory_0 {
	{{0, 0}, 147},
	{{0, 1}, 142},
	{{1, 0}, 147},
	{{1, 1}, 143},
	{{2, 0}, 147},
	{{2, 1}, 144},
	{{3, 0}, 146},
	{{3, 1}, 145},
};

static const std::vector<IndustryTileLayout> _tile_table_toy_factory {
	_tile_table_toy_factory_0,
};

static const IndustryTileLayout _tile_table_plastic_fountain_0 {
	{{0, 0}, 148},
	{{0, 1}, 151},
	{{0, 2}, 154},
};

static const IndustryTileLayout _tile_table_plastic_fountain_1 {
	{{0, 0}, 148},
	{{1, 0}, 151},
	{{2, 0}, 154},
};

static const std::vector<IndustryTileLayout> _tile_table_plastic_fountain {
	_tile_table_plastic_fountain_0,
	_tile_table_plastic_fountain_1,
};

static const IndustryTileLayout _tile_table_fizzy_drink_0 {
	{{0, 0}, 156},
	{{0, 1}, 157},
	{{1, 0}, 158},
	{{1, 1}, 159},
};

static const std::vector<IndustryTileLayout> _tile_table_fizzy_drink {
	_tile_table_fizzy_drink_0,
};

static const IndustryTileLayout _tile_table_bubble_generator_0 {
	{{0, 0}, 163},
	{{0, 1}, 160},
	{{1, 0}, 163},
	{{1, 1}, 161},
	{{2, 0}, 163},
	{{2, 1}, 162},
	{{0, 2}, 163},
	{{0, 3}, 160},
	{{1, 2}, 163},
	{{1, 3}, 161},
	{{2, 2}, 163},
	{{2, 3}, 162},
};

static const std::vector<IndustryTileLayout> _tile_table_bubble_generator {
	_tile_table_bubble_generator_0,
};

static const IndustryTileLayout _tile_table_toffee_quarry_0 {
	{{0, 0}, 164},
	{{1, 0}, 165},
	{{2, 0}, 166},
};

static const std::vector<IndustryTileLayout> _tile_table_toffee_quarry {
	_tile_table_toffee_quarry_0,
};

static const IndustryTileLayout _tile_table_sugar_mine_0 {
	{{0, 0}, 167},
	{{0, 1}, 168},
	{{1, 0}, 169},
	{{1, 1}, 170},
	{{2, 0}, 171},
	{{2, 1}, 172},
	{{3, 0}, 173},
	{{3, 1}, 174},
};

static const std::vector<IndustryTileLayout> _tile_table_sugar_mine {
	_tile_table_sugar_mine_0,
};

/** Array with saw sound, for sawmill */
static const std::initializer_list<uint8_t> _sawmill_sounds = { SND_28_SAWMILL };

/** Array with whistle sound, for factory */
static const std::initializer_list<uint8_t> _factory_sounds = { SND_03_FACTORY };

/** Array with 3 animal sounds, for farms */
static const std::initializer_list<uint8_t> _farm_sounds = { SND_24_FARM_1, SND_25_FARM_2, SND_26_FARM_3 };

/** Array with... hem... a sound of toyland */
static const std::initializer_list<uint8_t> _plastic_mine_sounds = { SND_33_PLASTIC_MINE };

enum IndustryTypes : uint8_t {
	IT_COAL_MINE           =   0,
	IT_POWER_STATION       =   1,
	IT_SAWMILL             =   2,
	IT_FOREST              =   3,
	IT_OIL_REFINERY        =   4,
	IT_OIL_RIG             =   5,
	IT_FACTORY             =   6,
	IT_PRINTING_WORKS      =   7,
	IT_STEEL_MILL          =   8,
	IT_FARM                =   9,
	IT_COPPER_MINE         =  10,
	IT_OIL_WELL            =  11,
	IT_BANK_TEMP           =  12,
	IT_FOOD_PROCESS        =  13,
	IT_PAPER_MILL          =  14,
	IT_GOLD_MINE           =  15,
	IT_BANK_TROPIC_ARCTIC  =  16,
	IT_DIAMOND_MINE        =  17,
	IT_IRON_MINE           =  18,
	IT_FRUIT_PLANTATION    =  19,
	IT_RUBBER_PLANTATION   =  20,
	IT_WATER_SUPPLY        =  21,
	IT_WATER_TOWER         =  22,
	IT_FACTORY_2           =  23,
	IT_FARM_2              =  24,
	IT_LUMBER_MILL         =  25,
	IT_COTTON_CANDY        =  26,
	IT_CANDY_FACTORY       =  27,
	IT_BATTERY_FARM        =  28,
	IT_COLA_WELLS          =  29,
	IT_TOY_SHOP            =  30,
	IT_TOY_FACTORY         =  31,
	IT_PLASTIC_FOUNTAINS   =  32,
	IT_FIZZY_DRINK_FACTORY =  33,
	IT_BUBBLE_GENERATOR    =  34,
	IT_TOFFEE_QUARRY       =  35,
	IT_SUGAR_MINE          =  36,
	IT_END,
};

/**
 * Writes the properties of an industry into the IndustrySpec struct.
 * @param tbl  tile table
 * @param snd  sounds table
 * @param d    cost multiplier
 * @param pc   prospecting chance
 * @param ai1  appear chance ingame - temperate
 * @param ai2  appear chance ingame - arctic
 * @param ai3  appear chance ingame - tropic
 * @param ai4  appear chance ingame - toyland
 * @param ag1  appear chance random creation - temperate
 * @param ag2  appear chance random creation - arctic
 * @param ag3  appear chance random creation - tropic
 * @param ag4  appear chance random creation - toyland
 * @param col  map colour
 * @param c1   industry proximity refusal - 1st
 * @param c2   industry proximity refusal - 2nd
 * @param c3   industry proximity refusal - 3th
 * @param proc check procedure index
 * @param p1   produce cargo 1
 * @param r1   rate of production 1
 * @param p2   produce cargo 2
 * @param r2   rate of production 1
 * @param m    minimum cargo moved to station
 * @param a1   accepted cargo 1
 * @param im1  input multiplier for cargo 1
 * @param a2   accepted cargo 2
 * @param im2  input multiplier for cargo 2
 * @param a3   accepted cargo 3
 * @param im3  input multiplier for cargo 3
 * @param pr   industry life (actually, the same as extractive, organic, processing in ttdpatch's specs)
 * @param clim climate availability
 * @param bev  industry behaviour
 * @param in   name
 * @param intx text while building
 * @param s1   text for closure
 * @param s2   text for production up
 * @param s3   text for production down
 */

#define MI(tbl, snd, d, pc, ai1, ai2, ai3, ai4, ag1, ag2, ag3, ag4, col, \
			c1, c2, c3, proc, p1, r1, p2, r2, m, a1, im1, a2, im2, a3, im3, pr, clim, bev, in, intx, s1, s2, s3) \
		{tbl, d, 0, pc, {c1, c2, c3}, proc, \
		{INVALID_CARGO, INVALID_CARGO, INVALID_CARGO, INVALID_CARGO, INVALID_CARGO, INVALID_CARGO, INVALID_CARGO, INVALID_CARGO, INVALID_CARGO, INVALID_CARGO, INVALID_CARGO, INVALID_CARGO, INVALID_CARGO, INVALID_CARGO, INVALID_CARGO, INVALID_CARGO}, \
		{r1, r2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, m, \
		{INVALID_CARGO, INVALID_CARGO, INVALID_CARGO, INVALID_CARGO, INVALID_CARGO, INVALID_CARGO, INVALID_CARGO, INVALID_CARGO, INVALID_CARGO, INVALID_CARGO, INVALID_CARGO, INVALID_CARGO, INVALID_CARGO, INVALID_CARGO, INVALID_CARGO, INVALID_CARGO}, \
		{{im1, 0}, {im2, 0}, {im3, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}}, \
		pr, clim, bev, PixelColour{col}, in, intx, s1, s2, s3, STR_UNDEFINED, {ai1, ai2, ai3, ai4}, {ag1, ag2, ag3, ag4}, \
		IndustryCallbackMasks{}, true, SubstituteGRFFileProps(IT_INVALID), snd, {}, \
		{{p1, p2}}, {{a1, a2, a3}}}
	/* Format:
	   tile table                              count and sounds table
	   cost multiplier                         appear chances(4ingame, 4random)  map colour
	   cannot be close to these industries (3 times)             check proc
	   (produced cargo + rate) (twice)         minimum cargo moved to station
	   3 accepted cargo and their corresponding input multiplier
	   industry life                           climate availability
	   industry behaviours
	   industry name                           building text
	   messages : Closure                      production up                      production down   */
extern const IndustrySpec _origin_industry_specs[NEW_INDUSTRYOFFSET] = {
	MI(_tile_table_coal_mine,                  {},
	   210,  0xB3333333,                       2, 3, 0, 0,    8, 8, 0, 0,          1,
	   IT_POWER_STATION,  IT_INVALID,          IT_INVALID,       IndustryCheck::None,
	   CT_COAL,       15, CT_INVALID,       0, 5,
	   CT_INVALID,   256, CT_INVALID,     256, CT_INVALID,   256,
	   IndustryLifeType::Extractive,                LandscapeTypes({LandscapeType::Temperate, LandscapeType::Arctic}),
	   IndustryBehaviour::CanSubsidence,
	   STR_INDUSTRY_NAME_COAL_MINE,                     STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_GENERAL,    STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_COAL,   STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_power_station,              {},
	   240,  0xFFFFFFFF,                       2, 2, 0, 0,    5, 5, 0, 0,        184,
	   IT_COAL_MINE,      IT_INVALID,          IT_INVALID,       IndustryCheck::None,
	   CT_INVALID,     0, CT_INVALID,       0, 5,
	   CT_COAL,      256, CT_INVALID,     256, CT_INVALID,   256,
	   INDUSTRYLIFE_BLACK_HOLE,                LandscapeTypes({LandscapeType::Temperate, LandscapeType::Arctic}),
	   {},
	   STR_INDUSTRY_NAME_POWER_STATION,                 STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_GENERAL,    STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_sawmill,                    _sawmill_sounds,
	   224,  0xFFFFFFFF,                       2, 0, 0, 0,    5, 0, 0, 0,        194,
	   IT_FOREST,         IT_INVALID,          IT_INVALID,       IndustryCheck::None,
	   CT_GOODS,       0, CT_INVALID,       0, 5,
	   CT_WOOD,      256, CT_INVALID,     256, CT_INVALID,   256,
	   IndustryLifeType::Processing,                LandscapeType::Temperate,
	   {},
	   STR_INDUSTRY_NAME_SAWMILL,                       STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_SUPPLY_PROBLEMS,      STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_forest,                     {},
	   200,  0xBFFFFFFF,                       3, 4, 0, 0,    5, 5, 0, 0,         86,
	   IT_SAWMILL,        IT_PAPER_MILL,       IT_INVALID,       IndustryCheck::Forest,
	   CT_WOOD,       13, CT_INVALID,       0, 30,
	   CT_INVALID,   256, CT_INVALID,     256, CT_INVALID,   256,
	   IndustryLifeType::Organic,                   LandscapeTypes({LandscapeType::Temperate, LandscapeType::Arctic}),
	   {},
	   STR_INDUSTRY_NAME_FOREST,                        STR_NEWS_INDUSTRY_PLANTED,
	   STR_NEWS_INDUSTRY_CLOSURE_GENERAL,    STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_FARM),

	MI(_tile_table_oil_refinery,               {},
	   244,  0xFFFFFFFF,                       2, 2, 2, 0,    4, 4, 4, 0,        191,
	   IT_OIL_RIG,        IT_INVALID,          IT_INVALID,       IndustryCheck::Refinery,
	   CT_GOODS,       0, CT_INVALID,       0, 5,
	   CT_OIL,       256, CT_INVALID,     256, CT_INVALID,   256,
	   IndustryLifeType::Processing,                LandscapeTypes({LandscapeType::Temperate, LandscapeType::Arctic, LandscapeType::Tropic}),
	   IndustryBehaviour::AirplaneAttacks,
	   STR_INDUSTRY_NAME_OIL_REFINERY,                  STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_SUPPLY_PROBLEMS,      STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_oil_rig,                    {},
	   240,  0x99999999,                       6, 0, 0, 0,    0, 0, 0, 0,        152,
	   IT_OIL_REFINERY,   IT_INVALID,          IT_INVALID,       IndustryCheck::OilRig,
	   CT_OIL,        15, CT_PASSENGERS,    2, 5,
	   CT_INVALID,     0, CT_INVALID,       0, CT_INVALID,     0,
	   IndustryLifeType::Extractive,                LandscapeType::Temperate,
	   IndustryBehaviours({IndustryBehaviour::BuiltOnWater, IndustryBehaviour::After1960, IndustryBehaviour::AIAirShipRoutes}),
	   STR_INDUSTRY_NAME_OIL_RIG,                       STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_GENERAL,    STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_OIL,   STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_factory,                    _factory_sounds,
	   208,  0xFFFFFFFF,                       2, 0, 0, 0,    5, 0, 0, 0,        174,
	   IT_FARM,           IT_STEEL_MILL,       IT_INVALID,       IndustryCheck::None,
	   CT_GOODS,       0, CT_INVALID,       0, 5,
	   MCT_LIVESTOCK_FRUIT, 256, MCT_GRAIN_WHEAT_MAIZE,       256, CT_STEEL,    256,
	   IndustryLifeType::Processing,                LandscapeType::Temperate,
	   IndustryBehaviour::ChopperAttacks,
	   STR_INDUSTRY_NAME_FACTORY,                       STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_SUPPLY_PROBLEMS,      STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_printing_works,             _factory_sounds,
	   208,  0xFFFFFFFF,                       0, 2, 0, 0,    0, 5, 0, 0,        174,
	   IT_PAPER_MILL,     IT_INVALID,          IT_INVALID,       IndustryCheck::None,
	   CT_GOODS,       0, CT_INVALID,       0, 5,
	   CT_PAPER,     256, CT_INVALID,     256, CT_INVALID,   256,
	   IndustryLifeType::Processing,                LandscapeType::Arctic,
	   {},
	   STR_INDUSTRY_NAME_PRINTING_WORKS,                STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_SUPPLY_PROBLEMS,      STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_steel_mill,                 {},
	   215,  0xFFFFFFFF,                       2, 0, 0, 0,    5, 0, 0, 0,         10,
	   IT_IRON_MINE,      IT_FACTORY,          IT_INVALID,       IndustryCheck::None,
	   CT_STEEL,       0, CT_INVALID,       0, 5,
	   CT_IRON_ORE,  256, CT_INVALID,     256, CT_INVALID,   256,
	   IndustryLifeType::Processing,                LandscapeType::Temperate,
	   {},
	   STR_INDUSTRY_NAME_STEEL_MILL,                    STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_SUPPLY_PROBLEMS,      STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_farm,                       _farm_sounds,
	   250,  0xD9999999,                       2, 4, 0, 0,    9, 9, 0, 0,         48,
	   IT_FACTORY,        IT_FOOD_PROCESS,     IT_INVALID,       IndustryCheck::Farm,
	   MCT_GRAIN_WHEAT_MAIZE,      10, MCT_LIVESTOCK_FRUIT,    10, 5,
	   CT_INVALID,   256, CT_INVALID,     256, CT_INVALID,   256,
	   IndustryLifeType::Organic,                   LandscapeTypes({LandscapeType::Temperate, LandscapeType::Arctic}),
	   IndustryBehaviours({IndustryBehaviour::PlantFields, IndustryBehaviour::PlantOnBuild}),
	   STR_INDUSTRY_NAME_FARM,                          STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_GENERAL,    STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_FARM, STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_FARM),

	MI(_tile_table_copper_mine,                {},
	   205,  0xB3333333,                       0, 0, 3, 0,    0, 0, 4, 0,         10,
	   IT_FACTORY_2,      IT_INVALID,          IT_INVALID,       IndustryCheck::None,
	   CT_COPPER_ORE, 10, CT_INVALID,       0, 5,
	   CT_INVALID,   256, CT_INVALID,     256, CT_INVALID,   256,
	   IndustryLifeType::Extractive,                LandscapeType::Tropic,
	   {},
	   STR_INDUSTRY_NAME_COPPER_ORE_MINE,               STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_GENERAL,    STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_oil_well,                   {},
	   220,  0x99999999,                       0, 5, 3, 0,    4, 5, 5, 0,        152,
	   IT_OIL_REFINERY,   IT_INVALID,          IT_INVALID,       IndustryCheck::None,
	   CT_OIL,        12, CT_INVALID,       0, 5,
	   CT_INVALID,   256, CT_INVALID,     256, CT_INVALID,   256,
	   IndustryLifeType::Extractive,                LandscapeTypes({LandscapeType::Temperate, LandscapeType::Arctic, LandscapeType::Tropic}),
	   IndustryBehaviours({IndustryBehaviour::DontIncrProd, IndustryBehaviour::Before1950}),
	   STR_INDUSTRY_NAME_OIL_WELLS,                     STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_GENERAL,    STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_OIL,   STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_bank,                       {},
	   255,  0xA6666666,                       7, 0, 0, 0,    0, 0, 0, 0,         15,
	   IT_BANK_TEMP,      IT_INVALID,          IT_INVALID,       IndustryCheck::None,
	   MCT_VALUABLES_GOLD_DIAMONDS,   6, CT_INVALID,       0, 5,
	   MCT_VALUABLES_GOLD_DIAMONDS,   0, CT_INVALID,       0, CT_INVALID,     0,
	   INDUSTRYLIFE_BLACK_HOLE,                LandscapeType::Temperate,
	   IndustryBehaviour::Town1200More,
	   STR_INDUSTRY_NAME_BANK,                          STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_GENERAL,    STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_food_process,               {},
	   206,  0xFFFFFFFF,                       0, 2, 2, 0,    0, 3, 4, 0,         55,
	   IT_FRUIT_PLANTATION, IT_FARM,           IT_FARM_2,        IndustryCheck::None,
	   CT_FOOD,        0, CT_INVALID,       0, 5,
	   MCT_LIVESTOCK_FRUIT,     256, MCT_GRAIN_WHEAT_MAIZE,       256, CT_INVALID,   256,
	   IndustryLifeType::Processing,                LandscapeTypes({LandscapeType::Arctic, LandscapeType::Tropic}),
	   {},
	   STR_INDUSTRY_NAME_FOOD_PROCESSING_PLANT,         STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_SUPPLY_PROBLEMS,      STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_paper_mill,                 _sawmill_sounds,
	   227,  0xFFFFFFFF,                       0, 2, 0, 0,    0, 5, 0, 0,         10,
	   IT_FOREST,         IT_PRINTING_WORKS,   IT_INVALID,       IndustryCheck::None,
	   CT_PAPER,       0, CT_INVALID,       0, 5,
	   CT_WOOD,      256, CT_INVALID,     256, CT_INVALID,   256,
	   IndustryLifeType::Processing,                LandscapeType::Arctic,
	   {},
	   STR_INDUSTRY_NAME_PAPER_MILL,                    STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_SUPPLY_PROBLEMS,      STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_gold_mine,                  {},
	   208,  0x99999999,                       0, 3, 0, 0,    0, 4, 0, 0,        194,
	   IT_BANK_TROPIC_ARCTIC, IT_INVALID,      IT_INVALID,       IndustryCheck::None,
	   MCT_VALUABLES_GOLD_DIAMONDS,        7, CT_INVALID,       0, 5,
	   CT_INVALID,   256, CT_INVALID,     256, CT_INVALID,   256,
	   IndustryLifeType::Extractive,                LandscapeType::Arctic,
	   {},
	   STR_INDUSTRY_NAME_GOLD_MINE,                     STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_GENERAL,    STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_bank2,                      {},
	   151,  0xA6666666,                       0, 3, 3, 0,    0, 6, 5, 0,         15,
	   IT_GOLD_MINE,      IT_DIAMOND_MINE,     IT_INVALID,       IndustryCheck::None,
	   CT_INVALID,     0, CT_INVALID,       0, 5,
	   MCT_VALUABLES_GOLD_DIAMONDS,      256, CT_INVALID,     256, CT_INVALID,   256,
	   INDUSTRYLIFE_BLACK_HOLE,                LandscapeTypes({LandscapeType::Arctic, LandscapeType::Tropic}),
	   IndustryBehaviour::OnlyInTown,
	   STR_INDUSTRY_NAME_BANK_TROPIC_ARCTIC,                          STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_GENERAL,    STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_diamond_mine,               {},
	   213,  0x99999999,                       0, 0, 3, 0,    0, 0, 4, 0,        184,
	   IT_BANK_TROPIC_ARCTIC, IT_INVALID,      IT_INVALID,       IndustryCheck::None,
	   MCT_VALUABLES_GOLD_DIAMONDS,    7, CT_INVALID,       0, 5,
	   CT_INVALID,   256, CT_INVALID,     256, CT_INVALID,   256,
	   IndustryLifeType::Extractive,                LandscapeType::Tropic,
	   {},
	   STR_INDUSTRY_NAME_DIAMOND_MINE,                  STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_GENERAL,    STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_iron_mine,                  {},
	   220,  0xB3333333,                       2, 0, 0, 0,    5, 0, 0, 0,         55,
	   IT_STEEL_MILL,     IT_INVALID,          IT_INVALID,       IndustryCheck::None,
	   CT_IRON_ORE,   10, CT_INVALID,       0, 5,
	   CT_INVALID,   256, CT_INVALID,     256, CT_INVALID,   256,
	   IndustryLifeType::Extractive,                LandscapeType::Temperate,
	   {},
	   STR_INDUSTRY_NAME_IRON_ORE_MINE,                 STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_GENERAL,    STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_fruit_plantation,           {},
	   225,  0xBFFFFFFF,                       0, 0, 2, 0,    0, 0, 4, 0,         86,
	   IT_FOOD_PROCESS,   IT_INVALID,          IT_INVALID,       IndustryCheck::Plantation,
	   MCT_LIVESTOCK_FRUIT,      10, CT_INVALID,       0, 15,
	   CT_INVALID,   256, CT_INVALID,     256, CT_INVALID,   256,
	   IndustryLifeType::Organic,                   LandscapeType::Tropic,
	   {},
	   STR_INDUSTRY_NAME_FRUIT_PLANTATION,              STR_NEWS_INDUSTRY_PLANTED,
	   STR_NEWS_INDUSTRY_CLOSURE_GENERAL,    STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_FARM, STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_FARM),

	MI(_tile_table_rubber_plantation,          {},
	   218,  0xBFFFFFFF,                       0, 0, 3, 0,    0, 0, 4, 0,         39,
	   IT_FACTORY_2,      IT_INVALID,          IT_INVALID,       IndustryCheck::Plantation,
	   CT_RUBBER,     10, CT_INVALID,       0, 15,
	   CT_INVALID,   256, CT_INVALID,     256, CT_INVALID,   256,
	   IndustryLifeType::Organic,                   LandscapeType::Tropic,
	   {},
	   STR_INDUSTRY_NAME_RUBBER_PLANTATION,             STR_NEWS_INDUSTRY_PLANTED,
	   STR_NEWS_INDUSTRY_CLOSURE_GENERAL,    STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_FARM, STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_FARM),

	MI(_tile_table_water_supply,               {},
	   199,  0xB3333333,                       0, 0, 3, 0,    0, 0, 4, 0,         37,
	   IT_WATER_TOWER,    IT_INVALID,          IT_INVALID,       IndustryCheck::Water,
	   CT_WATER,      12, CT_INVALID,       0, 5,
	   CT_INVALID,   256, CT_INVALID,     256, CT_INVALID,   256,
	   IndustryLifeType::Extractive,                LandscapeType::Tropic,
	   {},
	   STR_INDUSTRY_NAME_WATER_SUPPLY,                  STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_GENERAL,    STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_water_tower,                {},
	   115,  0xFFFFFFFF,                       0, 0, 4, 0,    0, 0, 8, 0,        208,
	   IT_WATER_SUPPLY,   IT_INVALID,          IT_INVALID,       IndustryCheck::Water,
	   CT_INVALID,     0, CT_INVALID,       0, 5,
	   CT_WATER,     256, CT_INVALID,     256, CT_INVALID,   256,
	   INDUSTRYLIFE_BLACK_HOLE,                LandscapeType::Tropic,
	   IndustryBehaviour::OnlyInTown,
	   STR_INDUSTRY_NAME_WATER_TOWER,                   STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_GENERAL,    STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_factory2,                   _factory_sounds,
	   208,  0xFFFFFFFF,                       0, 0, 2, 0,    0, 0, 4, 0,        174,
	   IT_RUBBER_PLANTATION, IT_COPPER_MINE,   IT_LUMBER_MILL,   IndustryCheck::Plantation,
	   CT_GOODS,       0, CT_INVALID,       0, 5,
	   CT_RUBBER,    256, CT_COPPER_ORE,  256, CT_WOOD,      256,
	   IndustryLifeType::Processing,                LandscapeType::Tropic,
	   {},
	   STR_INDUSTRY_NAME_FACTORY_2,                       STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_SUPPLY_PROBLEMS,      STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_farm2,                      {},
	   250,  0xD9999999,                       0, 0, 1, 0,    0, 0, 2, 0,         48,
	   IT_FOOD_PROCESS,   IT_INVALID,          IT_INVALID,       IndustryCheck::Plantation,
	   MCT_GRAIN_WHEAT_MAIZE,      11, CT_INVALID,       0, 5,
	   CT_INVALID,   256, CT_INVALID,     256, CT_INVALID,   256,
	   IndustryLifeType::Organic,                   LandscapeType::Tropic,
	   IndustryBehaviours({IndustryBehaviour::PlantFields, IndustryBehaviour::PlantOnBuild}),
	   STR_INDUSTRY_NAME_FARM_2,                          STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_GENERAL,    STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_FARM, STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_FARM),

	MI(_tile_table_lumber_mill,                {},
	   135,  0xFFFFFFFF,                       0, 0, 0, 0,    0, 0, 0, 0,        194,
	   IT_FACTORY_2,      IT_INVALID,          IT_INVALID,       IndustryCheck::Lumbermill,
	   CT_WOOD,        0, CT_INVALID,       0, 5,
	   CT_INVALID,   256, CT_INVALID,     256, CT_INVALID,   256,
	   IndustryLifeType::Processing,                LandscapeType::Tropic,
	   IndustryBehaviour::CutTrees,
	   STR_INDUSTRY_NAME_LUMBER_MILL,                   STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_LACK_OF_TREES,   STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_cotton_candy,               {},
	   195,  0xBFFFFFFF,                       0, 0, 0, 3,    0, 0, 0, 5,         48,
	   IT_CANDY_FACTORY,  IT_INVALID,          IT_INVALID,       IndustryCheck::None,
	   CT_COTTON_CANDY, 13, CT_INVALID,    0, 30,
	   CT_INVALID,   256, CT_INVALID,     256, CT_INVALID,   256,
	   IndustryLifeType::Organic,                   LandscapeType::Toyland,
	   {},
	   STR_INDUSTRY_NAME_COTTON_CANDY_FOREST,           STR_NEWS_INDUSTRY_PLANTED,
	   STR_NEWS_INDUSTRY_CLOSURE_GENERAL,    STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_FARM, STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_candy_factory,              {},
	   206,  0xFFFFFFFF,                       0, 0, 0, 3,    0, 0, 0, 5,        174,
	   IT_COTTON_CANDY,   IT_TOFFEE_QUARRY,    IT_SUGAR_MINE,    IndustryCheck::None,
	   CT_CANDY,       0, CT_INVALID,       0, 5,
	   CT_SUGAR,     256, CT_TOFFEE,      256, CT_COTTON_CANDY, 256,
	   IndustryLifeType::Processing,                LandscapeType::Toyland,
	   {},
	   STR_INDUSTRY_NAME_CANDY_FACTORY,                 STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_SUPPLY_PROBLEMS,      STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_battery_farm,               {},
	   187,  0xB3333333,                       0, 0, 0, 3,    0, 0, 0, 4,         39,
	   IT_TOY_FACTORY,    IT_INVALID,          IT_INVALID,       IndustryCheck::None,
	   CT_BATTERIES,  11, CT_INVALID,       0, 30,
	   CT_INVALID,   256, CT_INVALID,     256, CT_INVALID,   256,
	   IndustryLifeType::Organic,                   LandscapeType::Toyland,
	   {},
	   STR_INDUSTRY_NAME_BATTERY_FARM,                  STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_GENERAL,    STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_FARM, STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_FARM),

	MI(_tile_table_cola_wells,                 {},
	   193,  0x99999999,                       0, 0, 0, 3,    0, 0, 0, 5,         55,
	   IT_FIZZY_DRINK_FACTORY, IT_INVALID,     IT_INVALID,       IndustryCheck::None,
	   CT_COLA,       12, CT_INVALID,       0, 5,
	   CT_INVALID,   256, CT_INVALID,     256, CT_INVALID,   256,
	   IndustryLifeType::Extractive,                LandscapeType::Toyland,
	   {},
	   STR_INDUSTRY_NAME_COLA_WELLS,                    STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_GENERAL,    STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_toy_shop,                   {},
	   133,  0xFFFFFFFF,                       0, 0, 0, 3,    0, 0, 0, 4,        208,
	   IT_TOY_FACTORY,    IT_INVALID,          IT_INVALID,       IndustryCheck::None,
	   CT_INVALID,     0, CT_INVALID,       0, 5,
	   CT_TOYS,      256, CT_INVALID,     256, CT_INVALID,   256,
	   INDUSTRYLIFE_BLACK_HOLE,                LandscapeType::Toyland,
	   IndustryBehaviour::OnlyNearTown,
	   STR_INDUSTRY_NAME_TOY_SHOP,                      STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_SUPPLY_PROBLEMS,      STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_toy_factory,                {},
	   163,  0xFFFFFFFF,                       0, 0, 0, 3,    0, 0, 0, 5,          10,
	   IT_PLASTIC_FOUNTAINS, IT_BATTERY_FARM,  IT_TOY_SHOP,     IndustryCheck::None,
	   CT_TOYS,        0, CT_INVALID,       0, 5,
	   CT_PLASTIC,   256, CT_BATTERIES,   256, CT_INVALID,   256,
	   IndustryLifeType::Processing,                LandscapeType::Toyland,
	   {},
	   STR_INDUSTRY_NAME_TOY_FACTORY,                   STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_SUPPLY_PROBLEMS,      STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_plastic_fountain,           _plastic_mine_sounds,
	   192,  0xA6666666,                       0, 0, 0, 3,    0, 0, 0, 5,         37,
	   IT_TOY_FACTORY,    IT_INVALID,          IT_INVALID,       IndustryCheck::None,
	   CT_PLASTIC,    14, CT_INVALID,       0, 5,
	   CT_INVALID,   256, CT_INVALID,     256, CT_INVALID,   256,
	   IndustryLifeType::Extractive,                LandscapeType::Toyland,
	   {},
	   STR_INDUSTRY_NAME_PLASTIC_FOUNTAINS,             STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_GENERAL,    STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_fizzy_drink,                {},
	   177,  0xFFFFFFFF,                       0, 0, 0, 3,    0, 0, 0, 4,        184,
	   IT_COLA_WELLS,     IT_BUBBLE_GENERATOR, IT_INVALID,       IndustryCheck::None,
	   CT_FIZZY_DRINKS, 0, CT_INVALID,      0, 5,
	   CT_COLA,       256, CT_BUBBLES,    256, CT_INVALID,   256,
	   IndustryLifeType::Processing,                LandscapeType::Toyland,
	   {},
	   STR_INDUSTRY_NAME_FIZZY_DRINK_FACTORY,           STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_SUPPLY_PROBLEMS,      STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_bubble_generator,           {},
	   203,  0xB3333333,                       0, 0, 0, 3,    0, 0, 0, 5,        152,
	   IT_FIZZY_DRINK_FACTORY, IT_INVALID,     IT_INVALID,       IndustryCheck::BubbleGen,
	   CT_BUBBLES,    13, CT_INVALID,       0, 5,
	   CT_INVALID,   256, CT_INVALID,     256, CT_INVALID,   256,
	   IndustryLifeType::Extractive,                LandscapeType::Toyland,
	   {},
	   STR_INDUSTRY_NAME_BUBBLE_GENERATOR,              STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_GENERAL,    STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_toffee_quarry,              {},
	   213,  0xCCCCCCCC,                       0, 0, 0, 3,    0, 0, 0, 5,        194,
	   IT_CANDY_FACTORY,  IT_INVALID,          IT_INVALID,       IndustryCheck::None,
	   CT_TOFFEE,     10, CT_INVALID,       0, 5,
	   CT_INVALID,   256, CT_INVALID,     256, CT_INVALID,   256,
	   IndustryLifeType::Extractive,                LandscapeType::Toyland,
	   {},
	   STR_INDUSTRY_NAME_TOFFEE_QUARRY,                 STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_GENERAL,    STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_sugar_mine,                 {},
	   210,  0xBFFFFFFF,                       0, 0, 0, 2,    0, 0, 0, 4,         15,
	   IT_CANDY_FACTORY,  IT_INVALID,          IT_INVALID,       IndustryCheck::None,
	   CT_SUGAR,      11, CT_INVALID,       0, 5,
	   CT_INVALID,   256, CT_INVALID,     256, CT_INVALID,   256,
	   IndustryLifeType::Extractive,                LandscapeType::Toyland,
	   {},
	   STR_INDUSTRY_NAME_SUGAR_MINE,                    STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_GENERAL,    STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),
};
#undef MI

static const IndustryTileSpec _origin_industry_tile_specs[NEW_INDUSTRYTILEOFFSET] = {
	/* Coal Mine */
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, true},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{1, CT_PASSENGERS, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},

	/* Power Station */
	{0, CT_INVALID, 0, CT_INVALID, false},
	{1, CT_PASSENGERS, 8, CT_COAL, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},

	/* Sawmill */
	{1, CT_PASSENGERS, 0, CT_INVALID, false},
	{1, CT_PASSENGERS, 0, CT_INVALID, false},
	{1, CT_PASSENGERS, 8, CT_WOOD, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},

	/* Forest Artic, temperate */
	{0, CT_INVALID, 0, CT_INVALID, 0, CT_INVALID, Corner::Steep, 17, INDUSTRYTILE_NOANIM, false}, ///< Chopping forest
	{0, CT_INVALID, 0, CT_INVALID, 0, CT_INVALID, Corner::Steep, INDUSTRYTILE_NOANIM, 16, false}, ///< Growing forest

	/* Oil refinery */
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 8, CT_OIL, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{1, CT_PASSENGERS, 0, CT_INVALID, false},

	/* Oil Rig */
	{0, CT_INVALID, 8, CT_PASSENGERS, false},
	{0, CT_INVALID, 8, CT_MAIL, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},

	/* Oil Wells arctic, temperate and sub-tropical */
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, 0, CT_INVALID, Corner::Steep, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, true},
	{0, CT_INVALID, 0, CT_INVALID, 0, CT_INVALID, Corner::Steep, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, true},
	{0, CT_INVALID, 0, CT_INVALID, 0, CT_INVALID, Corner::Steep, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, true},

	/* Farm tropic, arctic and temperate */
	{1, CT_PASSENGERS, 0, CT_INVALID, false},
	{1, CT_PASSENGERS, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},

	/* Factory temperate */
	{8, MCT_GRAIN_WHEAT_MAIZE, 8, MCT_LIVESTOCK_FRUIT, 8, CT_STEEL, Corner::Steep, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false},
	{8, MCT_GRAIN_WHEAT_MAIZE, 8, MCT_LIVESTOCK_FRUIT, 8, CT_STEEL, Corner::Steep, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false},
	{8, MCT_GRAIN_WHEAT_MAIZE, 8, MCT_LIVESTOCK_FRUIT, 8, CT_STEEL, Corner::Steep, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false},
	{8, MCT_GRAIN_WHEAT_MAIZE, 8, MCT_LIVESTOCK_FRUIT, 8, CT_STEEL, Corner::Steep, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false},

	/* Printing works */
	{0, CT_INVALID, 8, CT_PAPER, false},
	{0, CT_INVALID, 8, CT_PAPER, false},
	{0, CT_INVALID, 8, CT_PAPER, false},
	{0, CT_INVALID, 8, CT_PAPER, false},

	/* Copper ore mine */
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, 0, CT_INVALID, Corner::Steep, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, true},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{1, CT_PASSENGERS, 0, CT_INVALID, false},
	{1, CT_PASSENGERS, 0, CT_INVALID, false},

	/* Steel mill */
	{1, CT_PASSENGERS, 8, CT_IRON_ORE, false},
	{1, CT_PASSENGERS, 8, CT_IRON_ORE, false},
	{1, CT_PASSENGERS, 8, CT_IRON_ORE, false},
	{1, CT_PASSENGERS, 8, CT_IRON_ORE, false},
	{1, CT_PASSENGERS, 8, CT_IRON_ORE, false},
	{1, CT_PASSENGERS, 8, CT_IRON_ORE, false},

	/* Bank temperate*/
	{1, CT_PASSENGERS, 8, MCT_VALUABLES_GOLD_DIAMONDS, 0, CT_INVALID, Corner::E, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false},
	{1, CT_PASSENGERS, 8, MCT_VALUABLES_GOLD_DIAMONDS, 0, CT_INVALID, Corner::S, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false},

	/* Food processing plant, tropic and arctic. CT_MAIZE or CT_WHEAT, CT_LIVESTOCK or CT_FRUIT*/
	{8, MCT_GRAIN_WHEAT_MAIZE, 8, MCT_LIVESTOCK_FRUIT, false},
	{8, MCT_GRAIN_WHEAT_MAIZE, 8, MCT_LIVESTOCK_FRUIT, false},
	{8, MCT_GRAIN_WHEAT_MAIZE, 8, MCT_LIVESTOCK_FRUIT, false},
	{8, MCT_GRAIN_WHEAT_MAIZE, 8, MCT_LIVESTOCK_FRUIT, false},

	/* Paper mill */
	{0, CT_INVALID, 8, CT_WOOD, false},
	{0, CT_INVALID, 8, CT_WOOD, false},
	{0, CT_INVALID, 8, CT_WOOD, false},
	{0, CT_INVALID, 8, CT_WOOD, false},
	{0, CT_INVALID, 8, CT_WOOD, false},
	{0, CT_INVALID, 8, CT_WOOD, false},
	{0, CT_INVALID, 8, CT_WOOD, false},
	{0, CT_INVALID, 8, CT_WOOD, false},

	/* Gold mine */
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, true},

	/* Bank Sub Arctic */
	{0, CT_INVALID, 8, MCT_VALUABLES_GOLD_DIAMONDS, 0, CT_INVALID, Corner::E, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false},
	{0, CT_INVALID, 8, MCT_VALUABLES_GOLD_DIAMONDS, 0, CT_INVALID, Corner::S, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false},

	/* Diamond mine */
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},

	/* Iron ore Mine */
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},

	/* Fruit plantation */
	{0, CT_INVALID, 0, CT_INVALID, false},

	/* Rubber plantation */
	{0, CT_INVALID, 0, CT_INVALID, false},

	/* Water supply */
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},

	/* Water tower */
	{0, CT_INVALID, 8, CT_WATER, false},

	/* Factory (sub-tropical) */
	{8, CT_COPPER_ORE, 8, CT_RUBBER, 8, CT_WOOD, Corner::Steep, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false},
	{8, CT_COPPER_ORE, 8, CT_RUBBER, 8, CT_WOOD, Corner::Steep, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false},
	{8, CT_COPPER_ORE, 8, CT_RUBBER, 8, CT_WOOD, Corner::Steep, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false},
	{8, CT_COPPER_ORE, 8, CT_RUBBER, 8, CT_WOOD, Corner::Steep, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false},

	/* Lumber mill */
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},

	/* Candyfloss forest */
	{0, CT_INVALID, 0, CT_INVALID, 0, CT_INVALID, Corner::Steep, 130, INDUSTRYTILE_NOANIM, false}, ///< Chopping candyfloss
	{0, CT_INVALID, 0, CT_INVALID, 0, CT_INVALID, Corner::Steep, INDUSTRYTILE_NOANIM, 129, false}, ///< Growing candyfloss

	/* Sweet factory */
	{8, CT_COTTON_CANDY, 8, CT_TOFFEE, 8, CT_SUGAR, Corner::Steep, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false},
	{8, CT_COTTON_CANDY, 8, CT_TOFFEE, 8, CT_SUGAR, Corner::Steep, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false},
	{8, CT_COTTON_CANDY, 8, CT_TOFFEE, 8, CT_SUGAR, Corner::Steep, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false},
	{8, CT_COTTON_CANDY, 8, CT_TOFFEE, 8, CT_SUGAR, Corner::Steep, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false},

	/* Battery farm */
	{0, CT_INVALID, 0, CT_INVALID, 0, CT_INVALID, Corner::Steep, 136, INDUSTRYTILE_NOANIM, false}, ///< Reaping batteries
	{0, CT_INVALID, 0, CT_INVALID, 0, CT_INVALID, Corner::Steep, INDUSTRYTILE_NOANIM, 135, false}, ///< Growing batteries

	/* Cola wells */
	{0, CT_INVALID, 0, CT_INVALID, false},

	/* Toy shop */
	{0, CT_INVALID, 8, CT_TOYS, false},
	{0, CT_INVALID, 8, CT_TOYS, false},
	{0, CT_INVALID, 8, CT_TOYS, false},
	{0, CT_INVALID, 8, CT_TOYS, false},

	/* Toy factory */
	{8, CT_BATTERIES, 8, CT_PLASTIC, false},
	{8, CT_BATTERIES, 8, CT_PLASTIC, false},
	{8, CT_BATTERIES, 8, CT_PLASTIC, false},
	{8, CT_BATTERIES, 8, CT_PLASTIC, false},
	{8, CT_BATTERIES, 8, CT_PLASTIC, false},
	{8, CT_BATTERIES, 8, CT_PLASTIC, false},

	/* Plastic Fountain */
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},

	/* Fizzy drink factory */
	{8, CT_BUBBLES, 8, CT_COLA, false},
	{8, CT_BUBBLES, 8, CT_COLA, false},
	{8, CT_BUBBLES, 8, CT_COLA, false},
	{8, CT_BUBBLES, 8, CT_COLA, false},

	/* Bubble generator */
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},

	/* Toffee quarry */
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},

	/* Sugar mine */
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
	{0, CT_INVALID, 0, CT_INVALID, false},
};

#endif  /* BUILD_INDUSTRY_H */
