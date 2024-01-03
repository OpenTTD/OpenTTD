/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file build_industry.h Tables with default industry layouts and behaviours. */

#ifndef BUILD_INDUSTRY_H
#define BUILD_INDUSTRY_H

/**
 * Definition of an industry tiles layout.
 * @param x offset x of this tile
 * @param y offset y of this tile
 * @param m index of the tile.
 * @see _industry_specs
 * @see IndustryTileTable
 */
#define MK(x, y, m) {{x, y}, m}

static const IndustryTileLayout _tile_table_coal_mine_0 {
	MK(1, 1, 0),
	MK(1, 2, 2),
	MK(0, 0, 5),
	MK(1, 0, 6),
	MK(2, 0, 3),
	MK(2, 2, 3),
};

static const IndustryTileLayout _tile_table_coal_mine_1 {
	MK(1, 1, 0),
	MK(1, 2, 2),
	MK(2, 0, 0),
	MK(2, 1, 2),
	MK(1, 0, 3),
	MK(0, 0, 3),
	MK(0, 1, 4),
	MK(0, 2, 4),
	MK(2, 2, 4),
};

static const IndustryTileLayout _tile_table_coal_mine_2 {
	MK(0, 0, 0),
	MK(0, 1, 2),
	MK(0, 2, 5),
	MK(1, 0, 3),
	MK(1, 1, 3),
	MK(1, 2, 6),
};

static const IndustryTileLayout _tile_table_coal_mine_3 {
	MK(0, 1, 0),
	MK(0, 2, 2),
	MK(0, 3, 4),
	MK(1, 0, 5),
	MK(1, 1, 0),
	MK(1, 2, 2),
	MK(1, 3, 3),
	MK(2, 0, 6),
	MK(2, 1, 4),
	MK(2, 2, 3),
};

static const std::vector<IndustryTileLayout> _tile_table_coal_mine {
	_tile_table_coal_mine_0,
	_tile_table_coal_mine_1,
	_tile_table_coal_mine_2,
	_tile_table_coal_mine_3,
};

static const IndustryTileLayout _tile_table_power_station_0 {
	MK(0, 0, 7),
	MK(0, 1, 9),
	MK(1, 0, 7),
	MK(1, 1, 8),
	MK(2, 0, 7),
	MK(2, 1, 8),
	MK(3, 0, 10),
	MK(3, 1, 10),
};

static const IndustryTileLayout _tile_table_power_station_1 {
	MK(0, 1, 7),
	MK(0, 2, 7),
	MK(1, 0, 8),
	MK(1, 1, 8),
	MK(1, 2, 7),
	MK(2, 0, 9),
	MK(2, 1, 10),
	MK(2, 2, 9),
};

static const IndustryTileLayout _tile_table_power_station_2 {
	MK(0, 0, 7),
	MK(0, 1, 7),
	MK(1, 0, 9),
	MK(1, 1, 8),
	MK(2, 0, 10),
	MK(2, 1, 9),
};

static const std::vector<IndustryTileLayout> _tile_table_power_station {
	_tile_table_power_station_0,
	_tile_table_power_station_1,
	_tile_table_power_station_2,
};

static const IndustryTileLayout _tile_table_sawmill_0 {
	MK(1, 0, 14),
	MK(1, 1, 12),
	MK(1, 2, 11),
	MK(2, 0, 14),
	MK(2, 1, 13),
	MK(0, 0, 15),
	MK(0, 1, 15),
	MK(0, 2, 12),
};

static const IndustryTileLayout _tile_table_sawmill_1 {
	MK(0, 0, 15),
	MK(0, 1, 11),
	MK(0, 2, 14),
	MK(1, 0, 15),
	MK(1, 1, 13),
	MK(1, 2, 12),
	MK(2, 0, 11),
	MK(2, 1, 13),
};

static const std::vector<IndustryTileLayout> _tile_table_sawmill {
	_tile_table_sawmill_0,
	_tile_table_sawmill_1,
};

static const IndustryTileLayout _tile_table_forest_0 {
	MK(0, 0, 16),
	MK(0, 1, 16),
	MK(0, 2, 16),
	MK(0, 3, 16),
	MK(1, 0, 16),
	MK(1, 1, 16),
	MK(1, 2, 16),
	MK(1, 3, 16),
	MK(2, 0, 16),
	MK(2, 1, 16),
	MK(2, 2, 16),
	MK(2, 3, 16),
	MK(3, 0, 16),
	MK(3, 1, 16),
	MK(3, 2, 16),
	MK(3, 3, 16),
	MK(1, 4, 16),
	MK(2, 4, 16),
};

static const IndustryTileLayout _tile_table_forest_1 {
	MK(0, 0, 16),
	MK(1, 0, 16),
	MK(2, 0, 16),
	MK(3, 0, 16),
	MK(4, 0, 16),
	MK(0, 1, 16),
	MK(1, 1, 16),
	MK(2, 1, 16),
	MK(3, 1, 16),
	MK(4, 1, 16),
	MK(0, 2, 16),
	MK(1, 2, 16),
	MK(2, 2, 16),
	MK(3, 2, 16),
	MK(4, 2, 16),
	MK(0, 3, 16),
	MK(1, 3, 16),
	MK(2, 3, 16),
	MK(3, 3, 16),
	MK(4, 3, 16),
	MK(1, 4, 16),
	MK(2, 4, 16),
	MK(3, 4, 16),
};

static const std::vector<IndustryTileLayout> _tile_table_forest {
	_tile_table_forest_0,
	_tile_table_forest_1,
};

static const IndustryTileLayout _tile_table_oil_refinery_0 {
	MK(0, 0, 20),
	MK(0, 1, 21),
	MK(0, 2, 22),
	MK(0, 3, 21),
	MK(1, 0, 20),
	MK(1, 1, 19),
	MK(1, 2, 22),
	MK(1, 3, 20),
	MK(2, 1, 18),
	MK(2, 2, 18),
	MK(2, 3, 18),
	MK(3, 2, 18),
	MK(3, 3, 18),
	MK(2, 0, 23),
	MK(3, 1, 23),
};

static const IndustryTileLayout _tile_table_oil_refinery_1 {
	MK(0, 0, 18),
	MK(0, 1, 18),
	MK(0, 2, 21),
	MK(0, 3, 22),
	MK(0, 4, 20),
	MK(1, 0, 18),
	MK(1, 1, 18),
	MK(1, 2, 19),
	MK(1, 3, 20),
	MK(2, 0, 18),
	MK(2, 1, 18),
	MK(2, 2, 19),
	MK(2, 3, 22),
	MK(1, 4, 23),
	MK(2, 4, 23),
};

static const std::vector<IndustryTileLayout> _tile_table_oil_refinery {
	_tile_table_oil_refinery_0,
	_tile_table_oil_refinery_1,
};

static const IndustryTileLayout _tile_table_oil_rig_0 {
	MK(0, 0, 24),
	MK(0, 1, 24),
	MK(0, 2, 25),
	MK(1, 0, 26),
	MK(1, 1, 27),
	MK(1, 2, 28),
	MK(-4, -4, 255),
	MK(-4, -3, 255),
	MK(-4, -2, 255),
	MK(-4, -1, 255),
	MK(-4, 0, 255),
	MK(-4, 1, 255),
	MK(-4, 2, 255),
	MK(-4, 3, 255),
	MK(-4, 4, 255),
	MK(-4, 5, 255),
	MK(-4, 6, 255),
	MK(-3, 6, 255),
	MK(-2, 6, 255),
	MK(-1, 6, 255),
	MK(0, 6, 255),
	MK(1, 6, 255),
	MK(2, 6, 255),
	MK(3, 6, 255),
	MK(4, 6, 255),
	MK(5, 6, 255),
	MK(5, 5, 255),
	MK(5, 4, 255),
	MK(5, 3, 255),
	MK(5, 2, 255),
	MK(5, 1, 255),
	MK(5, 0, 255),
	MK(5, -1, 255),
	MK(5, -2, 255),
	MK(5, -3, 255),
	MK(5, -4, 255),
	MK(4, -4, 255),
	MK(3, -4, 255),
	MK(2, -4, 255),
	MK(1, -4, 255),
	MK(0, -4, 255),
	MK(-1, -4, 255),
	MK(-2, -4, 255),
	MK(-3, -4, 255),
	MK(2, 0, 255),
	MK(2, -1, 255),
	MK(1, -1, 255),
	MK(0, -1, 255),
	MK(-1, -1, 255),
	MK(-1, 0, 255),
	MK(-1, 1, 255),
	MK(-1, 2, 255),
	MK(-1, 3, 255),
	MK(0, 3, 255),
	MK(1, 3, 255),
	MK(2, 3, 255),
	MK(2, 2, 255),
	MK(2, 1, 255),
};

