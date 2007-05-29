/* $Id$ */

/** @file build_industry.h */

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

/**
 * Terminator of industry tiles layout definition
 */
#define MKEND {{-0x80, 0}, 0}

static const IndustryTileTable _tile_table_coal_mine_0[] = {
	MK(1, 1, 0),
	MK(1, 2, 2),
	MK(0, 0, 5),
	MK(1, 0, 6),
	MK(2, 0, 3),
	MK(2, 2, 3),
	MKEND
};

static const IndustryTileTable _tile_table_coal_mine_1[] = {
	MK(1, 1, 0),
	MK(1, 2, 2),
	MK(2, 0, 0),
	MK(2, 1, 2),
	MK(1, 0, 3),
	MK(0, 0, 3),
	MK(0, 1, 4),
	MK(0, 2, 4),
	MK(2, 2, 4),
	MKEND
};

static const IndustryTileTable _tile_table_coal_mine_2[] = {
	MK(0, 0, 0),
	MK(0, 1, 2),
	MK(0, 2, 5),
	MK(1, 0, 3),
	MK(1, 1, 3),
	MK(1, 2, 6),
	MKEND
};

static const IndustryTileTable _tile_table_coal_mine_3[] = {
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
	MKEND
};

static const IndustryTileTable * const _tile_table_coal_mine[] = {
	_tile_table_coal_mine_0,
	_tile_table_coal_mine_1,
	_tile_table_coal_mine_2,
	_tile_table_coal_mine_3,
};

static const IndustryTileTable _tile_table_power_station_0[] = {
	MK(0, 0, 7),
	MK(0, 1, 9),
	MK(1, 0, 7),
	MK(1, 1, 8),
	MK(2, 0, 7),
	MK(2, 1, 8),
	MK(3, 0, 10),
	MK(3, 1, 10),
	MKEND
};

static const IndustryTileTable _tile_table_power_station_1[] = {
	MK(0, 1, 7),
	MK(0, 2, 7),
	MK(1, 0, 8),
	MK(1, 1, 8),
	MK(1, 2, 7),
	MK(2, 0, 9),
	MK(2, 1, 10),
	MK(2, 2, 9),
	MKEND
};

static const IndustryTileTable _tile_table_power_station_2[] = {
	MK(0, 0, 7),
	MK(0, 1, 7),
	MK(1, 0, 9),
	MK(1, 1, 8),
	MK(2, 0, 10),
	MK(2, 1, 9),
	MKEND
};

static const IndustryTileTable * const _tile_table_power_station[] = {
	_tile_table_power_station_0,
	_tile_table_power_station_1,
	_tile_table_power_station_2,
};

static const IndustryTileTable _tile_table_sawmill_0[] = {
	MK(1, 0, 14),
	MK(1, 1, 12),
	MK(1, 2, 11),
	MK(2, 0, 14),
	MK(2, 1, 13),
	MK(0, 0, 15),
	MK(0, 1, 15),
	MK(0, 2, 12),
	MKEND
};

static const IndustryTileTable _tile_table_sawmill_1[] = {
	MK(0, 0, 15),
	MK(0, 1, 11),
	MK(0, 2, 14),
	MK(1, 0, 15),
	MK(1, 1, 13),
	MK(1, 2, 12),
	MK(2, 0, 11),
	MK(2, 1, 13),
	MKEND
};

static const IndustryTileTable * const _tile_table_sawmill[] = {
	_tile_table_sawmill_0,
	_tile_table_sawmill_1,
};

static const IndustryTileTable _tile_table_forest_0[] = {
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
	MKEND
};

static const IndustryTileTable _tile_table_forest_1[] = {
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
	MKEND
};

static const IndustryTileTable * const _tile_table_forest[] = {
	_tile_table_forest_0,
	_tile_table_forest_1,
};

static const IndustryTileTable _tile_table_oil_refinery_0[] = {
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
	MKEND
};

static const IndustryTileTable _tile_table_oil_refinery_1[] = {
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
	MKEND
};

static const IndustryTileTable * const _tile_table_oil_refinery[] = {
	_tile_table_oil_refinery_0,
	_tile_table_oil_refinery_1,
};

static const IndustryTileTable _tile_table_oil_rig_0[] = {
	MK(0, 0, 24),
	MK(0, 1, 24),
	MK(0, 2, 25),
	MK(1, 0, 26),
	MK(1, 1, 27),
	MK(1, 2, 28),
	MK(-4, -5, 255),
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
	MK(-3, 5, 255),
	MK(-2, 5, 255),
	MK(-1, 5, 255),
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
	MK(-1, -5, 255),
	MK(-2, -5, 255),
	MK(-3, -5, 255),
	MK(2, 0, 255),
	MKEND
};

static const IndustryTileTable * const _tile_table_oil_rig[] = {
	_tile_table_oil_rig_0,
};

static const IndustryTileTable _tile_table_factory_0[] = {
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
	MKEND
};

static const IndustryTileTable _tile_table_factory_1[] = {
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
	MKEND
};

static const IndustryTileTable * const _tile_table_factory[] = {
	_tile_table_factory_0,
	_tile_table_factory_1,
};

static const IndustryTileTable _tile_table_printing_works_0[] = {
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
	MKEND
};

static const IndustryTileTable _tile_table_printing_works_1[] = {
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
	MKEND
};

static const IndustryTileTable * const _tile_table_printing_works[] = {
	_tile_table_printing_works_0,
	_tile_table_printing_works_1,
};

static const IndustryTileTable _tile_table_steel_mill_0[] = {
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
	MKEND
};

static const IndustryTileTable _tile_table_steel_mill_1[] = {
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
	MKEND
};

static const IndustryTileTable * const _tile_table_steel_mill[] = {
	_tile_table_steel_mill_0,
	_tile_table_steel_mill_1,
};

static const IndustryTileTable _tile_table_farm_0[] = {
	MK(1, 0, 33),
	MK(1, 1, 34),
	MK(1, 2, 36),
	MK(0, 0, 37),
	MK(0, 1, 37),
	MK(0, 2, 36),
	MK(2, 0, 35),
	MK(2, 1, 38),
	MK(2, 2, 38),
	MKEND
};

static const IndustryTileTable _tile_table_farm_1[] = {
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
	MKEND
};

static const IndustryTileTable _tile_table_farm_2[] = {
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
	MKEND
};

static const IndustryTileTable * const _tile_table_farm[] = {
	_tile_table_farm_0,
	_tile_table_farm_1,
	_tile_table_farm_2,
};

static const IndustryTileTable _tile_table_copper_mine_0[] = {
	MK(0, 0, 47),
	MK(0, 1, 49),
	MK(0, 2, 51),
	MK(1, 0, 47),
	MK(1, 1, 49),
	MK(1, 2, 50),
	MK(2, 0, 51),
	MK(2, 1, 51),
	MKEND
};

static const IndustryTileTable _tile_table_copper_mine_1[] = {
	MK(0, 0, 50),
	MK(0, 1, 47),
	MK(0, 2, 49),
	MK(1, 0, 47),
	MK(1, 1, 49),
	MK(1, 2, 51),
	MK(2, 0, 51),
	MK(2, 1, 47),
	MK(2, 2, 49),
	MKEND
};

static const IndustryTileTable * const _tile_table_copper_mine[] = {
	_tile_table_copper_mine_0,
	_tile_table_copper_mine_1,
};

static const IndustryTileTable _tile_table_oil_well_0[] = {
	MK(0, 0, 29),
	MK(1, 0, 29),
	MK(2, 0, 29),
	MK(0, 1, 29),
	MK(0, 2, 29),
	MKEND
};

