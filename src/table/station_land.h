/* $Id$ */

#define TILE_SEQ_LINE(dx, dy, dz, sx, sy, sz, img) { dx, dy, dz, sx, sy, sz, {img, PAL_NONE} },
#define TILE_SEQ_LINE_PAL(dx, dy, dz, sx, sy, sz, img, pal) { dx, dy, dz, sx, sy, sz, {img, pal} },
#define TILE_SEQ_END() { (byte)0x80, 0, 0, 0, 0, 0, {0, 0} }

static const DrawTileSeqStruct _station_display_nothing[] = {
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_0[] = {
	TILE_SEQ_LINE( 0,  0,  0, 16,  5,  2, SPR_RAIL_PLATFORM_X_REAR  | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_LINE( 0, 11,  0, 16,  5,  2, SPR_RAIL_PLATFORM_X_FRONT | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_1[] = {
	TILE_SEQ_LINE( 0,  0,  0,  5, 16,  2, SPR_RAIL_PLATFORM_Y_REAR  | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_LINE(11,  0,  0,  5, 16,  2, SPR_RAIL_PLATFORM_Y_FRONT | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_2[] = {
	TILE_SEQ_LINE( 0,  0,  0, 16,  5,  2, SPR_RAIL_PLATFORM_BUILDING_X | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_LINE( 0, 11,  0, 16,  5,  2, SPR_RAIL_PLATFORM_X_FRONT    | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_3[] = {
	TILE_SEQ_LINE( 0,  0,  0,  5, 16,  2, SPR_RAIL_PLATFORM_BUILDING_Y | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_LINE(11,  0,  0,  5, 16,  2, SPR_RAIL_PLATFORM_Y_FRONT    | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_4[] = {
	TILE_SEQ_LINE( 0,  0,  0, 16,  5,  7, SPR_RAIL_PLATFORM_PILLARS_X_REAR | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_LINE( 0, 11,  0, 16,  5,  2, SPR_RAIL_PLATFORM_X_FRONT        | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_LINE( 0,  0, 16, 16, 16, 10, SPR_RAIL_ROOF_STRUCTURE_X_TILE_A | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_LINE_PAL( 0,  0, (byte)0x80, 0,  0,  0, SPR_RAIL_ROOF_GLASS_X_TILE_A     | (1 << PALETTE_MODIFIER_TRANSPARENT), PALETTE_TO_TRANSPARENT)
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_5[] = {
	TILE_SEQ_LINE( 0,  0,  0,  5, 16,  2, SPR_RAIL_PLATFORM_PILLARS_Y_REAR | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_LINE(11,  0,  0,  5, 16,  2, SPR_RAIL_PLATFORM_Y_FRONT        | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_LINE( 0,  0, 16, 16, 16, 10, SPR_RAIL_ROOF_STRUCTURE_Y_TILE_A | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_LINE_PAL( 0,  0, (byte)0x80, 0,  0,  0, SPR_RAIL_ROOF_GLASS_Y_TILE_A     | (1 << PALETTE_MODIFIER_TRANSPARENT), PALETTE_TO_TRANSPARENT)
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_6[] = {
	TILE_SEQ_LINE( 0,  0,  0, 16,  5,  2, SPR_RAIL_PLATFORM_X_REAR          | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_LINE( 0, 11,  0, 16,  5,  2, SPR_RAIL_PLATFORM_PILLARS_X_FRONT | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_LINE( 0,  0, 16, 16, 16, 10, SPR_RAIL_ROOF_STRUCTURE_X_TILE_B  | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_LINE_PAL( 0,  0, (byte)0x80, 0,  0,  0, SPR_RAIL_ROOF_GLASS_X_TILE_B      | (1 << PALETTE_MODIFIER_TRANSPARENT), PALETTE_TO_TRANSPARENT)
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_7[] = {
	TILE_SEQ_LINE( 0,  0,  0,  5, 16,  2, SPR_RAIL_PLATFORM_Y_REAR          | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_LINE(11,  0,  0,  5, 16,  2, SPR_RAIL_PLATFORM_PILLARS_Y_FRONT | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_LINE( 0,  0, 16, 16, 16, 10, SPR_RAIL_ROOF_STRUCTURE_Y_TILE_B  | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_LINE_PAL( 0,  0, (byte)0x80, 0,  0,  0, SPR_RAIL_ROOF_GLASS_Y_TILE_B      | (1 << PALETTE_MODIFIER_TRANSPARENT), PALETTE_TO_TRANSPARENT)
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_9[] = {
	TILE_SEQ_LINE( 0,  0,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR)) // fences north
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_10[] = {
	TILE_SEQ_LINE(15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR)) // fences west
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_21[] = {
	TILE_SEQ_LINE(15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_22[] = {
	TILE_SEQ_LINE( 0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_23[] = {
	TILE_SEQ_LINE( 0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_24[] = {
	TILE_SEQ_LINE( 0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_25[] = {
	TILE_SEQ_LINE( 0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_26[] = {
	TILE_SEQ_LINE( 0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_27[] = {
	TILE_SEQ_LINE( 2,  0,  0, 11, 16, 40, SPR_AIRPORT_TERMINAL_A | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_28[] = {
	TILE_SEQ_LINE( 3,  3,  0, 10, 10, 60, SPR_AIRPORT_TOWER | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_LINE(15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_29[] = {
	TILE_SEQ_LINE( 0,  1,  0, 14, 14, 30, SPR_AIRPORT_CONCOURSE | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_30[] = {
	TILE_SEQ_LINE( 3,  3,  0, 10, 11, 35, SPR_AIRPORT_TERMINAL_B | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_31[] = {
	TILE_SEQ_LINE( 0,  3,  0, 16, 11, 40, SPR_AIRPORT_TERMINAL_C | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_32[] = {
	TILE_SEQ_LINE(14,  0,  0,  2, 16, 28, SPR_AIRPORT_HANGAR_FRONT | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_LINE( 0,  0,  0,  2, 16, 28, SPR_AIRPORT_HANGAR_REAR | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_33[] = {
	TILE_SEQ_LINE( 7, 11,  0,  3,  3, 14, SPR_AIRPORT_JETWAY_1)
	TILE_SEQ_LINE( 0,  0,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_34[] = {
	TILE_SEQ_LINE( 2,  7,  0,  3,  3, 14, SPR_AIRPORT_JETWAY_2)
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_35[] = {
	TILE_SEQ_LINE( 3,  2,  0,  3,  3, 14, SPR_AIRPORT_JETWAY_3)
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_36[] = {
	TILE_SEQ_LINE( 0,  8,  0, 14,  3, 14, SPR_AIRPORT_PASSENGER_TUNNEL)
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_38[] = {
	TILE_SEQ_LINE( 0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_39[] = {
	TILE_SEQ_LINE( 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_1)
	TILE_SEQ_LINE(15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_40[] = {
	TILE_SEQ_LINE( 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_2)
	TILE_SEQ_LINE(15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_41[] = {
	TILE_SEQ_LINE( 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_3)
	TILE_SEQ_LINE(15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_42[] = {
	TILE_SEQ_LINE( 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_4)
	TILE_SEQ_LINE(15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_43[] = {
	TILE_SEQ_LINE( 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_5)
	TILE_SEQ_LINE(15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_44[] = {
	TILE_SEQ_LINE( 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_6)
	TILE_SEQ_LINE(15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_45[] = {
	TILE_SEQ_LINE( 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_7)
	TILE_SEQ_LINE(15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_46[] = {
	TILE_SEQ_LINE( 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_8)
	TILE_SEQ_LINE(15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_47[] = {
	TILE_SEQ_LINE( 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_9)
	TILE_SEQ_LINE(15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_48[] = {
	TILE_SEQ_LINE( 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_A)
	TILE_SEQ_LINE(15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_49[] = {
	TILE_SEQ_LINE( 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_B)
	TILE_SEQ_LINE(15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_50[] = {
	TILE_SEQ_LINE( 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_C)
	TILE_SEQ_LINE(15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_51[] = {
	TILE_SEQ_LINE( 7,  7,  0,  2,  2, 70, SPR_UNMOVABLE_TRANSMITTER)
	TILE_SEQ_LINE( 0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_54[] = {
	TILE_SEQ_LINE( 0,  0,  0, 15, 15, 30, SPR_AIRFIELD_TERM_C_BUILD | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_55[] = {
	TILE_SEQ_LINE(15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_58[] = {
	TILE_SEQ_LINE( 0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_LINE( 4, 11,  0,  1,  1, 20, SPR_AIRFIELD_WIND_1 | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_59[] = {
	TILE_SEQ_LINE( 0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_LINE( 4, 11,  0,  1,  1, 20, SPR_AIRFIELD_WIND_2 | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_60[] = {
	TILE_SEQ_LINE( 0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_LINE( 4, 11,  0,  1,  1, 20, SPR_AIRFIELD_WIND_3 | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_61[] = {
	TILE_SEQ_LINE( 0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_LINE( 4, 11,  0,  1,  1, 20, SPR_AIRFIELD_WIND_4 | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_62[] = {
	TILE_SEQ_LINE( 0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_63[] = {
	TILE_SEQ_LINE( 0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_64[] = {
	TILE_SEQ_LINE( 0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_65[] = {
	TILE_SEQ_LINE(14,  0,  0,  2, 16, 28, SPR_AIRFIELD_HANGAR_FRONT | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_LINE( 0,  0,  0,  2, 16, 28, SPR_AIRFIELD_HANGAR_REAR | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_66[] = {
	TILE_SEQ_LINE( 0,  0,  0, 16, 16, 60, SPR_HELIPORT | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_67[] = {
	TILE_SEQ_LINE( 0, 15,  0, 13,  1, 10, SPR_TRUCK_STOP_NE_BUILD_A | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_LINE(13,  0,  0,  3, 16, 10, SPR_TRUCK_STOP_NE_BUILD_B | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_LINE( 2,  0,  0, 11,  1, 10, SPR_TRUCK_STOP_NE_BUILD_C | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_68[] = {
	TILE_SEQ_LINE(15,  3,  0,  1, 13, 10, SPR_TRUCK_STOP_SE_BUILD_A | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_LINE( 0,  0,  0, 16,  3, 10, SPR_TRUCK_STOP_SE_BUILD_B | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_LINE( 0,  3,  0,  1, 11, 10, SPR_TRUCK_STOP_SE_BUILD_C | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_69[] = {
	TILE_SEQ_LINE( 3,  0,  0, 13,  1, 10, SPR_TRUCK_STOP_SW_BUILD_A | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_LINE( 0,  0,  0,  3, 16, 10, SPR_TRUCK_STOP_SW_BUILD_B | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_LINE( 3, 15,  0, 11,  1, 10, SPR_TRUCK_STOP_SW_BUILD_C | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_70[] = {
	TILE_SEQ_LINE( 0,  0,  0,  1, 13, 10, SPR_TRUCK_STOP_NW_BUILD_A | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_LINE( 0, 13,  0, 16,  3, 10, SPR_TRUCK_STOP_NW_BUILD_B | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_LINE(15,  2,  0,  1, 11, 10, SPR_TRUCK_STOP_NW_BUILD_C | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_71[] = {
	TILE_SEQ_LINE( 2,  0,  0, 11,  1, 10, SPR_BUS_STOP_NE_BUILD_A | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_LINE(13,  0,  0,  3, 16, 10, SPR_BUS_STOP_NE_BUILD_B | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_LINE( 0, 13,  0, 13,  3, 10, SPR_BUS_STOP_NE_BUILD_C | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_72[] = {
	TILE_SEQ_LINE( 0,  3,  0,  1, 11, 10, SPR_BUS_STOP_SE_BUILD_A | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_LINE( 0,  0,  0, 16,  3, 10, SPR_BUS_STOP_SE_BUILD_B | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_LINE(13,  3,  0,  3, 13, 10, SPR_BUS_STOP_SE_BUILD_C | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_73[] = {
	TILE_SEQ_LINE( 3, 15,  0, 11,  1, 10, SPR_BUS_STOP_SW_BUILD_A | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_LINE( 0,  0,  0,  3, 16, 10, SPR_BUS_STOP_SW_BUILD_B | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_LINE( 3,  0,  0, 13,  3, 10, SPR_BUS_STOP_SW_BUILD_C | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_74[] = {
	TILE_SEQ_LINE(15,  2,  0,  1, 11, 10, SPR_BUS_STOP_NW_BUILD_A | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_LINE( 0, 13,  0, 16,  3, 10, SPR_BUS_STOP_NW_BUILD_B | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_LINE( 0,  0,  0,  3, 13, 10, SPR_BUS_STOP_NW_BUILD_C | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_76[] = {
	TILE_SEQ_LINE( 0,  4,  0, 16,  8,  8, SPR_DOCK_SLOPE_NE)
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_77[] = {
	TILE_SEQ_LINE( 4,  0,  0,  8, 16,  8, SPR_DOCK_SLOPE_SE)
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_78[] = {
	TILE_SEQ_LINE( 0,  4,  0, 16,  8,  8, SPR_DOCK_SLOPE_SW)
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_79[] = {
	TILE_SEQ_LINE( 4,  0,  0,  8, 16,  8, SPR_DOCK_SLOPE_NW)
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_80[] = {
	TILE_SEQ_LINE( 0,  4,  0, 16,  8,  8, SPR_DOCK_FLAT_X)
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_81[] = {
	TILE_SEQ_LINE( 4,  0,  0,  8, 16,  8, SPR_DOCK_FLAT_Y)
	TILE_SEQ_END()
};

/* Buoy, which will _always_ drown under the ship */
static const DrawTileSeqStruct _station_display_datas_82[] = {
	TILE_SEQ_LINE( 4,  -1,  0,  0,  0,  0, SPR_IMG_BOUY)
	TILE_SEQ_END()
};

// control tower with concrete underground and no fence
// concrete underground
static const DrawTileSeqStruct _station_display_datas_085[] = {
	TILE_SEQ_LINE( 3,  3,  0, 10, 10, 60, SPR_AIRPORT_TOWER | (1 << PALETTE_MODIFIER_COLOR))  // control tower
	TILE_SEQ_END()
};

// new airportdepot, facing west
// concrete underground
static const DrawTileSeqStruct _station_display_datas_086[] = {
	TILE_SEQ_LINE(14, 0,  0,  2, 16, 28, SPR_AIRFIELD_HANGAR_FRONT | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_LINE( 0, 0,  0,  2, 16, 28, SPR_AIRFIELD_HANGAR_REAR | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

// asphalt tile with fences in north
// concrete underground
static const DrawTileSeqStruct _station_display_datas_087[] = {
	TILE_SEQ_LINE( 0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

// end of runway
static const DrawTileSeqStruct _station_display_datas_088[] = {
	TILE_SEQ_LINE( 0,  0,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR)) // fences
	TILE_SEQ_END()
};

// runway tiles
static const DrawTileSeqStruct _station_display_datas_089[] = {
	TILE_SEQ_LINE( 0,  0,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR)) // fences
	TILE_SEQ_END()
};

// turning radar with concrete underground fences on south -- needs 12 tiles
// concrete underground
//BEGIN
static const DrawTileSeqStruct _station_display_datas_090[] = {
	TILE_SEQ_LINE( 7, 7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_1)   // turning radar
	TILE_SEQ_LINE(15, 0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR))  //fences
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_091[] = {
	TILE_SEQ_LINE( 7, 7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_2)
	TILE_SEQ_LINE(15, 0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_092[] = {
	TILE_SEQ_LINE(7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_3)
	TILE_SEQ_LINE(15, 0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_093[] = {
	TILE_SEQ_LINE(7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_4)
	TILE_SEQ_LINE(15, 0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_094[] = {
	TILE_SEQ_LINE(7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_5)
	TILE_SEQ_LINE(15, 0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_095[] = {
	TILE_SEQ_LINE(7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_6)
	TILE_SEQ_LINE(15, 0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_096[] = {
	TILE_SEQ_LINE(7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_7)
	TILE_SEQ_LINE(15, 0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_097[] = {
	TILE_SEQ_LINE(7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_8)
	TILE_SEQ_LINE(15, 0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_098[] = {
	TILE_SEQ_LINE(7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_9)
	TILE_SEQ_LINE(15, 0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_099[] = {
	TILE_SEQ_LINE(7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_A)
	TILE_SEQ_LINE(15, 0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_0100[] = {
	TILE_SEQ_LINE(7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_B)
	TILE_SEQ_LINE(15, 0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_0101[] = {
	TILE_SEQ_LINE(7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_C)
	TILE_SEQ_LINE(15, 0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};
//END

// turning radar with concrete underground fences on north -- needs 12 tiles
// concrete underground
//BEGIN
static const DrawTileSeqStruct _station_display_datas_0102[] = {
	TILE_SEQ_LINE(7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_1)   // turning radar
	TILE_SEQ_LINE(0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_0103[] = {
	TILE_SEQ_LINE(7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_2)
	TILE_SEQ_LINE(0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_0104[] = {
	TILE_SEQ_LINE(7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_3)
	TILE_SEQ_LINE(0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_0105[] = {
	TILE_SEQ_LINE(7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_4)
	TILE_SEQ_LINE(0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_0106[] = {
	TILE_SEQ_LINE(7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_5)
	TILE_SEQ_LINE(0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_0107[] = {
	TILE_SEQ_LINE(7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_6)
	TILE_SEQ_LINE(0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_0108[] = {
	TILE_SEQ_LINE(7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_7)
	TILE_SEQ_LINE(0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_0109[] = {
	TILE_SEQ_LINE(7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_8)
	TILE_SEQ_LINE(0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_0110[] = {
	TILE_SEQ_LINE(7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_9)
	TILE_SEQ_LINE(0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_0111[] = {
	TILE_SEQ_LINE(7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_A)
	TILE_SEQ_LINE(0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_0112[] = {
	TILE_SEQ_LINE(7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_B)
	TILE_SEQ_LINE(0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_0113[] = {
	TILE_SEQ_LINE(7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_C)
	TILE_SEQ_LINE(0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};
//END

// helipad for international airport
// concrete underground
static const DrawTileSeqStruct _station_display_datas_0114[] = {
	TILE_SEQ_LINE(10,  6,  0,  0,  0,  0, SPR_AIRPORT_HELIPAD)
	TILE_SEQ_LINE(15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR)) // fences bottom
	TILE_SEQ_END()
};

// helipad for commuter airport
// concrete underground
static const DrawTileSeqStruct _station_display_datas_0115[] = {
	TILE_SEQ_LINE(10,  6,  0,  0,  0,  0, SPR_AIRPORT_HELIPAD)
	TILE_SEQ_LINE( 0,  0,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR)) // fences left
	TILE_SEQ_END()
};

// helipad for continental airport
// concrete underground
static const DrawTileSeqStruct _station_display_datas_0116[] = {
	TILE_SEQ_LINE(10,  6,  0,  0,  0,  0, SPR_AIRPORT_HELIPAD)
	TILE_SEQ_END()
};

// asphalt tile with fences in north and south
// concrete underground
static const DrawTileSeqStruct _station_display_datas_0117[] = {
	TILE_SEQ_LINE( 0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_LINE(15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

// runway tiles with 2 corner fences
static const DrawTileSeqStruct _station_display_datas_0118[] = {
	TILE_SEQ_LINE(15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR)) // fences west
	TILE_SEQ_LINE( 0,  0,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR)) // fences north
	TILE_SEQ_END()
};

// runway tiles with 2 corner fences
static const DrawTileSeqStruct _station_display_datas_0119[] = {
	TILE_SEQ_LINE(15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR)) // fences west
	TILE_SEQ_LINE( 0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR)) // fences south
	TILE_SEQ_END()
};

// runway tiles with 2 corner fences
static const DrawTileSeqStruct _station_display_datas_0120[] = {
	TILE_SEQ_LINE( 0,  0,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR)) // fences north
	TILE_SEQ_LINE( 0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR)) // fences east
	TILE_SEQ_END()
};

// runway tiles with 2 corner fences
static const DrawTileSeqStruct _station_display_datas_0121[] = {
	TILE_SEQ_LINE( 0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR)) // fences east
	TILE_SEQ_LINE( 0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR)) // fences south
	TILE_SEQ_END()
};

// ======== new 2x2 helidepot ========
// helipad tiles with 2 corner fences top+right
static const DrawTileSeqStruct _station_display_datas_0122[] = {
	TILE_SEQ_LINE(10,  6,  0,  0,  0,  0, SPR_AIRPORT_HELIPAD)
	TILE_SEQ_LINE( 0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR)) // fences east
	TILE_SEQ_LINE( 0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR)) // fences south
	TILE_SEQ_END()
};

// tarmac tiles with 2 corner fences bottom+right
static const DrawTileSeqStruct _station_display_datas_0123[] = {
	TILE_SEQ_LINE(15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR)) // fences north
	TILE_SEQ_LINE( 0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR)) // fences south
	TILE_SEQ_END()
};

// helidepot office with concrete underground and no fence
// concrete underground, fences top + left
static const DrawTileSeqStruct _station_display_datas_0124[] = {
	TILE_SEQ_LINE( 0,  0,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR)) // fences left
	TILE_SEQ_LINE( 0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR)) // fences east
	TILE_SEQ_LINE( 3,  3,  0, 10, 10, 60, SPR_AIRPORT_HELIDEPOT_OFFICE | (1 << PALETTE_MODIFIER_COLOR))  // helidepot office
	TILE_SEQ_END()
};

// N/S runway plain
static const DrawTileSeqStruct _station_display_datas_0125[] = {
	TILE_SEQ_LINE( 0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR)) // fences east
	TILE_SEQ_END()
};

// N/S runway end
static const DrawTileSeqStruct _station_display_datas_0126[] = {
	TILE_SEQ_LINE( 0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR)) // fences east
	TILE_SEQ_END()
};

// N/S runway plain
static const DrawTileSeqStruct _station_display_datas_0127[] = {
	TILE_SEQ_LINE(15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR)) // fences bottom
	TILE_SEQ_END()
};

// N/S runway end
static const DrawTileSeqStruct _station_display_datas_0128[] = {
	TILE_SEQ_LINE(15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR)) // fences bottom
	TILE_SEQ_END()
};

// West facing hangar
static const DrawTileSeqStruct _station_display_datas_0129[] = {
	TILE_SEQ_LINE(14,  0,  0,  2, 16, 28, SPR_NEWHANGAR_W | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_LINE( 0,  0,  0,  2, 16, 28, SPR_NEWHANGAR_W_WALL | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

// North facing hangar
static const DrawTileSeqStruct _station_display_datas_0130[] = {
	TILE_SEQ_LINE(14,  0,  0,  2, 16, 28, SPR_NEWHANGAR_N | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

// East facing hangar
static const DrawTileSeqStruct _station_display_datas_0131[] = {
	TILE_SEQ_LINE(14,  0,  0,  2, 16, 28, SPR_NEWHANGAR_E | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

// helipad for district airport NS
// concrete underground
static const DrawTileSeqStruct _station_display_datas_0132[] = {
	TILE_SEQ_LINE(10,  6,  0,  0,  0,  0, SPR_AIRPORT_HELIPAD)
	TILE_SEQ_LINE(15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR)) // fences bottom
	TILE_SEQ_LINE( 0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR)) // fences right
	TILE_SEQ_END()
};

// helipad for district airport NS
// concrete underground
static const DrawTileSeqStruct _station_display_datas_0133[] = {
	TILE_SEQ_LINE(10,  6,  0,  0,  0,  0, SPR_AIRPORT_HELIPAD)
	TILE_SEQ_LINE( 0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR)) // fences south
	TILE_SEQ_END()
};

// helidepot office with concrete underground and fence north
// concrete underground
static const DrawTileSeqStruct _station_display_datas_0134[] = {
	TILE_SEQ_LINE( 0,  0,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR)) // fences north
	TILE_SEQ_LINE( 3,  3,  0, 10, 10, 60, SPR_AIRPORT_HELIDEPOT_OFFICE | (1 << PALETTE_MODIFIER_COLOR))  // helidepot office
	TILE_SEQ_END()
};

// helidepot office with concrete underground and fence east
// concrete underground
static const DrawTileSeqStruct _station_display_datas_0135[] = {
	TILE_SEQ_LINE( 0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR)) // fences east
	TILE_SEQ_LINE( 3,  3,  0, 10, 10, 60, SPR_AIRPORT_HELIDEPOT_OFFICE | (1 << PALETTE_MODIFIER_COLOR))  // helidepot office
	TILE_SEQ_END()
};

// helidepot office with concrete underground and fence west
// concrete underground
static const DrawTileSeqStruct _station_display_datas_0136[] = {
	TILE_SEQ_LINE(15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR)) // fences west
	TILE_SEQ_LINE( 3,  3,  0, 10, 10, 60, SPR_AIRPORT_HELIDEPOT_OFFICE | (1 << PALETTE_MODIFIER_COLOR))  // helidepot office
	TILE_SEQ_END()
};

// helidepot office with concrete underground and fence south
// concrete underground
static const DrawTileSeqStruct _station_display_datas_0137[] = {
	TILE_SEQ_LINE( 0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR)) // fences south
	TILE_SEQ_LINE( 3,  3,  0, 10, 10, 60, SPR_AIRPORT_HELIDEPOT_OFFICE | (1 << PALETTE_MODIFIER_COLOR))  // helidepot office
	TILE_SEQ_END()
};

// terminal with fence to east
static const DrawTileSeqStruct _station_display_datas_0138[] = {
	TILE_SEQ_LINE( 0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR)) // fences east
	TILE_SEQ_END()
};

// terminal with fence to south
static const DrawTileSeqStruct _station_display_datas_0139[] = {
	TILE_SEQ_LINE( 0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR)) // fences south
	TILE_SEQ_END()
};

// terminal with fence to north
static const DrawTileSeqStruct _station_display_datas_0140[] = {
	TILE_SEQ_LINE(15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR)) // fences west
	TILE_SEQ_END()
};

// concrete with fence to east
static const DrawTileSeqStruct _station_display_datas_0141[] = {
	TILE_SEQ_LINE( 0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR)) // fences east
	TILE_SEQ_END()
};

// concrete with fence to south
static const DrawTileSeqStruct _station_display_datas_0142[] = {
	TILE_SEQ_LINE( 0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR)) // fences south
	TILE_SEQ_END()
};

// helipad for district airport EW
// concrete underground
static const DrawTileSeqStruct _station_display_datas_0143[] = {
	TILE_SEQ_LINE(10,  6,  0,  0,  0,  0, SPR_AIRPORT_HELIPAD)
	TILE_SEQ_LINE(15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR)) // fences west
	TILE_SEQ_LINE( 0,  0,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR)) // fences north
	TILE_SEQ_END()
};

// helipad for district airport EW
// concrete underground
static const DrawTileSeqStruct _station_display_datas_0144[] = {
	TILE_SEQ_LINE(10,  6,  0,  0,  0,  0, SPR_AIRPORT_HELIPAD)
	TILE_SEQ_LINE(15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR)) // fences west
	TILE_SEQ_END()
};

// turning radar with concrete underground fences on south -- needs 12 tiles
// concrete underground
//BEGIN
static const DrawTileSeqStruct _station_display_datas_0145[] = {
	TILE_SEQ_LINE( 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_1)   // turning radar
	TILE_SEQ_LINE( 0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR)) // fences south
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_0146[] = {
	TILE_SEQ_LINE( 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_2)
	TILE_SEQ_LINE( 0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR)) // fences south
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_0147[] = {
	TILE_SEQ_LINE( 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_3)
	TILE_SEQ_LINE( 0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR)) // fences south
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_0148[] = {
	TILE_SEQ_LINE( 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_4)
	TILE_SEQ_LINE( 0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR)) // fences south
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_0149[] = {
	TILE_SEQ_LINE( 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_5)
	TILE_SEQ_LINE( 0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR)) // fences south
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_0150[] = {
	TILE_SEQ_LINE( 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_6)
	TILE_SEQ_LINE( 0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR)) // fences south
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_0151[] = {
	TILE_SEQ_LINE( 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_7)
	TILE_SEQ_LINE( 0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR)) // fences south
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_0152[] = {
	TILE_SEQ_LINE( 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_8)
	TILE_SEQ_LINE( 0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR)) // fences south
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_0153[] = {
	TILE_SEQ_LINE( 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_9)
	TILE_SEQ_LINE( 0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR)) // fences south
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_0154[] = {
	TILE_SEQ_LINE( 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_A)
	TILE_SEQ_LINE( 0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR)) // fences south
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_0155[] = {
	TILE_SEQ_LINE( 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_B)
	TILE_SEQ_LINE( 0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR)) // fences south
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_0156[] = {
	TILE_SEQ_LINE( 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_C)
	TILE_SEQ_LINE( 0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR)) // fences south
	TILE_SEQ_END()
};
//END

// helipad for helistation
// concrete underground
static const DrawTileSeqStruct _station_display_datas_0157[] = {
	TILE_SEQ_LINE( 0,  1,  2,  0,  0,  0, SPR_NEWHELIPAD)
	TILE_SEQ_LINE(15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR)) // fences west
	TILE_SEQ_LINE( 0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR)) // fences south
	TILE_SEQ_END()
};

// helipad for helistation
// concrete underground
static const DrawTileSeqStruct _station_display_datas_0158[] = {
	TILE_SEQ_LINE( 0,  1,  2,  0,  0,  0, SPR_NEWHELIPAD)
	TILE_SEQ_LINE(15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR)) // fences west
	TILE_SEQ_LINE( 0,  0,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR)) // fences north
	TILE_SEQ_END()
};

// helipad for helistation
// concrete underground
static const DrawTileSeqStruct _station_display_datas_0159[] = {
	TILE_SEQ_LINE( 0,  1,  2,  0,  0,  0, SPR_NEWHELIPAD)
	TILE_SEQ_LINE( 0,  0,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR)) // fences north
	TILE_SEQ_END()
};

// helidepot office with concrete underground - no fence
static const DrawTileSeqStruct _station_display_datas_0160[] = {
	TILE_SEQ_LINE( 3,  3,  0, 10, 10, 60, SPR_AIRPORT_HELIDEPOT_OFFICE | (1 << PALETTE_MODIFIER_COLOR))  // helidepot office
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_0161[] = {
	TILE_SEQ_LINE( 0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR)) // fences east
	TILE_SEQ_LINE( 0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR)) // fences south
	TILE_SEQ_END()
};

// half grass half SPR_AIRPORT_APRON
static const DrawTileSeqStruct _station_display_datas_0162[] = {
	TILE_SEQ_LINE(0,  0,  0,  0,  0,  0, SPR_GRASS_LEFT)
	TILE_SEQ_END()
};

// half grass half SPR_AIRPORT_APRON
static const DrawTileSeqStruct _station_display_datas_0163[] = {
	TILE_SEQ_LINE(0,  0,  0,  0,  0,  0, SPR_GRASS_RIGHT)
	TILE_SEQ_END()
};

// drive-through truck stop X
static const DrawTileSeqStruct _station_display_datas_0168[] = {
	TILE_SEQ_LINE( 0,  0,  0,  16,  3, 16, SPR_TRUCK_STOP_DT_X_W | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_LINE( 0, 13,  0,  16,  3, 16, SPR_TRUCK_STOP_DT_X_E | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

// drive-through truck stop Y
static const DrawTileSeqStruct _station_display_datas_0169[] = {
	TILE_SEQ_LINE(13,  0,  0,  3, 16, 16, SPR_TRUCK_STOP_DT_Y_W | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_LINE( 0,  0,  0,  3, 16, 16, SPR_TRUCK_STOP_DT_Y_E | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

// drive-through bus stop X
static const DrawTileSeqStruct _station_display_datas_0170[] = {
	TILE_SEQ_LINE( 0,  0,  0,  16,  3, 16, SPR_BUS_STOP_DT_X_W | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_LINE( 0, 13,  0,  16,  3, 16, SPR_BUS_STOP_DT_X_E | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

// drive-through bus stop Y
static const DrawTileSeqStruct _station_display_datas_0171[] = {
	TILE_SEQ_LINE(13,  0,  0,  3,  16, 16, SPR_BUS_STOP_DT_Y_W | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_LINE( 0,  0,  0,  3,  16, 16, SPR_BUS_STOP_DT_Y_E | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

#undef TILE_SEQ_END
#undef TILE_SEQ_LINE
#undef TILE_SEQ_LINE_PAL

#define TILE_SPRITE_LINE(img, dtss) { {img, PAL_NONE}, dtss },

static const DrawTileSprites _station_display_datas_rail[] = {
	TILE_SPRITE_LINE(SPR_RAIL_TRACK_X,               _station_display_datas_0)
	TILE_SPRITE_LINE(SPR_RAIL_TRACK_Y,               _station_display_datas_1)
	TILE_SPRITE_LINE(SPR_RAIL_TRACK_X,               _station_display_datas_2)
	TILE_SPRITE_LINE(SPR_RAIL_TRACK_Y,               _station_display_datas_3)
	TILE_SPRITE_LINE(SPR_RAIL_TRACK_X,               _station_display_datas_4)
	TILE_SPRITE_LINE(SPR_RAIL_TRACK_Y,               _station_display_datas_5)
	TILE_SPRITE_LINE(SPR_RAIL_TRACK_X,               _station_display_datas_6)
	TILE_SPRITE_LINE(SPR_RAIL_TRACK_Y,               _station_display_datas_7)
};

static const DrawTileSprites _station_display_datas_airport[] = {
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_nothing)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_9)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_10)
	TILE_SPRITE_LINE(SPR_AIRPORT_AIRCRAFT_STAND,     _station_display_nothing)
	TILE_SPRITE_LINE(SPR_AIRPORT_TAXIWAY_NS_WEST,    _station_display_nothing)
	TILE_SPRITE_LINE(SPR_AIRPORT_TAXIWAY_EW_SOUTH,   _station_display_nothing)
	TILE_SPRITE_LINE(SPR_AIRPORT_TAXIWAY_XING_SOUTH, _station_display_nothing)
	TILE_SPRITE_LINE(SPR_AIRPORT_TAXIWAY_XING_WEST,  _station_display_nothing)
	TILE_SPRITE_LINE(SPR_AIRPORT_TAXIWAY_NS_CTR,     _station_display_nothing)
	TILE_SPRITE_LINE(SPR_AIRPORT_TAXIWAY_XING_EAST,  _station_display_nothing)
	TILE_SPRITE_LINE(SPR_AIRPORT_TAXIWAY_NS_EAST,    _station_display_nothing)
	TILE_SPRITE_LINE(SPR_AIRPORT_TAXIWAY_EW_NORTH,   _station_display_nothing)
	TILE_SPRITE_LINE(SPR_AIRPORT_TAXIWAY_EW_CTR,     _station_display_nothing)
	TILE_SPRITE_LINE(SPR_AIRPORT_TAXIWAY_EW_NORTH,   _station_display_datas_21)
	TILE_SPRITE_LINE(SPR_AIRPORT_RUNWAY_EXIT_A,      _station_display_datas_22)
	TILE_SPRITE_LINE(SPR_AIRPORT_RUNWAY_EXIT_B,      _station_display_datas_23)
	TILE_SPRITE_LINE(SPR_AIRPORT_RUNWAY_EXIT_C,      _station_display_datas_24)
	TILE_SPRITE_LINE(SPR_AIRPORT_RUNWAY_EXIT_D,      _station_display_datas_25)
	TILE_SPRITE_LINE(SPR_AIRPORT_RUNWAY_END,         _station_display_datas_26)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_27)
	TILE_SPRITE_LINE(SPR_FLAT_GRASS_TILE,            _station_display_datas_28)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_29)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_30)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_31)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_32)
	TILE_SPRITE_LINE(SPR_AIRPORT_AIRCRAFT_STAND,     _station_display_datas_33)
	TILE_SPRITE_LINE(SPR_AIRPORT_AIRCRAFT_STAND,     _station_display_datas_34)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_35)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_36)
	TILE_SPRITE_LINE(SPR_FLAT_GRASS_TILE,            _station_display_nothing)
	TILE_SPRITE_LINE(SPR_FLAT_GRASS_TILE,            _station_display_datas_38)
	TILE_SPRITE_LINE(SPR_FLAT_GRASS_TILE,            _station_display_datas_39)
	TILE_SPRITE_LINE(SPR_FLAT_GRASS_TILE,            _station_display_datas_40)
	TILE_SPRITE_LINE(SPR_FLAT_GRASS_TILE,            _station_display_datas_41)
	TILE_SPRITE_LINE(SPR_FLAT_GRASS_TILE,            _station_display_datas_42)
	TILE_SPRITE_LINE(SPR_FLAT_GRASS_TILE,            _station_display_datas_43)
	TILE_SPRITE_LINE(SPR_FLAT_GRASS_TILE,            _station_display_datas_44)
	TILE_SPRITE_LINE(SPR_FLAT_GRASS_TILE,            _station_display_datas_45)
	TILE_SPRITE_LINE(SPR_FLAT_GRASS_TILE,            _station_display_datas_46)
	TILE_SPRITE_LINE(SPR_FLAT_GRASS_TILE,            _station_display_datas_47)
	TILE_SPRITE_LINE(SPR_FLAT_GRASS_TILE,            _station_display_datas_48)
	TILE_SPRITE_LINE(SPR_FLAT_GRASS_TILE,            _station_display_datas_49)
	TILE_SPRITE_LINE(SPR_FLAT_GRASS_TILE,            _station_display_datas_50)
	TILE_SPRITE_LINE(SPR_FLAT_GRASS_TILE,            _station_display_datas_51)
	TILE_SPRITE_LINE(SPR_AIRFIELD_TERM_A,            _station_display_nothing)
	TILE_SPRITE_LINE(SPR_AIRFIELD_TERM_B,            _station_display_nothing)
	TILE_SPRITE_LINE(SPR_AIRFIELD_TERM_C_GROUND | (1 << PALETTE_MODIFIER_COLOR), _station_display_datas_54)
	TILE_SPRITE_LINE(SPR_AIRFIELD_APRON_A,           _station_display_datas_55)
	TILE_SPRITE_LINE(SPR_AIRFIELD_APRON_B,           _station_display_nothing)
	TILE_SPRITE_LINE(SPR_AIRFIELD_APRON_C,           _station_display_nothing)
	TILE_SPRITE_LINE(SPR_AIRFIELD_APRON_D,           _station_display_datas_58)
	TILE_SPRITE_LINE(SPR_AIRFIELD_APRON_D,           _station_display_datas_59)
	TILE_SPRITE_LINE(SPR_AIRFIELD_APRON_D,           _station_display_datas_60)
	TILE_SPRITE_LINE(SPR_AIRFIELD_APRON_D,           _station_display_datas_61)
	TILE_SPRITE_LINE(SPR_AIRFIELD_RUNWAY_NEAR_END,   _station_display_datas_62)
	TILE_SPRITE_LINE(SPR_AIRFIELD_RUNWAY_MIDDLE,     _station_display_datas_63)
	TILE_SPRITE_LINE(SPR_AIRFIELD_RUNWAY_FAR_END,    _station_display_datas_64)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_65)
	TILE_SPRITE_LINE(SPR_FLAT_GRASS_TILE,            _station_display_datas_66)
	TILE_SPRITE_LINE(SPR_AIRPORT_RUNWAY_END,         _station_display_nothing)
	TILE_SPRITE_LINE(SPR_AIRPORT_RUNWAY_EXIT_B,      _station_display_nothing)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_085)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_086)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_087)
	TILE_SPRITE_LINE(SPR_AIRPORT_RUNWAY_END,         _station_display_datas_088)
	TILE_SPRITE_LINE(SPR_AIRPORT_RUNWAY_EXIT_B,      _station_display_datas_089)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_090)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_091)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_092)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_093)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_094)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_095)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_096)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_097)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_098)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_099)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_0100)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_0101)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_0102)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_0103)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_0104)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_0105)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_0106)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_0107)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_0108)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_0109)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_0110)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_0111)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_0112)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_0113)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_0114)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_0115)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_0116)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_0117)
	TILE_SPRITE_LINE(SPR_AIRPORT_RUNWAY_END,         _station_display_datas_0118)
	TILE_SPRITE_LINE(SPR_AIRPORT_RUNWAY_END,         _station_display_datas_0119)
	TILE_SPRITE_LINE(SPR_AIRPORT_RUNWAY_END,         _station_display_datas_0120)
	TILE_SPRITE_LINE(SPR_AIRPORT_RUNWAY_END,         _station_display_datas_0121)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_0122)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_0123)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_0124)
	TILE_SPRITE_LINE(SPR_NSRUNWAY1,                  _station_display_datas_0125)
	TILE_SPRITE_LINE(SPR_NSRUNWAY_END,               _station_display_datas_0126)
	TILE_SPRITE_LINE(SPR_NSRUNWAY1,                  _station_display_datas_0127)
	TILE_SPRITE_LINE(SPR_NSRUNWAY_END,               _station_display_datas_0128)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_0129)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_0130)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_0131)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_0132)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_0133)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_0134)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_0135)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_0136)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_0137)
	TILE_SPRITE_LINE(SPR_AIRPORT_AIRCRAFT_STAND,     _station_display_datas_0138)
	TILE_SPRITE_LINE(SPR_AIRPORT_AIRCRAFT_STAND,     _station_display_datas_0139)
	TILE_SPRITE_LINE(SPR_AIRPORT_AIRCRAFT_STAND,     _station_display_datas_0140)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_0141)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_0142)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_0143)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_0144)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_0145)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_0146)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_0147)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_0148)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_0149)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_0150)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_0151)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_0152)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_0153)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_0154)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_0155)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_0156)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_0157)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_0158)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_0159)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_0160)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_0161)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_0162)
	TILE_SPRITE_LINE(SPR_AIRPORT_APRON,              _station_display_datas_0163)
	TILE_SPRITE_LINE(SPR_FLAT_GRASS_TILE,            _station_display_datas_58)
	TILE_SPRITE_LINE(SPR_FLAT_GRASS_TILE,            _station_display_datas_59)
	TILE_SPRITE_LINE(SPR_FLAT_GRASS_TILE,            _station_display_datas_60)
	TILE_SPRITE_LINE(SPR_FLAT_GRASS_TILE,            _station_display_datas_61)
};

static const DrawTileSprites _station_display_datas_truck[] = {
	TILE_SPRITE_LINE(SPR_TRUCK_STOP_NE_GROUND | (1 << PALETTE_MODIFIER_COLOR), _station_display_datas_67)
	TILE_SPRITE_LINE(SPR_TRUCK_STOP_SE_GROUND | (1 << PALETTE_MODIFIER_COLOR), _station_display_datas_68)
	TILE_SPRITE_LINE(SPR_TRUCK_STOP_SW_GROUND | (1 << PALETTE_MODIFIER_COLOR), _station_display_datas_69)
	TILE_SPRITE_LINE(SPR_TRUCK_STOP_NW_GROUND | (1 << PALETTE_MODIFIER_COLOR), _station_display_datas_70)
	TILE_SPRITE_LINE(SPR_ROAD_PAVED_STRAIGHT_X,      _station_display_datas_0168)
	TILE_SPRITE_LINE(SPR_ROAD_PAVED_STRAIGHT_Y,      _station_display_datas_0169)
};

static const DrawTileSprites _station_display_datas_bus[] = {
	TILE_SPRITE_LINE(SPR_BUS_STOP_NE_GROUND   | (1 << PALETTE_MODIFIER_COLOR), _station_display_datas_71)
	TILE_SPRITE_LINE(SPR_BUS_STOP_SE_GROUND   | (1 << PALETTE_MODIFIER_COLOR), _station_display_datas_72)
	TILE_SPRITE_LINE(SPR_BUS_STOP_SW_GROUND   | (1 << PALETTE_MODIFIER_COLOR), _station_display_datas_73)
	TILE_SPRITE_LINE(SPR_BUS_STOP_NW_GROUND   | (1 << PALETTE_MODIFIER_COLOR), _station_display_datas_74)
	TILE_SPRITE_LINE(SPR_ROAD_PAVED_STRAIGHT_X,      _station_display_datas_0170)
	TILE_SPRITE_LINE(SPR_ROAD_PAVED_STRAIGHT_Y,      _station_display_datas_0171)
};

static const DrawTileSprites _station_display_datas_oilrig[] = {
	TILE_SPRITE_LINE(SPR_FLAT_WATER_TILE,            _station_display_nothing)
};

static const DrawTileSprites _station_display_datas_dock[] = {
	TILE_SPRITE_LINE(SPR_SHORE_BASE + SLOPE_SW,      _station_display_datas_76)
	TILE_SPRITE_LINE(SPR_SHORE_BASE + SLOPE_NW,      _station_display_datas_77)
	TILE_SPRITE_LINE(SPR_SHORE_BASE + SLOPE_NE,      _station_display_datas_78)
	TILE_SPRITE_LINE(SPR_SHORE_BASE + SLOPE_SE,      _station_display_datas_79)
	TILE_SPRITE_LINE(SPR_FLAT_WATER_TILE,            _station_display_datas_80)
	TILE_SPRITE_LINE(SPR_FLAT_WATER_TILE,            _station_display_datas_81)
};

static const DrawTileSprites _station_display_datas_buoy[] = {
	TILE_SPRITE_LINE(SPR_FLAT_WATER_TILE,            _station_display_datas_82)
};

#undef TILE_SPRITE_LINE

static const DrawTileSprites *_station_display_datas[] = {
	_station_display_datas_rail,
	_station_display_datas_airport,
	_station_display_datas_truck,
	_station_display_datas_bus,
	_station_display_datas_oilrig,
	_station_display_datas_dock,
	_station_display_datas_buoy,
};