static const std::vector<IndustryTileLayout> _tile_table_oil_rig {
	_tile_table_oil_rig_0,
};

static const IndustryTileLayout _tile_table_factory_0 {
	MK(0, 0, 39),
	MK(0, 1, 40),
	MK(1, 0, 41),
	MK(1, 1, 42),
	MK(0, 2, 39),
	MK(0, 3, 40),
	MK(1, 2, 41),
	MK(1, 3, 42),
	MK(2, 1, 39),
	MK(2, 2, 40),
	MK(3, 1, 41),
	MK(3, 2, 42),
};

static const IndustryTileLayout _tile_table_factory_1 {
	MK(0, 0, 39),
	MK(0, 1, 40),
	MK(1, 0, 41),
	MK(1, 1, 42),
	MK(2, 0, 39),
	MK(2, 1, 40),
	MK(3, 0, 41),
	MK(3, 1, 42),
	MK(1, 2, 39),
	MK(1, 3, 40),
	MK(2, 2, 41),
	MK(2, 3, 42),
};

static const std::vector<IndustryTileLayout> _tile_table_factory {
	_tile_table_factory_0,
	_tile_table_factory_1,
};

static const IndustryTileLayout _tile_table_printing_works_0 {
	MK(0, 0, 43),
	MK(0, 1, 44),
	MK(1, 0, 45),
	MK(1, 1, 46),
	MK(0, 2, 43),
	MK(0, 3, 44),
	MK(1, 2, 45),
	MK(1, 3, 46),
	MK(2, 1, 43),
	MK(2, 2, 44),
	MK(3, 1, 45),
	MK(3, 2, 46),
};

static const IndustryTileLayout _tile_table_printing_works_1 {
	MK(0, 0, 43),
	MK(0, 1, 44),
	MK(1, 0, 45),
	MK(1, 1, 46),
	MK(2, 0, 43),
	MK(2, 1, 44),
	MK(3, 0, 45),
	MK(3, 1, 46),
	MK(1, 2, 43),
	MK(1, 3, 44),
	MK(2, 2, 45),
	MK(2, 3, 46),
};

static const std::vector<IndustryTileLayout> _tile_table_printing_works {
	_tile_table_printing_works_0,
	_tile_table_printing_works_1,
};

static const IndustryTileLayout _tile_table_steel_mill_0 {
	MK(2, 1, 52),
	MK(2, 2, 53),
	MK(3, 1, 54),
	MK(3, 2, 55),
	MK(0, 0, 56),
	MK(1, 0, 57),
	MK(0, 1, 56),
	MK(1, 1, 57),
	MK(0, 2, 56),
	MK(1, 2, 57),
	MK(2, 0, 56),
	MK(3, 0, 57),
};

static const IndustryTileLayout _tile_table_steel_mill_1 {
	MK(0, 0, 52),
	MK(0, 1, 53),
	MK(1, 0, 54),
	MK(1, 1, 55),
	MK(2, 0, 52),
	MK(2, 1, 53),
	MK(3, 0, 54),
	MK(3, 1, 55),
	MK(0, 2, 56),
	MK(1, 2, 57),
	MK(2, 2, 56),
	MK(3, 2, 57),
	MK(1, 3, 56),
	MK(2, 3, 57),
};

static const std::vector<IndustryTileLayout> _tile_table_steel_mill {
	_tile_table_steel_mill_0,
	_tile_table_steel_mill_1,
};

static const IndustryTileLayout _tile_table_farm_0 {
	MK(1, 0, 33),
	MK(1, 1, 34),
	MK(1, 2, 36),
	MK(0, 0, 37),
	MK(0, 1, 37),
	MK(0, 2, 36),
	MK(2, 0, 35),
	MK(2, 1, 38),
	MK(2, 2, 38),
};

static const IndustryTileLayout _tile_table_farm_1 {
	MK(1, 1, 33),
	MK(1, 2, 34),
	MK(0, 0, 35),
	MK(0, 1, 36),
	MK(0, 2, 36),
	MK(0, 3, 35),
	MK(1, 0, 37),
	MK(1, 3, 38),
	MK(2, 0, 37),
	MK(2, 1, 37),
	MK(2, 2, 38),
	MK(2, 3, 38),
};

static const IndustryTileLayout _tile_table_farm_2 {
	MK(2, 0, 33),
	MK(2, 1, 34),
	MK(0, 0, 36),
	MK(0, 1, 36),
	MK(0, 2, 37),
	MK(0, 3, 37),
	MK(1, 0, 35),
	MK(1, 1, 38),
	MK(1, 2, 38),
	MK(1, 3, 37),
	MK(2, 2, 37),
	MK(2, 3, 35),
};

static const std::vector<IndustryTileLayout> _tile_table_farm {
	_tile_table_farm_0,
	_tile_table_farm_1,
	_tile_table_farm_2,
};

static const IndustryTileLayout _tile_table_copper_mine_0 {
	MK(0, 0, 47),
	MK(0, 1, 49),
	MK(0, 2, 51),
	MK(1, 0, 47),
	MK(1, 1, 49),
	MK(1, 2, 50),
	MK(2, 0, 51),
	MK(2, 1, 51),
};

static const IndustryTileLayout _tile_table_copper_mine_1 {
	MK(0, 0, 50),
	MK(0, 1, 47),
	MK(0, 2, 49),
	MK(1, 0, 47),
	MK(1, 1, 49),
	MK(1, 2, 51),
	MK(2, 0, 51),
	MK(2, 1, 47),
	MK(2, 2, 49),
};

static const std::vector<IndustryTileLayout> _tile_table_copper_mine {
	_tile_table_copper_mine_0,
	_tile_table_copper_mine_1,
};

static const IndustryTileLayout _tile_table_oil_well_0 {
	MK(0, 0, 29),
	MK(1, 0, 29),
	MK(2, 0, 29),
	MK(0, 1, 29),
	MK(0, 2, 29),
};

static const IndustryTileLayout _tile_table_oil_well_1 {
	MK(0, 0, 29),
	MK(1, 0, 29),
	MK(1, 1, 29),
	MK(2, 2, 29),
	MK(2, 3, 29),
};

static const std::vector<IndustryTileLayout> _tile_table_oil_well {
	_tile_table_oil_well_0,
	_tile_table_oil_well_1,
};

static const IndustryTileLayout _tile_table_bank_0 {
	MK(0, 0, 58),
	MK(1, 0, 59),
};

static const std::vector<IndustryTileLayout> _tile_table_bank {
	_tile_table_bank_0,
};

static const IndustryTileLayout _tile_table_food_process_0 {
	MK(0, 0, 60),
	MK(1, 0, 60),
	MK(2, 0, 60),
	MK(0, 1, 60),
	MK(1, 1, 60),
	MK(2, 1, 60),
	MK(0, 2, 61),
	MK(1, 2, 61),
	MK(2, 2, 63),
	MK(0, 3, 62),
	MK(1, 3, 62),
	MK(2, 3, 63),
};

static const IndustryTileLayout _tile_table_food_process_1 {
	MK(0, 0, 61),
	MK(1, 0, 60),
	MK(2, 0, 61),
	MK(3, 0, 61),
	MK(0, 1, 62),
	MK(1, 1, 63),
	MK(2, 1, 63),
	MK(3, 1, 63),
	MK(0, 2, 60),
	MK(1, 2, 60),
	MK(2, 2, 60),
	MK(3, 2, 60),
	MK(0, 3, 62),
	MK(1, 3, 62),
};

static const std::vector<IndustryTileLayout> _tile_table_food_process {
	_tile_table_food_process_0,
	_tile_table_food_process_1,
};

static const IndustryTileLayout _tile_table_paper_mill_0 {
	MK(0, 0, 64),
	MK(1, 0, 65),
	MK(2, 0, 66),
	MK(3, 0, 67),
	MK(0, 1, 68),
	MK(1, 1, 69),
	MK(2, 1, 67),
	MK(3, 1, 67),
	MK(0, 2, 66),
	MK(1, 2, 71),
	MK(2, 2, 71),
	MK(3, 2, 70),
};

static const std::vector<IndustryTileLayout> _tile_table_paper_mill {
	_tile_table_paper_mill_0,
};