static const IndustryTileTable _tile_table_oil_well_1[] = {
	MK(0, 0, 29),
	MK(1, 0, 29),
	MK(1, 1, 29),
	MK(2, 2, 29),
	MK(2, 3, 29),
	MKEND
};

static const IndustryTileTable * const _tile_table_oil_well[] = {
	_tile_table_oil_well_0,
	_tile_table_oil_well_1,
};

static const IndustryTileTable _tile_table_bank_0[] = {
	MK(0, 0, 58),
	MK(1, 0, 59),
	MKEND
};

static const IndustryTileTable * const _tile_table_bank[] = {
	_tile_table_bank_0,
};

static const IndustryTileTable _tile_table_food_process_0[] = {
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
	MKEND
};

static const IndustryTileTable _tile_table_food_process_1[] = {
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
	MKEND
};

static const IndustryTileTable * const _tile_table_food_process[] = {
	_tile_table_food_process_0,
	_tile_table_food_process_1,
};

static const IndustryTileTable _tile_table_paper_mill_0[] = {
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
	MKEND
};

static const IndustryTileTable * const _tile_table_paper_mill[] = {
	_tile_table_paper_mill_0,
};

static const IndustryTileTable _tile_table_gold_mine_0[] = {
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
	MKEND
};

static const IndustryTileTable * const _tile_table_gold_mine[] = {
	_tile_table_gold_mine_0,
};

static const IndustryTileTable _tile_table_bank2_0[] = {
	MK(0, 0, 89),
	MK(1, 0, 90),
	MKEND
};

static const IndustryTileTable * const _tile_table_bank2[] = {
	_tile_table_bank2_0,
};

static const IndustryTileTable _tile_table_diamond_mine_0[] = {
	MK(0, 0, 91),
	MK(0, 1, 92),
	MK(0, 2, 93),
	MK(1, 0, 94),
	MK(1, 1, 95),
	MK(1, 2, 96),
	MK(2, 0, 97),
	MK(2, 1, 98),
	MK(2, 2, 99),
	MKEND
};

static const IndustryTileTable * const _tile_table_diamond_mine[] = {
	_tile_table_diamond_mine_0,
};

static const IndustryTileTable _tile_table_iron_mine_0[] = {
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
	MKEND
};

static const IndustryTileTable * const _tile_table_iron_mine[] = {
	_tile_table_iron_mine_0,
};

static const IndustryTileTable _tile_table_fruit_plantation_0[] = {
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
	MKEND
};

static const IndustryTileTable * const _tile_table_fruit_plantation[] = {
	_tile_table_fruit_plantation_0,
};

static const IndustryTileTable _tile_table_rubber_plantation_0[] = {
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
	MKEND
};

static const IndustryTileTable * const _tile_table_rubber_plantation[] = {
	_tile_table_rubber_plantation_0,
};

static const IndustryTileTable _tile_table_water_supply_0[] = {
	MK(0, 0, 118),
	MK(0, 1, 119),
	MK(1, 0, 118),
	MK(1, 1, 119),
	MKEND
};

static const IndustryTileTable * const _tile_table_water_supply[] = {
	_tile_table_water_supply_0,
};

static const IndustryTileTable _tile_table_water_tower_0[] = {
	MK(0, 0, 120),
	MKEND
};

static const IndustryTileTable * const _tile_table_water_tower[] = {
	_tile_table_water_tower_0,
};

static const IndustryTileTable _tile_table_factory2_0[] = {
	MK(0, 0, 121),
	MK(0, 1, 122),
	MK(1, 0, 123),
	MK(1, 1, 124),
	MK(0, 2, 121),
	MK(0, 3, 122),
	MK(1, 2, 123),
	MK(1, 3, 124),
	MKEND
};

static const IndustryTileTable _tile_table_factory2_1[] = {
	MK(0, 0, 121),
	MK(0, 1, 122),
	MK(1, 0, 123),
	MK(1, 1, 124),
	MK(2, 0, 121),
	MK(2, 1, 122),
	MK(3, 0, 123),
	MK(3, 1, 124),
	MKEND
};

static const IndustryTileTable * const _tile_table_factory2[] = {
	_tile_table_factory2_0,
	_tile_table_factory2_1,
};

static const IndustryTileTable _tile_table_farm2_0[] = {
	MK(1, 0, 33),
	MK(1, 1, 34),
	MK(1, 2, 36),
	MK(0, 0, 37),
	MK(0, 1, 37),
	MK(0, 2, 36),
	MK(2, 0, 35),
	MK(2, 1, 38),
	MK(2, 2, 38),
	MKEND
};

static const IndustryTileTable _tile_table_farm2_1[] = {
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
	MKEND
};

static const IndustryTileTable _tile_table_farm2_2[] = {
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
	MKEND
};

static const IndustryTileTable * const _tile_table_farm2[] = {
	_tile_table_farm2_0,
	_tile_table_farm2_1,
	_tile_table_farm2_2,
};

static const IndustryTileTable _tile_table_lumber_mill_0[] = {
	MK(0, 0, 125),
	MK(0, 1, 126),
	MK(1, 0, 127),
	MK(1, 1, 128),
	MKEND
};

static const IndustryTileTable * const _tile_table_lumber_mill[] = {
	_tile_table_lumber_mill_0,
};

static const IndustryTileTable _tile_table_cotton_candy_0[] = {
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
	MKEND
};

static const IndustryTileTable _tile_table_cotton_candy_1[] = {
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
	MKEND
};

static const IndustryTileTable * const _tile_table_cotton_candy[] = {
	_tile_table_cotton_candy_0,
	_tile_table_cotton_candy_1,
};

static const IndustryTileTable _tile_table_candy_factory_0[] = {
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
	MKEND
};

static const IndustryTileTable _tile_table_candy_factory_1[] = {
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
	MKEND
};

static const IndustryTileTable * const _tile_table_candy_factory[] = {
	_tile_table_candy_factory_0,
	_tile_table_candy_factory_1,
};

static const IndustryTileTable _tile_table_battery_farm_0[] = {
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
	MKEND
};

static const IndustryTileTable * const _tile_table_battery_farm[] = {
	_tile_table_battery_farm_0,
};

static const IndustryTileTable _tile_table_cola_wells_0[] = {
	MK(0, 0, 137),
	MK(0, 1, 137),
	MK(0, 2, 137),
	MK(1, 0, 137),
	MK(1, 1, 137),
	MK(1, 2, 137),
	MK(2, 1, 137),
	MK(2, 2, 137),
	MKEND
};

static const IndustryTileTable _tile_table_cola_wells_1[] = {
	MK(0, 1, 137),
	MK(0, 2, 137),
	MK(0, 3, 137),
	MK(1, 0, 137),
	MK(1, 1, 137),
	MK(1, 2, 137),
	MK(2, 1, 137),
	MKEND
};

static const IndustryTileTable * const _tile_table_cola_wells[] = {
	_tile_table_cola_wells_0,
	_tile_table_cola_wells_1,
};

static const IndustryTileTable _tile_table_toy_shop_0[] = {
	MK(0, 0, 138),
	MK(0, 1, 139),
	MK(1, 0, 140),
	MK(1, 1, 141),
	MKEND
};

static const IndustryTileTable * const _tile_table_toy_shop[] = {
	_tile_table_toy_shop_0,
};

static const IndustryTileTable _tile_table_toy_factory_0[] = {
	MK(0, 0, 147),
	MK(0, 1, 142),
	MK(1, 0, 147),
	MK(1, 1, 143),
	MK(2, 0, 147),
	MK(2, 1, 144),
	MK(3, 0, 146),
	MK(3, 1, 145),
	MKEND
};