static const IndustryTileLayout _tile_table_gold_mine_0 {
	MK(0, 0, 72),
	MK(0, 1, 73),
	MK(0, 2, 74),
	MK(0, 3, 75),
	MK(1, 0, 76),
	MK(1, 1, 77),
	MK(1, 2, 78),
	MK(1, 3, 79),
	MK(2, 0, 80),
	MK(2, 1, 81),
	MK(2, 2, 82),
	MK(2, 3, 83),
	MK(3, 0, 84),
	MK(3, 1, 85),
	MK(3, 2, 86),
	MK(3, 3, 87),
};

static const std::vector<IndustryTileLayout> _tile_table_gold_mine {
	_tile_table_gold_mine_0,
};

static const IndustryTileLayout _tile_table_bank2_0 {
	MK(0, 0, 89),
	MK(1, 0, 90),
};

static const std::vector<IndustryTileLayout> _tile_table_bank2 {
	_tile_table_bank2_0,
};

static const IndustryTileLayout _tile_table_diamond_mine_0 {
	MK(0, 0, 91),
	MK(0, 1, 92),
	MK(0, 2, 93),
	MK(1, 0, 94),
	MK(1, 1, 95),
	MK(1, 2, 96),
	MK(2, 0, 97),
	MK(2, 1, 98),
	MK(2, 2, 99),
};

static const std::vector<IndustryTileLayout> _tile_table_diamond_mine {
	_tile_table_diamond_mine_0,
};

static const IndustryTileLayout _tile_table_iron_mine_0 {
	MK(0, 0, 100),
	MK(0, 1, 101),
	MK(0, 2, 102),
	MK(0, 3, 103),
	MK(1, 0, 104),
	MK(1, 1, 105),
	MK(1, 2, 106),
	MK(1, 3, 107),
	MK(2, 0, 108),
	MK(2, 1, 109),
	MK(2, 2, 110),
	MK(2, 3, 111),
	MK(3, 0, 112),
	MK(3, 1, 113),
	MK(3, 2, 114),
	MK(3, 3, 115),
};

static const std::vector<IndustryTileLayout> _tile_table_iron_mine {
	_tile_table_iron_mine_0,
};

static const IndustryTileLayout _tile_table_fruit_plantation_0 {
	MK(0, 0, 116),
	MK(0, 1, 116),
	MK(0, 2, 116),
	MK(0, 3, 116),
	MK(1, 0, 116),
	MK(1, 1, 116),
	MK(1, 2, 116),
	MK(1, 3, 116),
	MK(2, 0, 116),
	MK(2, 1, 116),
	MK(2, 2, 116),
	MK(2, 3, 116),
	MK(3, 0, 116),
	MK(3, 1, 116),
	MK(3, 2, 116),
	MK(3, 3, 116),
	MK(4, 0, 116),
	MK(4, 1, 116),
	MK(4, 2, 116),
	MK(4, 3, 116),
};

static const std::vector<IndustryTileLayout> _tile_table_fruit_plantation {
	_tile_table_fruit_plantation_0,
};

static const IndustryTileLayout _tile_table_rubber_plantation_0 {
	MK(0, 0, 117),
	MK(0, 1, 117),
	MK(0, 2, 117),
	MK(0, 3, 117),
	MK(1, 0, 117),
	MK(1, 1, 117),
	MK(1, 2, 117),
	MK(1, 3, 117),
	MK(2, 0, 117),
	MK(2, 1, 117),
	MK(2, 2, 117),
	MK(2, 3, 117),
	MK(3, 0, 117),
	MK(3, 1, 117),
	MK(3, 2, 117),
	MK(3, 3, 117),
	MK(4, 0, 117),
	MK(4, 1, 117),
	MK(4, 2, 117),
	MK(4, 3, 117),
};

static const std::vector<IndustryTileLayout> _tile_table_rubber_plantation {
	_tile_table_rubber_plantation_0,
};

static const IndustryTileLayout _tile_table_water_supply_0 {
	MK(0, 0, 118),
	MK(0, 1, 119),
	MK(1, 0, 118),
	MK(1, 1, 119),
};

static const std::vector<IndustryTileLayout> _tile_table_water_supply {
	_tile_table_water_supply_0,
};

static const IndustryTileLayout _tile_table_water_tower_0 {
	MK(0, 0, 120),
};

static const std::vector<IndustryTileLayout> _tile_table_water_tower {
	_tile_table_water_tower_0,
};

static const IndustryTileLayout _tile_table_factory2_0 {
	MK(0, 0, 121),
	MK(0, 1, 122),
	MK(1, 0, 123),
	MK(1, 1, 124),
	MK(0, 2, 121),
	MK(0, 3, 122),
	MK(1, 2, 123),
	MK(1, 3, 124),
};

static const IndustryTileLayout _tile_table_factory2_1 {
	MK(0, 0, 121),
	MK(0, 1, 122),
	MK(1, 0, 123),
	MK(1, 1, 124),
	MK(2, 0, 121),
	MK(2, 1, 122),
	MK(3, 0, 123),
	MK(3, 1, 124),
};

static const std::vector<IndustryTileLayout> _tile_table_factory2 {
	_tile_table_factory2_0,
	_tile_table_factory2_1,
};

static const IndustryTileLayout _tile_table_farm2_0 {
	MK(1, 0, 33),
	MK(1, 1, 34),
	MK(1, 2, 36),
	MK(0, 0, 37),
	MK(0, 1, 37),
	MK(0, 2, 36),
	MK(2, 0, 35),
	MK(2, 1, 38),
	MK(2, 2, 38),
};

static const IndustryTileLayout _tile_table_farm2_1 {
	MK(1, 1, 33),
	MK(1, 2, 34),
	MK(0, 0, 35),
	MK(0, 1, 36),
	MK(0, 2, 36),
	MK(0, 3, 35),
	MK(1, 0, 37),
	MK(1, 3, 38),
	MK(2, 0, 37),
	MK(2, 1, 37),
	MK(2, 2, 38),
	MK(2, 3, 38),
};

static const IndustryTileLayout _tile_table_farm2_2 {
	MK(2, 0, 33),
	MK(2, 1, 34),
	MK(0, 0, 36),
	MK(0, 1, 36),
	MK(0, 2, 37),
	MK(0, 3, 37),
	MK(1, 0, 35),
	MK(1, 1, 38),
	MK(1, 2, 38),
	MK(1, 3, 37),
	MK(2, 2, 37),
	MK(2, 3, 35),
};

static const std::vector<IndustryTileLayout> _tile_table_farm2 {
	_tile_table_farm2_0,
	_tile_table_farm2_1,
	_tile_table_farm2_2,
};

static const IndustryTileLayout _tile_table_lumber_mill_0 {
	MK(0, 0, 125),
	MK(0, 1, 126),
	MK(1, 0, 127),
	MK(1, 1, 128),
};

static const std::vector<IndustryTileLayout> _tile_table_lumber_mill {
	_tile_table_lumber_mill_0,
};

static const IndustryTileLayout _tile_table_cotton_candy_0 {
	MK(0, 0, 129),
	MK(0, 1, 129),
	MK(0, 2, 129),
	MK(0, 3, 129),
	MK(1, 0, 129),
	MK(1, 1, 129),
	MK(1, 2, 129),
	MK(1, 3, 129),
	MK(2, 0, 129),
	MK(2, 1, 129),
	MK(2, 2, 129),
	MK(2, 3, 129),
	MK(3, 0, 129),
	MK(3, 1, 129),
	MK(3, 2, 129),
	MK(3, 3, 129),
	MK(1, 4, 129),
	MK(2, 4, 129),
};

static const IndustryTileLayout _tile_table_cotton_candy_1 {
	MK(0, 0, 129),
	MK(1, 0, 129),
	MK(2, 0, 129),
	MK(3, 0, 129),
	MK(4, 0, 129),
	MK(0, 1, 129),
	MK(1, 1, 129),
	MK(2, 1, 129),
	MK(3, 1, 129),
	MK(4, 1, 129),
	MK(0, 2, 129),
	MK(1, 2, 129),
	MK(2, 2, 129),
	MK(3, 2, 129),
	MK(4, 2, 129),
	MK(0, 3, 129),
	MK(1, 3, 129),
	MK(2, 3, 129),
	MK(3, 3, 129),
	MK(4, 3, 129),
	MK(1, 4, 129),
	MK(2, 4, 129),
	MK(3, 4, 129),
};

static const std::vector<IndustryTileLayout> _tile_table_cotton_candy {
	_tile_table_cotton_candy_0,
	_tile_table_cotton_candy_1,
};