static const IndustryTileTable * const _tile_table_toy_factory[] = {
	_tile_table_toy_factory_0,
};

static const IndustryTileTable _tile_table_plastic_fountain_0[] = {
	MK(0, 0, 148),
	MK(0, 1, 151),
	MK(0, 2, 154),
	MKEND
};

static const IndustryTileTable _tile_table_plastic_fountain_1[] = {
	MK(0, 0, 148),
	MK(1, 0, 151),
	MK(2, 0, 154),
	MKEND
};

static const IndustryTileTable * const _tile_table_plastic_fountain[] = {
	_tile_table_plastic_fountain_0,
	_tile_table_plastic_fountain_1,
};

static const IndustryTileTable _tile_table_fizzy_drink_0[] = {
	MK(0, 0, 156),
	MK(0, 1, 157),
	MK(1, 0, 158),
	MK(1, 1, 159),
	MKEND
};

static const IndustryTileTable * const _tile_table_fizzy_drink[] = {
	_tile_table_fizzy_drink_0,
};

static const IndustryTileTable _tile_table_bubble_generator_0[] = {
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
	MKEND
};

static const IndustryTileTable * const _tile_table_bubble_generator[] = {
	_tile_table_bubble_generator_0,
};

static const IndustryTileTable _tile_table_toffee_quarry_0[] = {
	MK(0, 0, 164),
	MK(1, 0, 165),
	MK(2, 0, 166),
	MKEND
};

static const IndustryTileTable * const _tile_table_toffee_quarry[] = {
	_tile_table_toffee_quarry_0,
};

static const IndustryTileTable _tile_table_sugar_mine_0[] = {
	MK(0, 0, 167),
	MK(0, 1, 168),
	MK(1, 0, 169),
	MK(1, 1, 170),
	MK(2, 0, 171),
	MK(2, 1, 172),
	MK(3, 0, 173),
	MK(3, 1, 174),
	MKEND
};

static const IndustryTileTable * const _tile_table_sugar_mine[] = {
	_tile_table_sugar_mine_0,
};

#undef MK
#undef MKEND

/** Array with saw sound, for sawmill */
static const uint8 _sawmill_sounds[] = { SND_28_SAWMILL };

/** Array with whistle sound, for factory */
static const uint8 _factory_sounds[] = { SND_03_FACTORY_WHISTLE };

/** Array with 3 animal sounds, for farms */
static const uint8 _farm_sounds[] = { SND_24_SHEEP, SND_25_COW, SND_26_HORSE };

/** Array with... hem... a sound of toyland */
static const uint8 _plastic_mine_sounds[] = { SND_33_PLASTIC_MINE };

/**
 * Writes the properties of an industry into the IndustrySpec struct.
 * @param tbl  tile table
 * @param sndc number of sounds
 * @param snd  sounds table
 * @param d    cost multiplier
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
 * @param clim climate availaility
 * @param bev  industry behaviour
 * @param in   name
 * @param intx text while building
 * @param s1   text for closure
 * @param s2   text for production up
 * @param s3   text for production down
 */