static const IndustryTileLayout _tile_table_candy_factory_0 {
	MK(0, 0, 131),
	MK(0, 1, 132),
	MK(1, 0, 133),
	MK(1, 1, 134),
	MK(0, 2, 131),
	MK(0, 3, 132),
	MK(1, 2, 133),
	MK(1, 3, 134),
	MK(2, 1, 131),
	MK(2, 2, 132),
	MK(3, 1, 133),
	MK(3, 2, 134),
};

static const IndustryTileLayout _tile_table_candy_factory_1 {
	MK(0, 0, 131),
	MK(0, 1, 132),
	MK(1, 0, 133),
	MK(1, 1, 134),
	MK(2, 0, 131),
	MK(2, 1, 132),
	MK(3, 0, 133),
	MK(3, 1, 134),
	MK(1, 2, 131),
	MK(1, 3, 132),
	MK(2, 2, 133),
	MK(2, 3, 134),
};

static const std::vector<IndustryTileLayout> _tile_table_candy_factory {
	_tile_table_candy_factory_0,
	_tile_table_candy_factory_1,
};

static const IndustryTileLayout _tile_table_battery_farm_0 {
	MK(0, 0, 135),
	MK(0, 1, 135),
	MK(0, 2, 135),
	MK(0, 3, 135),
	MK(1, 0, 135),
	MK(1, 1, 135),
	MK(1, 2, 135),
	MK(1, 3, 135),
	MK(2, 0, 135),
	MK(2, 1, 135),
	MK(2, 2, 135),
	MK(2, 3, 135),
	MK(3, 0, 135),
	MK(3, 1, 135),
	MK(3, 2, 135),
	MK(3, 3, 135),
	MK(4, 0, 135),
	MK(4, 1, 135),
	MK(4, 2, 135),
	MK(4, 3, 135),
};

static const std::vector<IndustryTileLayout> _tile_table_battery_farm {
	_tile_table_battery_farm_0,
};

static const IndustryTileLayout _tile_table_cola_wells_0 {
	MK(0, 0, 137),
	MK(0, 1, 137),
	MK(0, 2, 137),
	MK(1, 0, 137),
	MK(1, 1, 137),
	MK(1, 2, 137),
	MK(2, 1, 137),
	MK(2, 2, 137),
};

static const IndustryTileLayout _tile_table_cola_wells_1 {
	MK(0, 1, 137),
	MK(0, 2, 137),
	MK(0, 3, 137),
	MK(1, 0, 137),
	MK(1, 1, 137),
	MK(1, 2, 137),
	MK(2, 1, 137),
};

static const std::vector<IndustryTileLayout> _tile_table_cola_wells {
	_tile_table_cola_wells_0,
	_tile_table_cola_wells_1,
};

static const IndustryTileLayout _tile_table_toy_shop_0 {
	MK(0, 0, 138),
	MK(0, 1, 139),
	MK(1, 0, 140),
	MK(1, 1, 141),
};

static const std::vector<IndustryTileLayout> _tile_table_toy_shop {
	_tile_table_toy_shop_0,
};

static const IndustryTileLayout _tile_table_toy_factory_0 {
	MK(0, 0, 147),
	MK(0, 1, 142),
	MK(1, 0, 147),
	MK(1, 1, 143),
	MK(2, 0, 147),
	MK(2, 1, 144),
	MK(3, 0, 146),
	MK(3, 1, 145),
};

static const std::vector<IndustryTileLayout> _tile_table_toy_factory {
	_tile_table_toy_factory_0,
};

static const IndustryTileLayout _tile_table_plastic_fountain_0 {
	MK(0, 0, 148),
	MK(0, 1, 151),
	MK(0, 2, 154),
};

static const IndustryTileLayout _tile_table_plastic_fountain_1 {
	MK(0, 0, 148),
	MK(1, 0, 151),
	MK(2, 0, 154),
};

static const std::vector<IndustryTileLayout> _tile_table_plastic_fountain {
	_tile_table_plastic_fountain_0,
	_tile_table_plastic_fountain_1,
};

static const IndustryTileLayout _tile_table_fizzy_drink_0 {
	MK(0, 0, 156),
	MK(0, 1, 157),
	MK(1, 0, 158),
	MK(1, 1, 159),
};

static const std::vector<IndustryTileLayout> _tile_table_fizzy_drink {
	_tile_table_fizzy_drink_0,
};

static const IndustryTileLayout _tile_table_bubble_generator_0 {
	MK(0, 0, 163),
	MK(0, 1, 160),
	MK(1, 0, 163),
	MK(1, 1, 161),
	MK(2, 0, 163),
	MK(2, 1, 162),
	MK(0, 2, 163),
	MK(0, 3, 160),
	MK(1, 2, 163),
	MK(1, 3, 161),
	MK(2, 2, 163),
	MK(2, 3, 162),
};

static const std::vector<IndustryTileLayout> _tile_table_bubble_generator {
	_tile_table_bubble_generator_0,
};

static const IndustryTileLayout _tile_table_toffee_quarry_0 {
	MK(0, 0, 164),
	MK(1, 0, 165),
	MK(2, 0, 166),
};

static const std::vector<IndustryTileLayout> _tile_table_toffee_quarry {
	_tile_table_toffee_quarry_0,
};

static const IndustryTileLayout _tile_table_sugar_mine_0 {
	MK(0, 0, 167),
	MK(0, 1, 168),
	MK(1, 0, 169),
	MK(1, 1, 170),
	MK(2, 0, 171),
	MK(2, 1, 172),
	MK(3, 0, 173),
	MK(3, 1, 174),
};

static const std::vector<IndustryTileLayout> _tile_table_sugar_mine {
	_tile_table_sugar_mine_0,
};

#undef MK

/** Array with saw sound, for sawmill */
static const uint8_t _sawmill_sounds[] = { SND_28_SAWMILL };

/** Array with whistle sound, for factory */
static const uint8_t _factory_sounds[] = { SND_03_FACTORY };

/** Array with 3 animal sounds, for farms */
static const uint8_t _farm_sounds[] = { SND_24_FARM_1, SND_25_FARM_2, SND_26_FARM_3 };

/** Array with... hem... a sound of toyland */
static const uint8_t _plastic_mine_sounds[] = { SND_33_PLASTIC_MINE };

enum IndustryTypes {
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
 * @param sndc number of sounds
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

#define MI(tbl, sndc, snd, d, pc, ai1, ai2, ai3, ai4, ag1, ag2, ag3, ag4, col, \
			c1, c2, c3, proc, p1, r1, p2, r2, m, a1, im1, a2, im2, a3, im3, pr, clim, bev, in, intx, s1, s2, s3) \
		{tbl, d, 0, pc, {c1, c2, c3}, proc, \
		{p1, p2, CT_INVALID, CT_INVALID, CT_INVALID, CT_INVALID, CT_INVALID, CT_INVALID, CT_INVALID, CT_INVALID, CT_INVALID, CT_INVALID, CT_INVALID, CT_INVALID, CT_INVALID, CT_INVALID}, \
		{r1, r2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, m, \
		{a1, a2, a3, CT_INVALID, CT_INVALID, CT_INVALID, CT_INVALID, CT_INVALID, CT_INVALID, CT_INVALID, CT_INVALID, CT_INVALID, CT_INVALID, CT_INVALID, CT_INVALID, CT_INVALID}, \
		{{im1, 0}, {im2, 0}, {im3, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}}, \
		pr, clim, bev, col, in, intx, s1, s2, s3, STR_UNDEFINED, {ai1, ai2, ai3, ai4}, {ag1, ag2, ag3, ag4}, \
		sndc, snd, 0, 0, true, GRFFileProps(INVALID_INDUSTRYTYPE)}
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
static const IndustrySpec _origin_industry_specs[NEW_INDUSTRYOFFSET] = {
	MI(_tile_table_coal_mine,                  0, nullptr,
	   210,  0xB3333333,                       2, 3, 0, 0,    8, 8, 0, 0,          1,
	   IT_POWER_STATION,  IT_INVALID,          IT_INVALID,       CHECK_NOTHING,
	   CT_COAL,       15, CT_INVALID,       0, 5,
	   CT_INVALID,   256, CT_INVALID,     256, CT_INVALID,   256,
	   INDUSTRYLIFE_EXTRACTIVE,                1 << LT_TEMPERATE | 1 << LT_ARCTIC,
	   INDUSTRYBEH_CAN_SUBSIDENCE,
	   STR_INDUSTRY_NAME_COAL_MINE,                     STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_GENERAL,    STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_COAL,   STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_power_station,              0, nullptr,
	   240,  0xFFFFFFFF,                       2, 2, 0, 0,    5, 5, 0, 0,        184,
	   IT_COAL_MINE,      IT_INVALID,          IT_INVALID,       CHECK_NOTHING,
	   CT_INVALID,     0, CT_INVALID,       0, 5,
	   CT_COAL,      256, CT_INVALID,     256, CT_INVALID,   256,
	   INDUSTRYLIFE_BLACK_HOLE,                1 << LT_TEMPERATE | 1 << LT_ARCTIC,
	   INDUSTRYBEH_NONE,
	   STR_INDUSTRY_NAME_POWER_STATION,                 STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_GENERAL,    STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_sawmill,                    1, _sawmill_sounds,
	   224,  0xFFFFFFFF,                       2, 0, 0, 0,    5, 0, 0, 0,        194,
	   IT_FOREST,         IT_INVALID,          IT_INVALID,       CHECK_NOTHING,
	   CT_GOODS,       0, CT_INVALID,       0, 5,
	   CT_WOOD,      256, CT_INVALID,     256, CT_INVALID,   256,
	   INDUSTRYLIFE_PROCESSING,                1 << LT_TEMPERATE,
	   INDUSTRYBEH_NONE,
	   STR_INDUSTRY_NAME_SAWMILL,                       STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_SUPPLY_PROBLEMS,      STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_forest,                     0, nullptr,
	   200,  0xBFFFFFFF,                       3, 4, 0, 0,    5, 5, 0, 0,         86,
	   IT_SAWMILL,        IT_PAPER_MILL,       IT_INVALID,       CHECK_FOREST,
	   CT_WOOD,       13, CT_INVALID,       0, 30,
	   CT_INVALID,   256, CT_INVALID,     256, CT_INVALID,   256,
	   INDUSTRYLIFE_ORGANIC,                   1 << LT_TEMPERATE | 1 << LT_ARCTIC,
	   INDUSTRYBEH_NONE,
	   STR_INDUSTRY_NAME_FOREST,                        STR_NEWS_INDUSTRY_PLANTED,
	   STR_NEWS_INDUSTRY_CLOSURE_GENERAL,    STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_FARM),

	MI(_tile_table_oil_refinery,               0, nullptr,
	   244,  0xFFFFFFFF,                       2, 2, 2, 0,    4, 4, 4, 0,        191,
	   IT_OIL_RIG,        IT_INVALID,          IT_INVALID,       CHECK_REFINERY,
	   CT_GOODS,       0, CT_INVALID,       0, 5,
	   CT_OIL,       256, CT_INVALID,     256, CT_INVALID,   256,
	   INDUSTRYLIFE_PROCESSING,                1 << LT_TEMPERATE | 1 << LT_ARCTIC | 1 << LT_TROPIC,
	   INDUSTRYBEH_AIRPLANE_ATTACKS,
	   STR_INDUSTRY_NAME_OIL_REFINERY,                  STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_SUPPLY_PROBLEMS,      STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_oil_rig,                    0, nullptr,
	   240,  0x99999999,                       6, 0, 0, 0,    0, 0, 0, 0,        152,
	   IT_OIL_REFINERY,   IT_INVALID,          IT_INVALID,       CHECK_OIL_RIG,
	   CT_OIL,        15, CT_PASSENGERS,    2, 5,
	   CT_INVALID,     0, CT_INVALID,       0, CT_INVALID,     0,
	   INDUSTRYLIFE_EXTRACTIVE,                1 << LT_TEMPERATE,
	   INDUSTRYBEH_BUILT_ONWATER | INDUSTRYBEH_AFTER_1960 | INDUSTRYBEH_AI_AIRSHIP_ROUTES,
	   STR_INDUSTRY_NAME_OIL_RIG,                       STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_GENERAL,    STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_OIL,   STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_factory,                    1, _factory_sounds,
	   208,  0xFFFFFFFF,                       2, 0, 0, 0,    5, 0, 0, 0,        174,
	   IT_FARM,           IT_STEEL_MILL,       IT_INVALID,       CHECK_NOTHING,
	   CT_GOODS,       0, CT_INVALID,       0, 5,
	   CT_LIVESTOCK, 256, CT_GRAIN,       256, CT_STEEL,    256,
	   INDUSTRYLIFE_PROCESSING,                1 << LT_TEMPERATE,
	   INDUSTRYBEH_CHOPPER_ATTACKS,
	   STR_INDUSTRY_NAME_FACTORY,                       STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_SUPPLY_PROBLEMS,      STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_printing_works,             1, _factory_sounds,
	   208,  0xFFFFFFFF,                       0, 2, 0, 0,    0, 5, 0, 0,        174,
	   IT_PAPER_MILL,     IT_INVALID,          IT_INVALID,       CHECK_NOTHING,
	   CT_GOODS,       0, CT_INVALID,       0, 5,
	   CT_PAPER,     256, CT_INVALID,     256, CT_INVALID,   256,
	   INDUSTRYLIFE_PROCESSING,                1 << LT_ARCTIC,
	   INDUSTRYBEH_NONE,
	   STR_INDUSTRY_NAME_PRINTING_WORKS,                STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_SUPPLY_PROBLEMS,      STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_steel_mill,                 0, nullptr,
	   215,  0xFFFFFFFF,                       2, 0, 0, 0,    5, 0, 0, 0,         10,
	   IT_IRON_MINE,      IT_FACTORY,          IT_INVALID,       CHECK_NOTHING,
	   CT_STEEL,       0, CT_INVALID,       0, 5,
	   CT_IRON_ORE,  256, CT_INVALID,     256, CT_INVALID,   256,
	   INDUSTRYLIFE_PROCESSING,                1 << LT_TEMPERATE,
	   INDUSTRYBEH_NONE,
	   STR_INDUSTRY_NAME_STEEL_MILL,                    STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_SUPPLY_PROBLEMS,      STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_farm,                       3, _farm_sounds,
	   250,  0xD9999999,                       2, 4, 0, 0,    9, 9, 0, 0,         48,
	   IT_FACTORY,        IT_FOOD_PROCESS,     IT_INVALID,       CHECK_FARM,
	   CT_GRAIN,      10, CT_LIVESTOCK,    10, 5,
	   CT_INVALID,   256, CT_INVALID,     256, CT_INVALID,   256,
	   INDUSTRYLIFE_ORGANIC,                   1 << LT_TEMPERATE | 1 << LT_ARCTIC,
	   INDUSTRYBEH_PLANT_FIELDS | INDUSTRYBEH_PLANT_ON_BUILT,
	   STR_INDUSTRY_NAME_FARM,                          STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_GENERAL,    STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_FARM, STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_FARM),

	MI(_tile_table_copper_mine,                0, nullptr,
	   205,  0xB3333333,                       0, 0, 3, 0,    0, 0, 4, 0,         10,
	   IT_FACTORY_2,      IT_INVALID,          IT_INVALID,       CHECK_NOTHING,
	   CT_COPPER_ORE, 10, CT_INVALID,       0, 5,
	   CT_INVALID,   256, CT_INVALID,     256, CT_INVALID,   256,
	   INDUSTRYLIFE_EXTRACTIVE,                1 << LT_TROPIC,
	   INDUSTRYBEH_NONE,
	   STR_INDUSTRY_NAME_COPPER_ORE_MINE,               STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_GENERAL,    STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_oil_well,                   0, nullptr,
	   220,  0x99999999,                       0, 5, 3, 0,    4, 5, 5, 0,        152,
	   IT_OIL_REFINERY,   IT_INVALID,          IT_INVALID,       CHECK_NOTHING,
	   CT_OIL,        12, CT_INVALID,       0, 5,
	   CT_INVALID,   256, CT_INVALID,     256, CT_INVALID,   256,
	   INDUSTRYLIFE_EXTRACTIVE,                1 << LT_TEMPERATE | 1 << LT_ARCTIC | 1 << LT_TROPIC,
	   INDUSTRYBEH_DONT_INCR_PROD | INDUSTRYBEH_BEFORE_1950,
	   STR_INDUSTRY_NAME_OIL_WELLS,                     STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_GENERAL,    STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_OIL,   STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_bank,                       0, nullptr,
	   255,  0xA6666666,                       7, 0, 0, 0,    0, 0, 0, 0,         15,
	   IT_BANK_TEMP,      IT_INVALID,          IT_INVALID,       CHECK_NOTHING,
	   CT_VALUABLES,   6, CT_INVALID,       0, 5,
	   CT_VALUABLES,   0, CT_INVALID,       0, CT_INVALID,     0,
	   INDUSTRYLIFE_BLACK_HOLE,                1 << LT_TEMPERATE,
	   INDUSTRYBEH_TOWN1200_MORE,
	   STR_INDUSTRY_NAME_BANK,                          STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_GENERAL,    STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_food_process,               0, nullptr,
	   206,  0xFFFFFFFF,                       0, 2, 2, 0,    0, 3, 4, 0,         55,
	   IT_FRUIT_PLANTATION, IT_FARM,           IT_FARM_2,        CHECK_NOTHING,
	   CT_FOOD,        0, CT_INVALID,       0, 5,
	   CT_FRUIT,     256, CT_MAIZE,       256, CT_INVALID,   256,
	   INDUSTRYLIFE_PROCESSING,                1 << LT_ARCTIC | 1 << LT_TROPIC,
	   INDUSTRYBEH_NONE,
	   STR_INDUSTRY_NAME_FOOD_PROCESSING_PLANT,         STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_SUPPLY_PROBLEMS,      STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_paper_mill,                 1, _sawmill_sounds,
	   227,  0xFFFFFFFF,                       0, 2, 0, 0,    0, 5, 0, 0,         10,
	   IT_FOREST,         IT_PRINTING_WORKS,   IT_INVALID,       CHECK_NOTHING,
	   CT_PAPER,       0, CT_INVALID,       0, 5,
	   CT_WOOD,      256, CT_INVALID,     256, CT_INVALID,   256,
	   INDUSTRYLIFE_PROCESSING,                1 << LT_ARCTIC,
	   INDUSTRYBEH_NONE,
	   STR_INDUSTRY_NAME_PAPER_MILL,                    STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_SUPPLY_PROBLEMS,      STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_gold_mine,                  0, nullptr,
	   208,  0x99999999,                       0, 3, 0, 0,    0, 4, 0, 0,        194,
	   IT_BANK_TROPIC_ARCTIC, IT_INVALID,      IT_INVALID,       CHECK_NOTHING,
	   CT_GOLD,        7, CT_INVALID,       0, 5,
	   CT_INVALID,   256, CT_INVALID,     256, CT_INVALID,   256,
	   INDUSTRYLIFE_EXTRACTIVE,                1 << LT_ARCTIC,
	   INDUSTRYBEH_NONE,
	   STR_INDUSTRY_NAME_GOLD_MINE,                     STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_GENERAL,    STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_bank2,                      0, nullptr,
	   151,  0xA6666666,                       0, 3, 3, 0,    0, 6, 5, 0,         15,
	   IT_GOLD_MINE,      IT_DIAMOND_MINE,     IT_INVALID,       CHECK_NOTHING,
	   CT_INVALID,     0, CT_INVALID,       0, 5,
	   CT_GOLD,      256, CT_INVALID,     256, CT_INVALID,   256,
	   INDUSTRYLIFE_BLACK_HOLE,                1 << LT_ARCTIC | 1 << LT_TROPIC,
	   INDUSTRYBEH_ONLY_INTOWN,
	   STR_INDUSTRY_NAME_BANK_TROPIC_ARCTIC,                          STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_GENERAL,    STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_diamond_mine,               0, nullptr,
	   213,  0x99999999,                       0, 0, 3, 0,    0, 0, 4, 0,        184,
	   IT_BANK_TROPIC_ARCTIC, IT_INVALID,      IT_INVALID,       CHECK_NOTHING,
	   CT_DIAMONDS,    7, CT_INVALID,       0, 5,
	   CT_INVALID,   256, CT_INVALID,     256, CT_INVALID,   256,
	   INDUSTRYLIFE_EXTRACTIVE,                1 << LT_TROPIC,
	   INDUSTRYBEH_NONE,
	   STR_INDUSTRY_NAME_DIAMOND_MINE,                  STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_GENERAL,    STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_iron_mine,                  0, nullptr,
	   220,  0xB3333333,                       2, 0, 0, 0,    5, 0, 0, 0,         55,
	   IT_STEEL_MILL,     IT_INVALID,          IT_INVALID,       CHECK_NOTHING,
	   CT_IRON_ORE,   10, CT_INVALID,       0, 5,
	   CT_INVALID,   256, CT_INVALID,     256, CT_INVALID,   256,
	   INDUSTRYLIFE_EXTRACTIVE,                1 << LT_TEMPERATE,
	   INDUSTRYBEH_NONE,
	   STR_INDUSTRY_NAME_IRON_ORE_MINE,                 STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_GENERAL,    STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_fruit_plantation,           0, nullptr,
	   225,  0xBFFFFFFF,                       0, 0, 2, 0,    0, 0, 4, 0,         86,
	   IT_FOOD_PROCESS,   IT_INVALID,          IT_INVALID,       CHECK_PLANTATION,
	   CT_FRUIT,      10, CT_INVALID,       0, 15,
	   CT_INVALID,   256, CT_INVALID,     256, CT_INVALID,   256,
	   INDUSTRYLIFE_ORGANIC,                   1 << LT_TROPIC,
	   INDUSTRYBEH_NONE,
	   STR_INDUSTRY_NAME_FRUIT_PLANTATION,              STR_NEWS_INDUSTRY_PLANTED,
	   STR_NEWS_INDUSTRY_CLOSURE_GENERAL,    STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_FARM, STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_FARM),

	MI(_tile_table_rubber_plantation,          0, nullptr,
	   218,  0xBFFFFFFF,                       0, 0, 3, 0,    0, 0, 4, 0,         39,
	   IT_FACTORY_2,      IT_INVALID,          IT_INVALID,       CHECK_PLANTATION,
	   CT_RUBBER,     10, CT_INVALID,       0, 15,
	   CT_INVALID,   256, CT_INVALID,     256, CT_INVALID,   256,
	   INDUSTRYLIFE_ORGANIC,                   1 << LT_TROPIC,
	   INDUSTRYBEH_NONE,
	   STR_INDUSTRY_NAME_RUBBER_PLANTATION,             STR_NEWS_INDUSTRY_PLANTED,
	   STR_NEWS_INDUSTRY_CLOSURE_GENERAL,    STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_FARM, STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_FARM),

	MI(_tile_table_water_supply,               0, nullptr,
	   199,  0xB3333333,                       0, 0, 3, 0,    0, 0, 4, 0,         37,
	   IT_WATER_TOWER,    IT_INVALID,          IT_INVALID,       CHECK_WATER,
	   CT_WATER,      12, CT_INVALID,       0, 5,
	   CT_INVALID,   256, CT_INVALID,     256, CT_INVALID,   256,
	   INDUSTRYLIFE_EXTRACTIVE,                1 << LT_TROPIC,
	   INDUSTRYBEH_NONE,
	   STR_INDUSTRY_NAME_WATER_SUPPLY,                  STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_GENERAL,    STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_water_tower,                0, nullptr,
	   115,  0xFFFFFFFF,                       0, 0, 4, 0,    0, 0, 8, 0,        208,
	   IT_WATER_SUPPLY,   IT_INVALID,          IT_INVALID,       CHECK_WATER,
	   CT_INVALID,     0, CT_INVALID,       0, 5,
	   CT_WATER,     256, CT_INVALID,     256, CT_INVALID,   256,
	   INDUSTRYLIFE_BLACK_HOLE,                1 << LT_TROPIC,
	   INDUSTRYBEH_ONLY_INTOWN,
	   STR_INDUSTRY_NAME_WATER_TOWER,                   STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_GENERAL,    STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_factory2,                   1, _factory_sounds,
	   208,  0xFFFFFFFF,                       0, 0, 2, 0,    0, 0, 4, 0,        174,
	   IT_RUBBER_PLANTATION, IT_COPPER_MINE,   IT_LUMBER_MILL,   CHECK_PLANTATION,
	   CT_GOODS,       0, CT_INVALID,       0, 5,
	   CT_RUBBER,    256, CT_COPPER_ORE,  256, CT_WOOD,      256,
	   INDUSTRYLIFE_PROCESSING,                1 << LT_TROPIC,
	   INDUSTRYBEH_NONE,
	   STR_INDUSTRY_NAME_FACTORY_2,                       STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_SUPPLY_PROBLEMS,      STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_farm2,                      0, nullptr,
	   250,  0xD9999999,                       0, 0, 1, 0,    0, 0, 2, 0,         48,
	   IT_FOOD_PROCESS,   IT_INVALID,          IT_INVALID,       CHECK_PLANTATION,
	   CT_MAIZE,      11, CT_INVALID,       0, 5,
	   CT_INVALID,   256, CT_INVALID,     256, CT_INVALID,   256,
	   INDUSTRYLIFE_ORGANIC,                   1 << LT_TROPIC,
	   INDUSTRYBEH_PLANT_FIELDS | INDUSTRYBEH_PLANT_ON_BUILT,
	   STR_INDUSTRY_NAME_FARM_2,                          STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_GENERAL,    STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_FARM, STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_FARM),

	MI(_tile_table_lumber_mill,                0, nullptr,
	   135,  0xFFFFFFFF,                       0, 0, 0, 0,    0, 0, 0, 0,        194,
	   IT_FACTORY_2,      IT_INVALID,          IT_INVALID,       CHECK_LUMBERMILL,
	   CT_WOOD,        0, CT_INVALID,       0, 5,
	   CT_INVALID,   256, CT_INVALID,     256, CT_INVALID,   256,
	   INDUSTRYLIFE_PROCESSING,                1 << LT_TROPIC,
	   INDUSTRYBEH_CUT_TREES,
	   STR_INDUSTRY_NAME_LUMBER_MILL,                   STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_LACK_OF_TREES,   STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_cotton_candy,               0, nullptr,
	   195,  0xBFFFFFFF,                       0, 0, 0, 3,    0, 0, 0, 5,         48,
	   IT_CANDY_FACTORY,  IT_INVALID,          IT_INVALID,       CHECK_NOTHING,
	   CT_COTTON_CANDY, 13, CT_INVALID,    0, 30,
	   CT_INVALID,   256, CT_INVALID,     256, CT_INVALID,   256,
	   INDUSTRYLIFE_ORGANIC,                   1 << LT_TOYLAND,
	   INDUSTRYBEH_NONE,
	   STR_INDUSTRY_NAME_COTTON_CANDY_FOREST,           STR_NEWS_INDUSTRY_PLANTED,
	   STR_NEWS_INDUSTRY_CLOSURE_GENERAL,    STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_FARM, STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_candy_factory,              0, nullptr,
	   206,  0xFFFFFFFF,                       0, 0, 0, 3,    0, 0, 0, 5,        174,
	   IT_COTTON_CANDY,   IT_TOFFEE_QUARRY,    IT_SUGAR_MINE,    CHECK_NOTHING,
	   CT_CANDY,       0, CT_INVALID,       0, 5,
	   CT_SUGAR,     256, CT_TOFFEE,      256, CT_COTTON_CANDY, 256,
	   INDUSTRYLIFE_PROCESSING,                1 << LT_TOYLAND,
	   INDUSTRYBEH_NONE,
	   STR_INDUSTRY_NAME_CANDY_FACTORY,                 STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_SUPPLY_PROBLEMS,      STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_battery_farm,               0, nullptr,
	   187,  0xB3333333,                       0, 0, 0, 3,    0, 0, 0, 4,         39,
	   IT_TOY_FACTORY,    IT_INVALID,          IT_INVALID,       CHECK_NOTHING,
	   CT_BATTERIES,  11, CT_INVALID,       0, 30,
	   CT_INVALID,   256, CT_INVALID,     256, CT_INVALID,   256,
	   INDUSTRYLIFE_ORGANIC,                   1 << LT_TOYLAND,
	   INDUSTRYBEH_NONE,
	   STR_INDUSTRY_NAME_BATTERY_FARM,                  STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_GENERAL,    STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_FARM, STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_FARM),

	MI(_tile_table_cola_wells,                 0, nullptr,
	   193,  0x99999999,                       0, 0, 0, 3,    0, 0, 0, 5,         55,
	   IT_FIZZY_DRINK_FACTORY, IT_INVALID,     IT_INVALID,       CHECK_NOTHING,
	   CT_COLA,       12, CT_INVALID,       0, 5,
	   CT_INVALID,   256, CT_INVALID,     256, CT_INVALID,   256,
	   INDUSTRYLIFE_EXTRACTIVE,                1 << LT_TOYLAND,
	   INDUSTRYBEH_NONE,
	   STR_INDUSTRY_NAME_COLA_WELLS,                    STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_GENERAL,    STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_toy_shop,                   0, nullptr,
	   133,  0xFFFFFFFF,                       0, 0, 0, 3,    0, 0, 0, 4,        208,
	   IT_TOY_FACTORY,    IT_INVALID,          IT_INVALID,       CHECK_NOTHING,
	   CT_INVALID,     0, CT_INVALID,       0, 5,
	   CT_TOYS,      256, CT_INVALID,     256, CT_INVALID,   256,
	   INDUSTRYLIFE_BLACK_HOLE,                1 << LT_TOYLAND,
	   INDUSTRYBEH_ONLY_NEARTOWN,
	   STR_INDUSTRY_NAME_TOY_SHOP,                      STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_SUPPLY_PROBLEMS,      STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_toy_factory,                0, nullptr,
	   163,  0xFFFFFFFF,                       0, 0, 0, 3,    0, 0, 0, 5,          10,
	   IT_PLASTIC_FOUNTAINS, IT_BATTERY_FARM,  IT_TOY_SHOP,     CHECK_NOTHING,
	   CT_TOYS,        0, CT_INVALID,       0, 5,
	   CT_PLASTIC,   256, CT_BATTERIES,   256, CT_INVALID,   256,
	   INDUSTRYLIFE_PROCESSING,                1 << LT_TOYLAND,
	   INDUSTRYBEH_NONE,
	   STR_INDUSTRY_NAME_TOY_FACTORY,                   STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_SUPPLY_PROBLEMS,      STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_plastic_fountain,           1, _plastic_mine_sounds,
	   192,  0xA6666666,                       0, 0, 0, 3,    0, 0, 0, 5,         37,
	   IT_TOY_FACTORY,    IT_INVALID,          IT_INVALID,       CHECK_NOTHING,
	   CT_PLASTIC,    14, CT_INVALID,       0, 5,
	   CT_INVALID,   256, CT_INVALID,     256, CT_INVALID,   256,
	   INDUSTRYLIFE_EXTRACTIVE,                1 << LT_TOYLAND,
	   INDUSTRYBEH_NONE,
	   STR_INDUSTRY_NAME_PLASTIC_FOUNTAINS,             STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_GENERAL,    STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_fizzy_drink,                0, nullptr,
	   177,  0xFFFFFFFF,                       0, 0, 0, 3,    0, 0, 0, 4,        184,
	   IT_COLA_WELLS,     IT_BUBBLE_GENERATOR, IT_INVALID,       CHECK_NOTHING,
	   CT_FIZZY_DRINKS, 0, CT_INVALID,      0, 5,
	   CT_COLA,       256, CT_BUBBLES,    256, CT_INVALID,   256,
	   INDUSTRYLIFE_PROCESSING,                1 << LT_TOYLAND,
	   INDUSTRYBEH_NONE,
	   STR_INDUSTRY_NAME_FIZZY_DRINK_FACTORY,           STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_SUPPLY_PROBLEMS,      STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_bubble_generator,           0, nullptr,
	   203,  0xB3333333,                       0, 0, 0, 3,    0, 0, 0, 5,        152,
	   IT_FIZZY_DRINK_FACTORY, IT_INVALID,     IT_INVALID,       CHECK_BUBBLEGEN,
	   CT_BUBBLES,    13, CT_INVALID,       0, 5,
	   CT_INVALID,   256, CT_INVALID,     256, CT_INVALID,   256,
	   INDUSTRYLIFE_EXTRACTIVE,                1 << LT_TOYLAND,
	   INDUSTRYBEH_NONE,
	   STR_INDUSTRY_NAME_BUBBLE_GENERATOR,              STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_GENERAL,    STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_toffee_quarry,              0, nullptr,
	   213,  0xCCCCCCCC,                       0, 0, 0, 3,    0, 0, 0, 5,        194,
	   IT_CANDY_FACTORY,  IT_INVALID,          IT_INVALID,       CHECK_NOTHING,
	   CT_TOFFEE,     10, CT_INVALID,       0, 5,
	   CT_INVALID,   256, CT_INVALID,     256, CT_INVALID,   256,
	   INDUSTRYLIFE_EXTRACTIVE,                1 << LT_TOYLAND,
	   INDUSTRYBEH_NONE,
	   STR_INDUSTRY_NAME_TOFFEE_QUARRY,                 STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_GENERAL,    STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),

	MI(_tile_table_sugar_mine,                 0, nullptr,
	   210,  0xBFFFFFFF,                       0, 0, 0, 2,    0, 0, 0, 4,         15,
	   IT_CANDY_FACTORY,  IT_INVALID,          IT_INVALID,       CHECK_NOTHING,
	   CT_SUGAR,      11, CT_INVALID,       0, 5,
	   CT_INVALID,   256, CT_INVALID,     256, CT_INVALID,   256,
	   INDUSTRYLIFE_EXTRACTIVE,                1 << LT_TOYLAND,
	   INDUSTRYBEH_NONE,
	   STR_INDUSTRY_NAME_SUGAR_MINE,                    STR_NEWS_INDUSTRY_CONSTRUCTION,
	   STR_NEWS_INDUSTRY_CLOSURE_GENERAL,    STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL,     STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL),
};
#undef MI

/**
 * Writes the properties of an industry tile into the IndustryTileSpec struct.
 * @param ca1 acceptance of first cargo
 * @param c1  first type of cargo accepted for this tile
 * @param ca2 acceptance of second cargo
 * @param c2  second cargo
 * @param ca3 acceptance of third cargo
 * @param c3  and third cargo. Those three are in an array
 * @param sl  slope refused upon choosing a place to build
 * @param a1  animation frame on production
 * @param a2  next frame of animation
 * @param a3  chooses between animation or construction state
 */
#define MT(ca1, c1, ca2, c2, ca3, c3, sl, a1, a2, a3) {{c1, c2, c3, CT_INVALID, CT_INVALID, CT_INVALID, CT_INVALID, CT_INVALID, CT_INVALID, CT_INVALID, CT_INVALID, CT_INVALID, CT_INVALID, CT_INVALID, CT_INVALID, CT_INVALID}, {ca1, ca2, ca3}, sl, a1, a2, a3, 0, {0, ANIM_STATUS_NO_ANIMATION, 2, 0}, INDTILE_SPECIAL_NONE, true, GRFFileProps(INVALID_INDUSTRYTILE)}
static const IndustryTileSpec _origin_industry_tile_specs[NEW_INDUSTRYTILEOFFSET] = {
	/* Coal Mine */
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, true),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(1, CT_PASSENGERS,   0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),

	/* Power Station */
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(1, CT_PASSENGERS,   8, CT_COAL,         0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),

	/* Sawmill */
	MT(1, CT_PASSENGERS,   0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(1, CT_PASSENGERS,   0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(1, CT_PASSENGERS,   8, CT_WOOD,         0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),

	/* Forest Artic, temperate */
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP,                  17, INDUSTRYTILE_NOANIM, false), ///< Chopping forest
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM,                  16, false), ///< Growing forest

	/* Oil refinery */
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      8, CT_OIL,          0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(1, CT_PASSENGERS,   0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),

	/* Oil Rig */
	MT(0, CT_INVALID,      8, CT_PASSENGERS,   0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      8, CT_MAIL,         0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),

	/* Oil Wells artic, temperate and sub-tropical */
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, true ),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, true ),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, true ),

	/* Farm tropic, arctic and temperate */
	MT(1, CT_PASSENGERS,   0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(1, CT_PASSENGERS,   0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),

	/* Factory temperate */
	MT(8, CT_GRAIN,        8, CT_LIVESTOCK,    8, CT_STEEL,       SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(8, CT_GRAIN,        8, CT_LIVESTOCK,    8, CT_STEEL,       SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(8, CT_GRAIN,        8, CT_LIVESTOCK,    8, CT_STEEL,       SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(8, CT_GRAIN,        8, CT_LIVESTOCK,    8, CT_STEEL,       SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),

	/* Printing works */
	MT(0, CT_INVALID,      8, CT_PAPER,        0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      8, CT_PAPER,        0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      8, CT_PAPER,        0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      8, CT_PAPER,        0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),

	/* Copper ore mine */
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, true ),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(1, CT_PASSENGERS,   0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(1, CT_PASSENGERS,   0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),

	/* Steel mill */
	MT(1, CT_PASSENGERS,   8, CT_IRON_ORE,     0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(1, CT_PASSENGERS,   8, CT_IRON_ORE,     0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(1, CT_PASSENGERS,   8, CT_IRON_ORE,     0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(1, CT_PASSENGERS,   8, CT_IRON_ORE,     0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(1, CT_PASSENGERS,   8, CT_IRON_ORE,     0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(1, CT_PASSENGERS,   8, CT_IRON_ORE,     0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),

	/* Bank temperate*/
	MT(1, CT_PASSENGERS,   8, CT_VALUABLES,    0, CT_INVALID,     SLOPE_E,     INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(1, CT_PASSENGERS,   8, CT_VALUABLES,    0, CT_INVALID,     SLOPE_S,     INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),

	/* Food processing plant, tropic and arctic. CT_MAIZE or CT_WHEAT, CT_LIVESTOCK or CT_FRUIT*/
	MT(8, CT_MAIZE,        8, CT_LIVESTOCK,    0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(8, CT_MAIZE,        8, CT_LIVESTOCK,    0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(8, CT_MAIZE,        8, CT_LIVESTOCK,    0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(8, CT_MAIZE,        8, CT_LIVESTOCK,    0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),

	/* Paper mill */
	MT(0, CT_INVALID,      8, CT_WOOD,         0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      8, CT_WOOD,         0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      8, CT_WOOD,         0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      8, CT_WOOD,         0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      8, CT_WOOD,         0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      8, CT_WOOD,         0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      8, CT_WOOD,         0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      8, CT_WOOD,         0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),

	/* Gold mine */
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, true),

	/* Bank Sub Arctic */
	MT(0, CT_INVALID,      8, CT_GOLD,         0, CT_INVALID,     SLOPE_E,     INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      8, CT_GOLD,         0, CT_INVALID,     SLOPE_S,     INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),

	/* Diamond mine */
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),

	/* Iron ore Mine */
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),

	/* Fruit plantation */
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),

	/* Rubber plantation */
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),

	/* Water supply */
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),

	/* Water tower */
	MT(0, CT_INVALID,      8, CT_WATER,        0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),

	/* Factory (sub-tropical) */
	MT(8, CT_COPPER_ORE,   8, CT_RUBBER,       8, CT_WOOD,        SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(8, CT_COPPER_ORE,   8, CT_RUBBER,       8, CT_WOOD,        SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(8, CT_COPPER_ORE,   8, CT_RUBBER,       8, CT_WOOD,        SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(8, CT_COPPER_ORE,   8, CT_RUBBER,       8, CT_WOOD,        SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),

	/* Lumber mill */
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),

	/* Candyfloss forest */
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP,                 130, INDUSTRYTILE_NOANIM, false), ///< Chopping candyfloss
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM,                 129, false), ///< Growing candyfloss

	/* Sweet factory */
	MT(8, CT_COTTON_CANDY, 8, CT_TOFFEE,       8, CT_SUGAR,       SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(8, CT_COTTON_CANDY, 8, CT_TOFFEE,       8, CT_SUGAR,       SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(8, CT_COTTON_CANDY, 8, CT_TOFFEE,       8, CT_SUGAR,       SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(8, CT_COTTON_CANDY, 8, CT_TOFFEE,       8, CT_SUGAR,       SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),

	/* Battery farm */
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP,                 136, INDUSTRYTILE_NOANIM, false), ///< Reaping batteries
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM,                 135, false), ///< Growing batteries

	/* Cola wells */
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),

	/* Toy shop */
	MT(0, CT_INVALID,      8, CT_TOYS,         0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      8, CT_TOYS,         0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      8, CT_TOYS,         0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      8, CT_TOYS,         0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),

	/* Toy factory */
	MT(8, CT_BATTERIES,    8, CT_PLASTIC,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(8, CT_BATTERIES,    8, CT_PLASTIC,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(8, CT_BATTERIES,    8, CT_PLASTIC,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(8, CT_BATTERIES,    8, CT_PLASTIC,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(8, CT_BATTERIES,    8, CT_PLASTIC,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(8, CT_BATTERIES,    8, CT_PLASTIC,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),

	/* Plastic Fountain */
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),

	/* Fizzy drink factory */
	MT(8, CT_BUBBLES,      8, CT_COLA,         0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(8, CT_BUBBLES,      8, CT_COLA,         0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(8, CT_BUBBLES,      8, CT_COLA,         0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(8, CT_BUBBLES,      8, CT_COLA,         0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),

	/* Bubble generator */
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),

	/* Toffee quarry */
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),

	/* Sugar mine */
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
	MT(0, CT_INVALID,      0, CT_INVALID,      0, CT_INVALID,     SLOPE_STEEP, INDUSTRYTILE_NOANIM, INDUSTRYTILE_NOANIM, false),
};
#undef MT

#endif  /* BUILD_INDUSTRY_H */