#define MI(tbl, sndc, snd, d, ai1, ai2, ai3, ai4, ag1, ag2, ag3, ag4, col, \
           c1, c2, c3, proc, p1, r1, p2, r2, m, a1, im1, a2, im2, a3, im3, pr, clim, bev, in, intx, s1, s2, s3) \
	 {tbl, lengthof(tbl), d, {c1, c2, c3}, proc, {p1, p2}, {r1, r2}, m,            \
	 {a1, a2, a3}, {{im1, 0}, {im2, 0}, {im3, 0}}, pr, clim, bev, col, in, intx, s1, s2, s3, {ai1, ai2, ai3, ai4}, {ag1, ag2, ag3, ag4}, \
	 sndc, snd, 0, true, {0, 0, NULL, NULL, INVALID_INDUSTRYTYPE}}
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
	MI(_tile_table_coal_mine,                  0, NULL,
	   210,                                    2, 3, 0, 0,    8, 8, 0, 0,        215,
	   IT_POWER_STATION,  IT_INVALID,          IT_INVALID,       CHECK_NOTHING,
	   CT_COAL,       15, CT_INVALID,       0, 5,
	   CT_INVALID,     0, CT_INVALID,       0, CT_INVALID,     0,
	   INDUSTRYLIFE_PRODUCTION,                1 << LT_TEMPERATE | 1 << LT_ARCTIC,
	   INDUSTRYBEH_CAN_SUBSIDENCE,
	   STR_4802_COAL_MINE,                     STR_482D_NEW_UNDER_CONSTRUCTION,
	   STR_4832_ANNOUNCES_IMMINENT_CLOSURE,    STR_4836_NEW_COAL_SEAM_FOUND_AT,   STR_4839_PRODUCTION_DOWN_BY_50),

	MI(_tile_table_power_station,              0, NULL,
	   30,                                     2, 2, 0, 0,    5, 5, 0, 0,        184,
	   IT_COAL_MINE,      IT_INVALID,          IT_INVALID,       CHECK_NOTHING,
	   CT_INVALID,     0, CT_INVALID,       0, 5,
	   CT_COAL,        0, CT_INVALID,       0, CT_INVALID,     0,
	   INDUSTRYLIFE_NOT_CLOSABLE,              1 << LT_TEMPERATE | 1 << LT_ARCTIC,
	   INDUSTRYBEH_NONE,
	   STR_4803_POWER_STATION,                 STR_482D_NEW_UNDER_CONSTRUCTION,
	   STR_4832_ANNOUNCES_IMMINENT_CLOSURE,    STR_4835_INCREASES_PRODUCTION,     STR_4839_PRODUCTION_DOWN_BY_50),

	MI(_tile_table_sawmill,                    1, _sawmill_sounds,
	   28,                                     2, 0, 0, 0,    5, 0, 0, 0,        194,
	   IT_FOREST,         IT_INVALID,          IT_INVALID,       CHECK_NOTHING,
	   CT_GOODS,       0, CT_INVALID,       0, 5,
	   CT_WOOD,      256, CT_INVALID,       0, CT_INVALID,     0,
	   INDUSTRYLIFE_CLOSABLE,                  1 << LT_TEMPERATE,
	   INDUSTRYBEH_NONE,
	   STR_4804_SAWMILL,                       STR_482D_NEW_UNDER_CONSTRUCTION,
	   STR_4833_SUPPLY_PROBLEMS_CAUSE_TO,      STR_4835_INCREASES_PRODUCTION,     STR_4839_PRODUCTION_DOWN_BY_50),

	MI(_tile_table_forest,                     0, NULL,
	   200,                                    3, 4, 0, 0,    5, 5, 0, 0,         86,
	   IT_SAWMILL,        IT_PAPER_MILL,       IT_INVALID,       CHECK_FOREST,
	   CT_WOOD,       13, CT_INVALID,       0, 30,
	   CT_INVALID,     0, CT_INVALID,       0, CT_INVALID,     0,
	   INDUSTRYLIFE_PRODUCTION,                1 << LT_TEMPERATE | 1 << LT_ARCTIC,
	   INDUSTRYBEH_NONE,
	   STR_4805_FOREST,                        STR_482E_NEW_BEING_PLANTED_NEAR,
	   STR_4832_ANNOUNCES_IMMINENT_CLOSURE,    STR_4835_INCREASES_PRODUCTION,     STR_483A_INSECT_INFESTATION_CAUSES),

	MI(_tile_table_oil_refinery,               0, NULL,
	   31,                                     2, 2, 2, 0,    4, 4, 4, 0,        191,
	   IT_OIL_RIG,        IT_INVALID,          IT_INVALID,       CHECK_REFINERY,
	   CT_GOODS,       0, CT_INVALID,       0, 5,
	   CT_OIL,       256, CT_INVALID,       0, CT_INVALID,     0,
	   INDUSTRYLIFE_CLOSABLE,                  1 << LT_TEMPERATE | 1 << LT_ARCTIC | 1 << LT_TROPIC,
	   INDUSTRYBEH_AIRPLANE_ATTACKS,
	   STR_4806_OIL_REFINERY,                  STR_482D_NEW_UNDER_CONSTRUCTION,
	   STR_4833_SUPPLY_PROBLEMS_CAUSE_TO,      STR_4835_INCREASES_PRODUCTION,     STR_4839_PRODUCTION_DOWN_BY_50),

	MI(_tile_table_oil_rig,                    0, NULL,
	   240,                                    6, 0, 0, 0,    0, 0, 0, 0,        152,
	   IT_OIL_REFINERY,   IT_INVALID,          IT_INVALID,       CHECK_OIL_RIG,
	   CT_OIL,        15, CT_PASSENGERS,    2, 5,
	   CT_INVALID,     0, CT_INVALID,       0, CT_INVALID,     0,
	   INDUSTRYLIFE_PRODUCTION,                1 << LT_TEMPERATE,
	   INDUSTRYBEH_BUILT_ONWATER | INDUSTRYBEH_AFTER_1960 | INDUSTRYBEH_AI_AIRSHIP_ROUTES,
	   STR_4807_OIL_RIG,                       STR_482D_NEW_UNDER_CONSTRUCTION,
	   STR_4832_ANNOUNCES_IMMINENT_CLOSURE,    STR_4837_NEW_OIL_RESERVES_FOUND,   STR_4839_PRODUCTION_DOWN_BY_50),

	MI(_tile_table_factory,                    1, _factory_sounds,
	   26,                                     2, 0, 0, 0,    5, 0, 0, 0,        174,
	   IT_FARM,           IT_STEEL_MILL,       IT_INVALID,       CHECK_NOTHING,
	   CT_GOODS,       0, CT_INVALID,       0, 5,
	   CT_LIVESTOCK, 256, CT_GRAIN ,      256, CT_STEEL,    256,
	   INDUSTRYLIFE_CLOSABLE,                  1 << LT_TEMPERATE,
	   INDUSTRYBEH_CHOPPER_ATTACKS,
	   STR_4808_FACTORY,                       STR_482D_NEW_UNDER_CONSTRUCTION,
	   STR_4833_SUPPLY_PROBLEMS_CAUSE_TO,      STR_4835_INCREASES_PRODUCTION,     STR_4839_PRODUCTION_DOWN_BY_50),

	MI(_tile_table_printing_works,             1, _factory_sounds,
	   26,                                     0, 2, 0, 0,    0, 5, 0, 0,        174,
	   IT_PAPER_MILL,     IT_INVALID,          IT_INVALID,       CHECK_NOTHING,
	   CT_GOODS,       0, CT_INVALID,       0, 5,
	   CT_PAPER,     256, CT_INVALID,       0, CT_INVALID,     0,
	   INDUSTRYLIFE_CLOSABLE,                  1 << LT_ARCTIC,
	   INDUSTRYBEH_NONE,
	   STR_4809_PRINTING_WORKS,                STR_482D_NEW_UNDER_CONSTRUCTION,
	   STR_4833_SUPPLY_PROBLEMS_CAUSE_TO,      STR_4835_INCREASES_PRODUCTION,     STR_4839_PRODUCTION_DOWN_BY_50),

	MI(_tile_table_steel_mill,                 0, NULL,
	   27,                                     2, 0, 0, 0,    5, 0, 0, 0,         10,
	   IT_IRON_MINE,      IT_FACTORY,          IT_INVALID,       CHECK_NOTHING,
	   CT_STEEL,       0, CT_INVALID,       0, 5,
	   CT_IRON_ORE,  256, CT_INVALID,       0, CT_INVALID,     0,
	   INDUSTRYLIFE_CLOSABLE,                  1 << LT_TEMPERATE,
	   INDUSTRYBEH_NONE,
	   STR_480A_STEEL_MILL,                    STR_482D_NEW_UNDER_CONSTRUCTION,
	   STR_4833_SUPPLY_PROBLEMS_CAUSE_TO,      STR_4835_INCREASES_PRODUCTION,     STR_4839_PRODUCTION_DOWN_BY_50),

	MI(_tile_table_farm,                       3, _farm_sounds,
	   250,                                    2, 4, 0, 0,    9, 9, 0, 0,         48,
	   IT_FACTORY,        IT_FOOD_PROCESS,     IT_INVALID,       CHECK_FARM,
	   CT_GRAIN,      10, CT_LIVESTOCK,    10, 5,
	   CT_INVALID,     0, CT_INVALID,       0, CT_INVALID,     0,
	   INDUSTRYLIFE_PRODUCTION,                1 << LT_TEMPERATE | 1 << LT_ARCTIC,
	   INDUSTRYBEH_PLANT_FIELDS | INDUSTRYBEH_PLANT_ON_BUILT,
	   STR_480B_FARM,                          STR_482D_NEW_UNDER_CONSTRUCTION,
	   STR_4832_ANNOUNCES_IMMINENT_CLOSURE,    STR_4838_IMPROVED_FARMING_METHODS, STR_483A_INSECT_INFESTATION_CAUSES),

	MI(_tile_table_copper_mine,                0, NULL,
	   205,                                    0, 0, 3, 0,    0, 0, 4, 0,         10,
	   IT_FACTORY_2,      IT_INVALID,          IT_INVALID,       CHECK_NOTHING,
	   CT_COPPER_ORE, 10, CT_INVALID,       0, 5,
	   CT_INVALID,     0, CT_INVALID,       0, CT_INVALID,     0,
	   INDUSTRYLIFE_PRODUCTION,                1 << LT_TROPIC,
	   INDUSTRYBEH_NONE,
	   STR_480C_COPPER_ORE_MINE,               STR_482D_NEW_UNDER_CONSTRUCTION,
	   STR_4832_ANNOUNCES_IMMINENT_CLOSURE,    STR_4835_INCREASES_PRODUCTION,     STR_4839_PRODUCTION_DOWN_BY_50),

	MI(_tile_table_oil_well,                   0, NULL,               220,              0, 5, 3, 0,    4, 5, 5, 0,        152,
	   IT_OIL_REFINERY,   IT_INVALID,          IT_INVALID,       CHECK_NOTHING,
	   CT_OIL,        12, CT_INVALID,       0, 5,
	   CT_INVALID,     0, CT_INVALID,       0, CT_INVALID,     0,
	   INDUSTRYLIFE_PRODUCTION,                1 << LT_TEMPERATE | 1 << LT_ARCTIC | 1 << LT_TROPIC,
	   INDUSTRYBEH_DONT_INCR_PROD | INDUSTRYBEH_BEFORE_1950,
	   STR_480D_OIL_WELLS,                     STR_482D_NEW_UNDER_CONSTRUCTION,
	   STR_4832_ANNOUNCES_IMMINENT_CLOSURE,    STR_4837_NEW_OIL_RESERVES_FOUND,   STR_4839_PRODUCTION_DOWN_BY_50),

	MI(_tile_table_bank,                       0, NULL,
	   193,                                    7, 0, 0, 0,    0, 0, 0, 0,         15,
	   IT_BANK_TEMP,      IT_INVALID,          IT_INVALID,       CHECK_NOTHING,
	   CT_VALUABLES,   6, CT_INVALID,       0, 5,
	   CT_VALUABLES,   0, CT_INVALID,       0, CT_INVALID,     0,
	   INDUSTRYLIFE_NOT_CLOSABLE,              1 << LT_TEMPERATE,
	   INDUSTRYBEH_TOWN1200_MORE | INDUSTRYBEH_ONLY_INTOWN,
	   STR_480E_BANK,                          STR_482D_NEW_UNDER_CONSTRUCTION,
	   STR_4832_ANNOUNCES_IMMINENT_CLOSURE,    STR_4835_INCREASES_PRODUCTION,     STR_4839_PRODUCTION_DOWN_BY_50),

	MI(_tile_table_food_process,               0, NULL,
	   26,                                     0, 2, 2, 0,    0, 3, 4, 0,         55,
	   IT_FRUIT_PLANTATION, IT_FARM,           IT_FARM_2,        CHECK_NOTHING,
	   CT_FOOD,        0, CT_INVALID,       0, 5,
	   CT_FRUIT,     256, CT_MAIZE,       256, CT_INVALID,     0,
	   INDUSTRYLIFE_CLOSABLE,                  1 << LT_ARCTIC | 1 << LT_TROPIC,
	   INDUSTRYBEH_NONE,
	   STR_480F_FOOD_PROCESSING_PLANT,         STR_482D_NEW_UNDER_CONSTRUCTION,
	   STR_4833_SUPPLY_PROBLEMS_CAUSE_TO,      STR_4835_INCREASES_PRODUCTION,     STR_4839_PRODUCTION_DOWN_BY_50),

	MI(_tile_table_paper_mill,                 1, _sawmill_sounds,
	   28,                                     0, 2, 0, 0,    0, 5, 0, 0,         10,
	   IT_FOREST,         IT_PRINTING_WORKS,   IT_INVALID,       CHECK_NOTHING,
	   CT_PAPER,       0, CT_INVALID,       0, 5,
	   CT_WOOD,      256, CT_INVALID,       0, CT_INVALID,     0,
	   INDUSTRYLIFE_CLOSABLE,                  1 << LT_ARCTIC,
	   INDUSTRYBEH_NONE,
	   STR_4810_PAPER_MILL,                    STR_482D_NEW_UNDER_CONSTRUCTION,
	   STR_4833_SUPPLY_PROBLEMS_CAUSE_TO,      STR_4835_INCREASES_PRODUCTION,     STR_4839_PRODUCTION_DOWN_BY_50),

	MI(_tile_table_gold_mine,                  0, NULL,
	   208,                                    0, 3, 0, 0,    0, 4, 0, 0,        194,
	   IT_BANK_TROPIC_ARCTIC, IT_INVALID,      IT_INVALID,       CHECK_NOTHING,
	   CT_GOLD,        7, CT_INVALID,       0, 5,
	   CT_INVALID,     0, CT_INVALID,       0, CT_INVALID,     0,
	   INDUSTRYLIFE_PRODUCTION,                1 << LT_ARCTIC,
	   INDUSTRYBEH_NONE,
	   STR_4811_GOLD_MINE,                     STR_482D_NEW_UNDER_CONSTRUCTION,
	   STR_4832_ANNOUNCES_IMMINENT_CLOSURE,    STR_4835_INCREASES_PRODUCTION,     STR_4839_PRODUCTION_DOWN_BY_50),

	MI(_tile_table_bank2,                      0, NULL,
	   19,                                     0, 3, 3, 0,    0, 6, 5, 0,         15,
	   IT_GOLD_MINE,      IT_DIAMOND_MINE,     IT_INVALID,       CHECK_NOTHING,
	   CT_INVALID,     0, CT_INVALID,       0, 5,
	   CT_GOLD,      256, CT_INVALID,       0, CT_INVALID,     0,
	   INDUSTRYLIFE_NOT_CLOSABLE,              1 << LT_ARCTIC | 1 << LT_TROPIC,
	   INDUSTRYBEH_ONLY_INTOWN,
	   STR_4812_BANK,                          STR_482D_NEW_UNDER_CONSTRUCTION,
	   STR_4832_ANNOUNCES_IMMINENT_CLOSURE,    STR_4835_INCREASES_PRODUCTION,     STR_4839_PRODUCTION_DOWN_BY_50),

	MI(_tile_table_diamond_mine,               0, NULL,
	   213,                                    0, 0, 3, 0,    0, 0, 4, 0,        184,
	   IT_BANK_TROPIC_ARCTIC, IT_INVALID,      IT_INVALID,       CHECK_NOTHING,
	   CT_DIAMONDS,    7, CT_INVALID,       0, 5,
	   CT_INVALID,     0, CT_INVALID,       0, CT_INVALID,     0,
	   INDUSTRYLIFE_PRODUCTION,                1 << LT_TROPIC,
	   INDUSTRYBEH_NONE,
	   STR_4813_DIAMOND_MINE,                  STR_482D_NEW_UNDER_CONSTRUCTION,
	   STR_4832_ANNOUNCES_IMMINENT_CLOSURE,    STR_4835_INCREASES_PRODUCTION,     STR_4839_PRODUCTION_DOWN_BY_50),

	MI(_tile_table_iron_mine,                  0, NULL,
	   220,                                    2, 0, 0, 0,    5, 0, 0, 0,         55,
	   IT_STEEL_MILL,     IT_INVALID,          IT_INVALID,       CHECK_NOTHING,
	   CT_IRON_ORE,   10, CT_INVALID,       0, 5,
	   CT_INVALID,     0, CT_INVALID,       0, CT_INVALID,     0,
	   INDUSTRYLIFE_PRODUCTION,                1 << LT_TEMPERATE,
	   INDUSTRYBEH_NONE,
	   STR_4814_IRON_ORE_MINE,                 STR_482D_NEW_UNDER_CONSTRUCTION,
	   STR_4832_ANNOUNCES_IMMINENT_CLOSURE,    STR_4835_INCREASES_PRODUCTION,     STR_4839_PRODUCTION_DOWN_BY_50),

	MI(_tile_table_fruit_plantation,           0, NULL,
	   225,                                    0, 0, 2, 0,    0, 0, 4, 0,         86,
	   IT_FOOD_PROCESS,   IT_INVALID,          IT_INVALID,       CHECK_PLANTATION,
	   CT_FRUIT,      10, CT_INVALID,       0, 15,
	   CT_INVALID,     0, CT_INVALID,       0, CT_INVALID,     0,
	   INDUSTRYLIFE_PRODUCTION,                1 << LT_TROPIC,
	   INDUSTRYBEH_NONE,
	   STR_4815_FRUIT_PLANTATION,              STR_482E_NEW_BEING_PLANTED_NEAR,
	   STR_4832_ANNOUNCES_IMMINENT_CLOSURE,    STR_4838_IMPROVED_FARMING_METHODS, STR_483A_INSECT_INFESTATION_CAUSES),

	MI(_tile_table_rubber_plantation,          0, NULL,
	   218,                                    0, 0, 3, 0,    0, 0, 4, 0,         39,
	   IT_FACTORY_2,      IT_INVALID,          IT_INVALID,       CHECK_PLANTATION,
	   CT_RUBBER,     10, CT_INVALID,       0, 15,
	   CT_INVALID,     0, CT_INVALID,       0, CT_INVALID,     0,
	   INDUSTRYLIFE_PRODUCTION,                1 << LT_TROPIC,
	   INDUSTRYBEH_NONE,
	   STR_4816_RUBBER_PLANTATION,             STR_482E_NEW_BEING_PLANTED_NEAR,
	   STR_4832_ANNOUNCES_IMMINENT_CLOSURE,    STR_4838_IMPROVED_FARMING_METHODS, STR_483A_INSECT_INFESTATION_CAUSES),

	MI(_tile_table_water_supply,               0, NULL,
	   199,                                    0, 0, 3, 0,    0, 0, 4, 0,         37,
	   IT_WATER_TOWER,    IT_INVALID,          IT_INVALID,       CHECK_WATER,
	   CT_WATER,      12, CT_INVALID,       0, 5,
	   CT_INVALID,     0, CT_INVALID,       0, CT_INVALID,     0,
	   INDUSTRYLIFE_PRODUCTION,                1 << LT_TROPIC,
	   INDUSTRYBEH_NONE,
	   STR_4817_WATER_SUPPLY,                  STR_482D_NEW_UNDER_CONSTRUCTION,
	   STR_4832_ANNOUNCES_IMMINENT_CLOSURE,    STR_4835_INCREASES_PRODUCTION,     STR_4839_PRODUCTION_DOWN_BY_50),

	MI(_tile_table_water_tower,                0, NULL,
	   14,                                     0, 0, 4, 0,    0, 0, 8, 0,        208,
	   IT_WATER_SUPPLY,   IT_INVALID,          IT_INVALID,       CHECK_WATER,
	   CT_INVALID,     0, CT_INVALID,       0, 5,
	   CT_WATER,     256, CT_INVALID,       0, CT_INVALID,     0,
	   INDUSTRYLIFE_NOT_CLOSABLE,              1 << LT_TROPIC,
	   INDUSTRYBEH_ONLY_INTOWN,
	   STR_4818_WATER_TOWER,                   STR_482D_NEW_UNDER_CONSTRUCTION,
	   STR_4832_ANNOUNCES_IMMINENT_CLOSURE,    STR_4835_INCREASES_PRODUCTION,     STR_4839_PRODUCTION_DOWN_BY_50),

	MI(_tile_table_factory2,                   1, _factory_sounds,
	   26,                                     0, 0, 2, 0,    0, 0, 4, 0,        174,
	   IT_RUBBER_PLANTATION, IT_COPPER_MINE,   IT_LUMBER_MILL,   CHECK_PLANTATION,
	   CT_GOODS,       0, CT_INVALID,       0, 5,
	   CT_RUBBER,    256, CT_COPPER_ORE,  256, CT_WOOD,      256,
	   INDUSTRYLIFE_CLOSABLE,                  1 << LT_TROPIC,
	   INDUSTRYBEH_NONE,
	   STR_4819_FACTORY,                       STR_482D_NEW_UNDER_CONSTRUCTION,
	   STR_4833_SUPPLY_PROBLEMS_CAUSE_TO,      STR_4835_INCREASES_PRODUCTION,     STR_4839_PRODUCTION_DOWN_BY_50),

	MI(_tile_table_farm2,                      0, NULL,
	   250,                                    0, 0, 1, 0,    0, 0, 2, 0,         48,
	   IT_FOOD_PROCESS,   IT_INVALID,          IT_INVALID,       CHECK_PLANTATION,
	   CT_MAIZE,      11, CT_INVALID,       0, 5,
	   CT_INVALID,     0, CT_INVALID,       0, CT_INVALID,     0,
	   INDUSTRYLIFE_PRODUCTION,                1 << LT_TROPIC,
	   INDUSTRYBEH_PLANT_FIELDS | INDUSTRYBEH_PLANT_ON_BUILT,
	   STR_481A_FARM,                          STR_482D_NEW_UNDER_CONSTRUCTION,
	   STR_4832_ANNOUNCES_IMMINENT_CLOSURE,    STR_4838_IMPROVED_FARMING_METHODS, STR_483A_INSECT_INFESTATION_CAUSES),

	MI(_tile_table_lumber_mill,                0, NULL,
	   17,                                     0, 0, 0, 0,    0, 0, 0, 0,        194,
	   IT_FACTORY_2,      IT_INVALID,          IT_INVALID,       CHECK_LUMBERMILL,
	   CT_WOOD,        0, CT_INVALID,       0, 5,
	   CT_INVALID,     0, CT_INVALID,       0, CT_INVALID,     0,
	   INDUSTRYLIFE_CLOSABLE,                  1 << LT_TROPIC,
	   INDUSTRYBEH_CUT_TREES,
	   STR_481B_LUMBER_MILL,                   STR_482D_NEW_UNDER_CONSTRUCTION,
	   STR_4834_LACK_OF_NEARBY_TREES_CAUSES,   STR_4835_INCREASES_PRODUCTION,     STR_4839_PRODUCTION_DOWN_BY_50),

	MI(_tile_table_cotton_candy,               0, NULL,
	   195,                                    0, 0, 0, 3,    0, 0, 0, 5,         48,
	   IT_CANDY_FACTORY,  IT_INVALID,          IT_INVALID,       CHECK_NOTHING,
	   CT_COTTON_CANDY, 13, CT_INVALID,    0, 30,
	   CT_INVALID,     0, CT_INVALID,       0, CT_INVALID,     0,
	   INDUSTRYLIFE_PRODUCTION,                1 << LT_TOYLAND,
	   INDUSTRYBEH_NONE,
	   STR_481C_COTTON_CANDY_FOREST,           STR_482E_NEW_BEING_PLANTED_NEAR,
	   STR_4832_ANNOUNCES_IMMINENT_CLOSURE,    STR_4838_IMPROVED_FARMING_METHODS, STR_4839_PRODUCTION_DOWN_BY_50),

	MI(_tile_table_candy_factory,              0, NULL,
	   26,                                     0, 0, 0, 3,    0, 0, 0, 5,        174,
	   IT_COTTON_CANDY,   IT_TOFFEE_QUARRY,    IT_SUGAR_MINE,    CHECK_NOTHING,
	   CT_CANDY,       0, CT_INVALID,       0, 5,
	   CT_SUGAR,     256, CT_TOFFEE,      256, CT_COTTON_CANDY, 256,
	   INDUSTRYLIFE_CLOSABLE,                  1 << LT_TOYLAND,
	   INDUSTRYBEH_NONE,
	   STR_481D_CANDY_FACTORY,                 STR_482D_NEW_UNDER_CONSTRUCTION,
	   STR_4833_SUPPLY_PROBLEMS_CAUSE_TO,      STR_4835_INCREASES_PRODUCTION,     STR_4839_PRODUCTION_DOWN_BY_50),

	MI(_tile_table_battery_farm,               0, NULL,
	   187,                                    0, 0, 0, 3,    0, 0, 0, 4,         39,
	   IT_TOY_FACTORY,    IT_INVALID,          IT_INVALID,       CHECK_NOTHING,
	   CT_BATTERIES,  11, CT_INVALID,      0, 30,
	   CT_INVALID,     0, CT_INVALID,       0, CT_INVALID,     0,
	   INDUSTRYLIFE_PRODUCTION,                1 << LT_TOYLAND,
	   INDUSTRYBEH_NONE,
	   STR_481E_BATTERY_FARM,                  STR_482D_NEW_UNDER_CONSTRUCTION,
	   STR_4832_ANNOUNCES_IMMINENT_CLOSURE,    STR_4838_IMPROVED_FARMING_METHODS, STR_483A_INSECT_INFESTATION_CAUSES),

	MI(_tile_table_cola_wells,                 0, NULL,
	   193,                                    0, 0, 0, 3,    0, 0, 0, 5,         55,
	   IT_FIZZY_DRINK_FACTORY, IT_INVALID,     IT_INVALID,       CHECK_NOTHING,
	   CT_COLA,       12, CT_INVALID,       0, 5,
	   CT_INVALID,     0, CT_INVALID,       0, CT_INVALID,     0,
	   INDUSTRYLIFE_PRODUCTION,                1 << LT_TOYLAND,
	   INDUSTRYBEH_NONE,
	   STR_481F_COLA_WELLS,                    STR_482D_NEW_UNDER_CONSTRUCTION,
	   STR_4832_ANNOUNCES_IMMINENT_CLOSURE,    STR_4835_INCREASES_PRODUCTION,     STR_4839_PRODUCTION_DOWN_BY_50),

	MI(_tile_table_toy_shop,                   0, NULL,
	   17,                                     0, 0, 0, 3,    0, 0, 0, 4,        208,
	   IT_TOY_FACTORY,    IT_INVALID,          IT_INVALID,       CHECK_NOTHING,
	   CT_INVALID,     0, CT_INVALID,       0, 5,
	   CT_TOYS,      256, CT_INVALID,       0, CT_INVALID,     0,
	   INDUSTRYLIFE_NOT_CLOSABLE,              1 << LT_TOYLAND,
	   INDUSTRYBEH_ONLY_NEARTOWN,
	   STR_4820_TOY_SHOP,                      STR_482D_NEW_UNDER_CONSTRUCTION,
	   STR_4833_SUPPLY_PROBLEMS_CAUSE_TO,      STR_4835_INCREASES_PRODUCTION,     STR_4839_PRODUCTION_DOWN_BY_50),

	MI(_tile_table_toy_factory,                0, NULL,
	   20,                                     0, 0, 0, 3,    0, 0, 0, 5,          10,
	   IT_PLASTIC_FOUNTAINS, IT_BATTERY_FARM,  IT_TOY_SHOP,     CHECK_NOTHING,
	   CT_TOYS,        0, CT_INVALID,       0, 5,
	   CT_PLASTIC,   256, CT_BATTERIES,   256, CT_INVALID,     0,
	   INDUSTRYLIFE_CLOSABLE,                  1 << LT_TOYLAND,
	   INDUSTRYBEH_NONE,
	   STR_4821_TOY_FACTORY,                   STR_482D_NEW_UNDER_CONSTRUCTION,
	   STR_4833_SUPPLY_PROBLEMS_CAUSE_TO,      STR_4835_INCREASES_PRODUCTION,     STR_4839_PRODUCTION_DOWN_BY_50),

	MI(_tile_table_plastic_fountain,           1, _plastic_mine_sounds,
	   192,                                    0, 0, 0, 3,    0, 0, 0, 5,         37,
	   IT_TOY_FACTORY,    IT_INVALID,          IT_INVALID,       CHECK_NOTHING,
	   CT_PLASTIC,    14, CT_INVALID,       0, 5,
	   CT_INVALID,     0, CT_INVALID,       0, CT_INVALID,     0,
	   INDUSTRYLIFE_PRODUCTION,                1 << LT_TOYLAND,
	   INDUSTRYBEH_NONE,
	   STR_4822_PLASTIC_FOUNTAINS,             STR_482D_NEW_UNDER_CONSTRUCTION,
	   STR_4832_ANNOUNCES_IMMINENT_CLOSURE,    STR_4835_INCREASES_PRODUCTION,     STR_4839_PRODUCTION_DOWN_BY_50),

	MI(_tile_table_fizzy_drink,                0, NULL,
	   22,                                     0, 0, 0, 3,    0, 0, 0, 4,        184,
	   IT_COLA_WELLS,     IT_BUBBLE_GENERATOR, IT_INVALID,       CHECK_NOTHING,
	   CT_FIZZY_DRINKS, 0, CT_INVALID,      0, 5,
	   CT_COLA,       256, CT_BUBBLES,    256, CT_INVALID,     0,
	   INDUSTRYLIFE_CLOSABLE,                  1 << LT_TOYLAND,
	   INDUSTRYBEH_NONE,
	   STR_4823_FIZZY_DRINK_FACTORY,           STR_482D_NEW_UNDER_CONSTRUCTION,
	   STR_4833_SUPPLY_PROBLEMS_CAUSE_TO,      STR_4835_INCREASES_PRODUCTION,     STR_4839_PRODUCTION_DOWN_BY_50),

	MI(_tile_table_bubble_generator,           0, NULL,
	   203,                                    0, 0, 0, 3,    0, 0, 0, 5,        152,
	   IT_FIZZY_DRINK_FACTORY, IT_INVALID,     IT_INVALID,       CHECK_BUBBLEGEN,
	   CT_BUBBLES,    13, CT_INVALID,       0, 5,
	   CT_INVALID,     0, CT_INVALID,       0, CT_INVALID,     0,
	   INDUSTRYLIFE_PRODUCTION,                1 << LT_TOYLAND,
	   INDUSTRYBEH_NONE,
	   STR_4824_BUBBLE_GENERATOR,              STR_482D_NEW_UNDER_CONSTRUCTION,
	   STR_4832_ANNOUNCES_IMMINENT_CLOSURE,    STR_4835_INCREASES_PRODUCTION,     STR_4839_PRODUCTION_DOWN_BY_50),

	MI(_tile_table_toffee_quarry,              0, NULL,
	   213,                                    0, 0, 0, 3,    0, 0, 0, 5,        194,
	   IT_CANDY_FACTORY,  IT_INVALID,          IT_INVALID,       CHECK_NOTHING,
	   CT_TOFFEE,     10, CT_INVALID,       0, 5,
	   CT_INVALID,     0, CT_INVALID,       0, CT_INVALID,     0,
	   INDUSTRYLIFE_PRODUCTION,                1 << LT_TOYLAND,
	   INDUSTRYBEH_NONE,
	   STR_4825_TOFFEE_QUARRY,                 STR_482D_NEW_UNDER_CONSTRUCTION,
	   STR_4832_ANNOUNCES_IMMINENT_CLOSURE,    STR_4835_INCREASES_PRODUCTION,     STR_4839_PRODUCTION_DOWN_BY_50),

	MI(_tile_table_sugar_mine,                 0, NULL,
	   210,                                    0, 0, 0, 2,    0, 0, 0, 4,         15,
	   IT_CANDY_FACTORY,  IT_INVALID,          IT_INVALID,       CHECK_NOTHING,
	   CT_SUGAR,      11, CT_INVALID,       0, 5,
	   CT_INVALID,     0, CT_INVALID,       0, CT_INVALID,     0,
	   INDUSTRYLIFE_PRODUCTION,                1 << LT_TOYLAND,
	   INDUSTRYBEH_NONE,
	   STR_4826_SUGAR_MINE,                    STR_482D_NEW_UNDER_CONSTRUCTION,
	   STR_4832_ANNOUNCES_IMMINENT_CLOSURE,    STR_4835_INCREASES_PRODUCTION,     STR_4839_PRODUCTION_DOWN_BY_50),
};
#undef MI

/** Writes the properties of an industry tile into the IndustryTileSpec struct.
 * @param ca1 acceptance of first cargo
 * @param c1  first type of cargo accepted for this tile
 * @param ca2 acceptance of second cargo
 * @param c2  second cargo
 * @param ca3 acceptance of third cargo
 * @param c3  and third cargo. Those three are in an array
 * @param s1  slope refused upon choosing a place to build
 * @param a1  animation frame on production
 * @param a2  next frame of animation
 * @param a3  chooses between animation or construction state
 */
#define MT(ca1, c1, ca2, c2, ca3, c3, sl, a1, a2, a3) {{c1, c2, c3}, {ca1, ca2, ca3}, sl, a1, a2, a3, 0, true, {0, 0, NULL, NULL, 0}}
static const IndustryTileSpec _origin_industry_tile_specs[NEW_INDUSTRYTILEOFFSET] = {
	/* Coal Mine */
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  true),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(1,  CT_PASSENGERS,   0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),

	/* Power Station */
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(1,  CT_PASSENGERS,   0,  CT_INVALID,   8,  CT_COAL,       SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),

	/* Sawmill */
	MT(1,  CT_PASSENGERS,   0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(1,  CT_PASSENGERS,   0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(1,  CT_PASSENGERS,   0,  CT_INVALID,   8,  CT_WOOD,       SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),

	/* Forest Artic, temperate */
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,                   17,  INDUSTRYTILE_NOANIM,   false), ///< Chopping forest
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,                   16,   false), ///< Growing forest

	/* Oil refinery */
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   8,  CT_OIL,        SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(1,  CT_PASSENGERS,   0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),

	/* oil Rig */
	MT(0,  CT_INVALID,      0,  CT_INVALID,   8,  CT_PASSENGERS, SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   8,  CT_MAIL,       SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),

	/* Oil Wells artic, temperate */
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  true ),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  true ),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  true ),

	/* Farm tropic, arctic and temperate */
	MT(1,  CT_PASSENGERS,   0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(1,  CT_PASSENGERS,   0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),

	/* Factory temperate */
	MT(8,  CT_GRAIN,        8,  CT_STEEL,     8,  CT_LIVESTOCK,  SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(8,  CT_GRAIN,        8,  CT_STEEL,     8,  CT_LIVESTOCK,  SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(8,  CT_GRAIN,        8,  CT_STEEL,     8,  CT_LIVESTOCK,  SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(8,  CT_GRAIN,        8,  CT_STEEL,     8,  CT_LIVESTOCK,  SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),

	/* Printing works */
	MT(0,  CT_INVALID,      0,  CT_INVALID,   8,  CT_PAPER,      SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   8,  CT_PAPER,      SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   8,  CT_PAPER,      SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   8,  CT_PAPER,      SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),

	/* Copper ore mine */
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  true ),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(1,  CT_PASSENGERS,   0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(1,  CT_PASSENGERS,   0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),

	/* Steel mill */
	MT(1,  CT_PASSENGERS,   0,  CT_INVALID,   8,  CT_IRON_ORE,   SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(1,  CT_PASSENGERS,   0,  CT_INVALID,   8,  CT_IRON_ORE,   SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(1,  CT_PASSENGERS,   0,  CT_INVALID,   8,  CT_IRON_ORE,   SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(1,  CT_PASSENGERS,   0,  CT_INVALID,   8,  CT_IRON_ORE,   SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(1,  CT_PASSENGERS,   0,  CT_INVALID,   8,  CT_IRON_ORE,   SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(1,  CT_PASSENGERS,   0,  CT_INVALID,   8,  CT_IRON_ORE,   SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),

	/* Bank temperate*/
	MT(1,  CT_PASSENGERS,   0,  CT_INVALID,   8,  CT_VALUABLES,  SLOPE_E,      INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(1,  CT_PASSENGERS,   0,  CT_INVALID,   8,  CT_VALUABLES,  SLOPE_S,      INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),

	/* Food processing plant, tropic and arctic. CT_MAIZE or CT_WHEAT, CT_LIVESTOCK or CT_FRUIT*/
	MT(8,  CT_MAIZE,        0,  CT_INVALID,   8,  CT_LIVESTOCK,  SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(8,  CT_MAIZE,        0,  CT_INVALID,   8,  CT_LIVESTOCK,  SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(8,  CT_MAIZE,        0,  CT_INVALID,   8,  CT_LIVESTOCK,  SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(8,  CT_MAIZE,        0,  CT_INVALID,   8,  CT_LIVESTOCK,  SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),

	/* Paper mill */
	MT(0,  CT_INVALID,      0,  CT_INVALID,   8,  CT_WOOD,       SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   8,  CT_WOOD,       SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   8,  CT_WOOD,       SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   8,  CT_WOOD,       SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   8,  CT_WOOD,       SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   8,  CT_WOOD,       SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   8,  CT_WOOD,       SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   8,  CT_WOOD,       SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),

	/* Gold mine */
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  true ),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),

	/* Bank Sub Arctic */
	MT(0,  CT_INVALID,      0,  CT_INVALID,   8,  CT_GOLD,       SLOPE_E,      INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   8,  CT_GOLD,       SLOPE_S,      INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),

	/* Diamond mine */
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),

	/* Iron ore Mine */
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),

	/* Fruit plantation */
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),

	/* Rubber plantation */
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),

	/* Water supply */
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),

	/* Water tower */
	MT(0,  CT_INVALID,      0,  CT_INVALID,   8,  CT_WATER,      SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),

	/* Factory (sub-tropical) */
	MT(8,  CT_COPPER_ORE,   8,  CT_WOOD,      8,  CT_RUBBER,     SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(8,  CT_COPPER_ORE,   8,  CT_WOOD,      8,  CT_RUBBER,     SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(8,  CT_COPPER_ORE,   8,  CT_WOOD,      8,  CT_RUBBER,     SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(8,  CT_COPPER_ORE,   8,  CT_WOOD,      8,  CT_RUBBER,     SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),

	/* Lumber mill */
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),

	/* Candyfloss forest */
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,                  130,   INDUSTRYTILE_NOANIM,  false), ///< Chopping candyfloss
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,                   129,  false), ///< Growing candyfloss

	/* Sweet factory */
	MT(8,  CT_COTTON_CANDY, 8,  CT_TOFFEE,    8,  CT_SUGAR,      SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(8,  CT_COTTON_CANDY, 8,  CT_TOFFEE,    8,  CT_SUGAR,      SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(8,  CT_COTTON_CANDY, 8,  CT_TOFFEE,    8,  CT_SUGAR,      SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(8,  CT_COTTON_CANDY, 8,  CT_TOFFEE,    8,  CT_SUGAR,      SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),

	/* Batter farm */
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,                  136,   INDUSTRYTILE_NOANIM,  false), ///< Reaping batteries
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,                   135,  false), ///< Growing batteries

	/* Cola wells */
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),

	/* Toy shop */
	MT(0,  CT_INVALID,      0,  CT_INVALID,   8,  CT_TOYS,       SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   8,  CT_TOYS,       SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   8,  CT_TOYS,       SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   8,  CT_TOYS,       SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),

	/* Toy factory */
	MT(8,  CT_BATTERIES,    0,  CT_INVALID,   8,  CT_PLASTIC,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(8,  CT_BATTERIES,    0,  CT_INVALID,   8,  CT_PLASTIC,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(8,  CT_BATTERIES,    0,  CT_INVALID,   8,  CT_PLASTIC,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(8,  CT_BATTERIES,    0,  CT_INVALID,   8,  CT_PLASTIC,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(8,  CT_BATTERIES,    0,  CT_INVALID,   8,  CT_PLASTIC,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(8,  CT_BATTERIES,    0,  CT_INVALID,   8,  CT_PLASTIC,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),

	/* Plastic Fountain */
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),

	/* Fizzy drink factory */
	MT(8,  CT_BUBBLES,      0,  CT_INVALID,   8,  CT_COLA,       SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(8,  CT_BUBBLES,      0,  CT_INVALID,   8,  CT_COLA,       SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(8,  CT_BUBBLES,      0,  CT_INVALID,   8,  CT_COLA,       SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(8,  CT_BUBBLES,      0,  CT_INVALID,   8,  CT_COLA,       SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),

	/* Bubble generator */
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),

	/* Toffee quarry */
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),

	/* Sugar mine */
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
	MT(0,  CT_INVALID,      0,  CT_INVALID,   0,  CT_INVALID,    SLOPE_STEEP,  INDUSTRYTILE_NOANIM,   INDUSTRYTILE_NOANIM,  false),
};
#undef MT

#endif  /* BUILD_INDUSTRY_H */
