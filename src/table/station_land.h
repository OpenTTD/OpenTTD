/* $Id$ */

#define TILE_SEQ_END()	{ (byte)0x80, 0, 0, 0, 0, 0, 0, 0 }

static const DrawTileSeqStruct _station_display_nothing[] = {
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_0[] = {
	{  0,  0,  0, 16,  5,  2, SPR_RAIL_PLATFORM_X_REAR  | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	{  0, 11,  0, 16,  5,  2, SPR_RAIL_PLATFORM_X_FRONT | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_1[] = {
	{  0,  0,  0,  5, 16,  2, SPR_RAIL_PLATFORM_Y_REAR  | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	{ 11,  0,  0,  5, 16,  2, SPR_RAIL_PLATFORM_Y_FRONT | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_2[] = {
	{  0,  0,  0, 16,  5,  2, SPR_RAIL_PLATFORM_BUILDING_X | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	{  0, 11,  0, 16,  5,  2, SPR_RAIL_PLATFORM_X_FRONT    | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_3[] = {
	{  0,  0,  0,  5, 16,  2, SPR_RAIL_PLATFORM_BUILDING_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	{ 11,  0,  0,  5, 16,  2, SPR_RAIL_PLATFORM_Y_FRONT    | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_4[] = {
	{  0,  0,  0, 16,  5,  7, SPR_RAIL_PLATFORM_PILLARS_X_REAR | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	{  0, 11,  0, 16,  5,  2, SPR_RAIL_PLATFORM_X_FRONT        | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	{  0,  0, 16, 16, 16, 10, SPR_RAIL_ROOF_STRUCTURE_X_TILE_A | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	{  0,  0,(byte)0x80, 0,  0,  0, SPR_RAIL_ROOF_GLASS_X_TILE_A     | (1 << PALETTE_MODIFIER_TRANSPARENT), PALETTE_TO_TRANSPARENT },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_5[] = {
	{  0,  0,  0,  5, 16,  2, SPR_RAIL_PLATFORM_PILLARS_Y_REAR | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	{ 11,  0,  0,  5, 16,  2, SPR_RAIL_PLATFORM_Y_FRONT        | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	{  0,  0, 16, 16, 16, 10, SPR_RAIL_ROOF_STRUCTURE_Y_TILE_A | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	{  0,  0,(byte)0x80, 0,  0,  0, SPR_RAIL_ROOF_GLASS_Y_TILE_A     | (1 << PALETTE_MODIFIER_TRANSPARENT), PALETTE_TO_TRANSPARENT },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_6[] = {
	{  0,  0,  0, 16,  5,  2, SPR_RAIL_PLATFORM_X_REAR          | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	{  0, 11,  0, 16,  5,  2, SPR_RAIL_PLATFORM_PILLARS_X_FRONT | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	{  0,  0, 16, 16, 16, 10, SPR_RAIL_ROOF_STRUCTURE_X_TILE_B  | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	{  0,  0,(byte)0x80, 0,  0,  0, SPR_RAIL_ROOF_GLASS_X_TILE_B      | (1 << PALETTE_MODIFIER_TRANSPARENT), PALETTE_TO_TRANSPARENT },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_7[] = {
	{  0,  0,  0,  5, 16,  2, SPR_RAIL_PLATFORM_Y_REAR          | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	{ 11,  0,  0,  5, 16,  2, SPR_RAIL_PLATFORM_PILLARS_Y_FRONT | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	{  0,  0, 16, 16, 16, 10, SPR_RAIL_ROOF_STRUCTURE_Y_TILE_B  | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	{  0,  0,(byte)0x80, 0,  0,  0, SPR_RAIL_ROOF_GLASS_Y_TILE_B      | (1 << PALETTE_MODIFIER_TRANSPARENT), PALETTE_TO_TRANSPARENT },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_9[] = {
	{  0,  0,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences north
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_10[] = {
	{ 15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences west
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_21[] = {
	{ 15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_22[] = {
	{  0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_23[] = {
	{  0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_24[] = {
	{  0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_25[] = {
	{  0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_26[] = {
	{  0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_27[] = {
	{  2,  0,  0, 11, 16, 40, SPR_AIRPORT_TERMINAL_A | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_28[] = {
	{  3,  3,  0, 10, 10, 60, SPR_AIRPORT_TOWER | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	{ 15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_29[] = {
	{  0,  1,  0, 14, 14, 30, SPR_AIRPORT_CONCOURSE | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_30[] = {
	{  3,  3,  0, 10, 11, 35, SPR_AIRPORT_TERMINAL_B | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_31[] = {
	{  0,  3,  0, 16, 11, 40, SPR_AIRPORT_TERMINAL_C | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_32[] = {
	{ 14,  0,  0,  2, 16, 28, SPR_AIRPORT_HANGAR_FRONT | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	{  0,  0,  0,  2, 16, 28, SPR_AIRPORT_HANGAR_REAR, PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_33[] = {
	{  7, 11,  0,  3,  3, 14, SPR_AIRPORT_JETWAY_1, PAL_NONE },
	{  0,  0,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_34[] = {
	{  2,  7,  0,  3,  3, 14, SPR_AIRPORT_JETWAY_2, PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_35[] = {
	{  3,  2,  0,  3,  3, 14, SPR_AIRPORT_JETWAY_3, PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_36[] = {
	{  0,  8,  0, 14,  3, 14, SPR_AIRPORT_PASSENGER_TUNNEL, PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_38[] = {
	{  0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_39[] = {
	{  7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_1, PAL_NONE },
	{ 15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_40[] = {
	{  7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_2, PAL_NONE },
	{ 15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_41[] = {
	{  7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_3, PAL_NONE },
	{ 15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_42[] = {
	{  7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_4, PAL_NONE },
	{ 15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_43[] = {
	{  7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_5, PAL_NONE },
	{ 15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_44[] = {
	{  7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_6, PAL_NONE },
	{ 15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_45[] = {
	{  7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_7, PAL_NONE },
	{ 15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_46[] = {
	{  7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_8, PAL_NONE },
	{ 15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_47[] = {
	{  7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_9, PAL_NONE },
	{ 15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_48[] = {
	{  7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_A, PAL_NONE },
	{ 15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_49[] = {
	{  7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_B, PAL_NONE },
	{ 15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_50[] = {
	{  7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_C, PAL_NONE },
	{ 15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_51[] = {
	{  7,  7,  0,  2,  2, 70, SPR_UNMOVABLE_TRANSMITTER, PAL_NONE },
	{  0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_54[] = {
	{  0,  0,  0, 15, 15, 30, SPR_AIRFIELD_TERM_C_BUILD | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_55[] = {
	{ 15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_58[] = {
	{  0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	{  4, 11,  0,  1,  1, 20, SPR_AIRFIELD_WIND_1 | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_59[] = {
	{  0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	{  4, 11,  0,  1,  1, 20, SPR_AIRFIELD_WIND_2 | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_60[] = {
	{  0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	{  4, 11,  0,  1,  1, 20, SPR_AIRFIELD_WIND_3 | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_61[] = {
	{  0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	{  4, 11,  0,  1,  1, 20, SPR_AIRFIELD_WIND_4 | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_62[] = {
	{  0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_63[] = {
	{  0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_64[] = {
	{  0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_65[] = {
	{ 14,  0,  0,  2, 16, 28, SPR_AIRFIELD_HANGAR_FRONT | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	{  0,  0,  0,  2, 16, 28, SPR_AIRFIELD_HANGAR_REAR, PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_66[] = {
	{  0,  0,  0, 16, 16, 60, SPR_HELIPORT | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_67[] = {
	{  0, 15,  0, 13,  1, 10, SPR_TRUCK_STOP_NE_BUILD_A | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	{ 13,  0,  0,  3, 16, 10, SPR_TRUCK_STOP_NE_BUILD_B | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	{  2,  0,  0, 11,  1, 10, SPR_TRUCK_STOP_NE_BUILD_C | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_68[] = {
	{ 15,  3,  0,  1, 13, 10, SPR_TRUCK_STOP_SE_BUILD_A | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	{  0,  0,  0, 16,  3, 10, SPR_TRUCK_STOP_SE_BUILD_B | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	{  0,  3,  0,  1, 11, 10, SPR_TRUCK_STOP_SE_BUILD_C | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_69[] = {
	{  3,  0,  0, 13,  1, 10, SPR_TRUCK_STOP_SW_BUILD_A | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	{  0,  0,  0,  3, 16, 10, SPR_TRUCK_STOP_SW_BUILD_B | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	{  3, 15,  0, 11,  1, 10, SPR_TRUCK_STOP_SW_BUILD_C | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_70[] = {
	{  0,  0,  0,  1, 13, 10, SPR_TRUCK_STOP_NW_BUILD_A | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	{  0, 13,  0, 16,  3, 10, SPR_TRUCK_STOP_NW_BUILD_B | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	{ 15,  2,  0,  1, 11, 10, SPR_TRUCK_STOP_NW_BUILD_C | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_71[] = {
	{  2,  0,  0, 11,  1, 10, SPR_BUS_STOP_NE_BUILD_A | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	{ 13,  0,  0,  3, 16, 10, SPR_BUS_STOP_NE_BUILD_B | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	{  0, 13,  0, 13,  3, 10, SPR_BUS_STOP_NE_BUILD_C | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_72[] = {
	{  0,  3,  0,  1, 11, 10, SPR_BUS_STOP_SE_BUILD_A | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	{  0,  0,  0, 16,  3, 10, SPR_BUS_STOP_SE_BUILD_B | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	{ 13,  3,  0,  3, 13, 10, SPR_BUS_STOP_SE_BUILD_C | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_73[] = {
	{  3, 15,  0, 11,  1, 10, SPR_BUS_STOP_SW_BUILD_A | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	{  0,  0,  0,  3, 16, 10, SPR_BUS_STOP_SW_BUILD_B | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	{  3,  0,  0, 13,  3, 10, SPR_BUS_STOP_SW_BUILD_C | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_74[] = {
	{ 15,  2,  0,  1, 11, 10, SPR_BUS_STOP_NW_BUILD_A | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	{  0, 13,  0, 16,  3, 10, SPR_BUS_STOP_NW_BUILD_B | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	{  0,  0,  0,  3, 13, 10, SPR_BUS_STOP_NW_BUILD_C | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_76[] = {
	{  0,  4,  0, 16,  8,  8, SPR_DOCK_SLOPE_NE, PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_77[] = {
	{  4,  0,  0,  8, 16,  8, SPR_DOCK_SLOPE_SE, PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_78[] = {
	{  0,  4,  0, 16,  8,  8, SPR_DOCK_SLOPE_SW, PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_79[] = {
	{  4,  0,  0,  8, 16,  8, SPR_DOCK_SLOPE_NW, PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_80[] = {
	{  0,  4,  0, 16,  8,  8, SPR_DOCK_FLAT_X, PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _station_display_datas_81[] = {
	{  4,  0,  0,  8, 16,  8, SPR_DOCK_FLAT_Y, PAL_NONE },
	TILE_SEQ_END()
};

/* Buoy, which will _always_ drown under the ship */
static const DrawTileSeqStruct _station_display_datas_82[] = {
	{  4,  -1,  0,  0,  0,  0, SPR_IMG_BOUY, PAL_NONE },
	TILE_SEQ_END()
};

// control tower with concrete underground and no fence
// concrete underground
static const DrawTileSeqStruct _station_display_datas_085[] = {
	{  3,  3,  0, 10, 10, 60, SPR_AIRPORT_TOWER | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },  // control tower
	TILE_SEQ_END()
};

// new airportdepot, facing west
// concrete underground
static const DrawTileSeqStruct _station_display_datas_086[] = {
	{ 14, 0,  0,  2, 16, 28, SPR_AIRFIELD_HANGAR_FRONT | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	{  0, 0,  0,  2, 16, 28, SPR_AIRFIELD_HANGAR_REAR, PAL_NONE },
	TILE_SEQ_END()
};

// asphalt tile with fences in north
// concrete underground
static const DrawTileSeqStruct _station_display_datas_087[] = {
	{  0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

// end of runway
static const DrawTileSeqStruct _station_display_datas_088[] = {
	{  0,  0,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences
	TILE_SEQ_END()
};

// runway tiles
static const DrawTileSeqStruct _station_display_datas_089[] = {
	{  0,  0,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences
	TILE_SEQ_END()
};

// turning radar with concrete underground fences on south -- needs 12 tiles
// concrete underground
//BEGIN
static const DrawTileSeqStruct _station_display_datas_090[] = {
	{  7, 7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_1, PAL_NONE },   // turning radar
	{ 15, 0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },  //fences
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_091[] = {
	{  7, 7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_2, PAL_NONE },
	{ 15, 0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_092[] = {
	{ 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_3, PAL_NONE },
	{ 15, 0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_093[] = {
	{ 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_4, PAL_NONE },
	{ 15, 0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_094[] = {
	{ 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_5, PAL_NONE },
	{ 15, 0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_095[] = {
	{ 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_6, PAL_NONE },
	{ 15, 0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_096[] = {
	{ 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_7, PAL_NONE },
	{ 15, 0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_097[] = {
	{ 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_8, PAL_NONE },
	{ 15, 0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_098[] = {
	{ 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_9, PAL_NONE },
	{ 15, 0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_099[] = {
	{ 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_A, PAL_NONE },
	{ 15, 0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_0100[] = {
	{ 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_B, PAL_NONE },
	{ 15, 0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_0101[] = {
	{ 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_C, PAL_NONE },
	{ 15, 0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};
//END

// turning radar with concrete underground fences on north -- needs 12 tiles
// concrete underground
//BEGIN
static const DrawTileSeqStruct _station_display_datas_0102[] = {
	{ 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_1, PAL_NONE },   // turning radar
	{ 0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_0103[] = {
	{ 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_2, PAL_NONE },
	{ 0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_0104[] = {
	{ 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_3, PAL_NONE },
	{ 0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_0105[] = {
	{ 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_4, PAL_NONE },
	{ 0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_0106[] = {
	{ 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_5, PAL_NONE },
	{ 0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_0107[] = {
	{ 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_6, PAL_NONE },
	{ 0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_0108[] = {
	{ 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_7, PAL_NONE },
	{ 0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_0109[] = {
	{ 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_8, PAL_NONE },
	{ 0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_0110[] = {
	{ 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_9, PAL_NONE },
	{ 0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_0111[] = {
	{ 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_A, PAL_NONE },
	{ 0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_0112[] = {
	{ 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_B, PAL_NONE },
	{ 0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_0113[] = {
	{ 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_C, PAL_NONE },
	{ 0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};
//END

// helipad for international airport
// concrete underground
static const DrawTileSeqStruct _station_display_datas_0114[] = {
	{ 10,  6,  0,  0,  0,  0, SPR_AIRPORT_HELIPAD, PAL_NONE },
	{ 15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences bottom
	TILE_SEQ_END()
};

// helipad for commuter airport
// concrete underground
static const DrawTileSeqStruct _station_display_datas_0115[] = {
	{ 10,  6,  0,  0,  0,  0, SPR_AIRPORT_HELIPAD, PAL_NONE },
	{  0,  0,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences left
	TILE_SEQ_END()
};

// helipad for continental airport
// concrete underground
static const DrawTileSeqStruct _station_display_datas_0116[] = {
	{ 10,  6,  0,  0,  0,  0, SPR_AIRPORT_HELIPAD, PAL_NONE },
	TILE_SEQ_END()
};

// asphalt tile with fences in north and south
// concrete underground
static const DrawTileSeqStruct _station_display_datas_0117[] = {
	{  0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	{ 15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

// runway tiles with 2 corner fences
static const DrawTileSeqStruct _station_display_datas_0118[] = {
	{ 15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences west
	{  0,  0,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences north
	TILE_SEQ_END()
};

// runway tiles with 2 corner fences
static const DrawTileSeqStruct _station_display_datas_0119[] = {
	{ 15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences west
	{  0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences south
	TILE_SEQ_END()
};

// runway tiles with 2 corner fences
static const DrawTileSeqStruct _station_display_datas_0120[] = {
	{  0,  0,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences north
	{  0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences east
	TILE_SEQ_END()
};

// runway tiles with 2 corner fences
static const DrawTileSeqStruct _station_display_datas_0121[] = {
	{  0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences east
	{  0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences south
	TILE_SEQ_END()
};

// ======== new 2x2 helidepot ========
// helipad tiles with 2 corner fences top+right
static const DrawTileSeqStruct _station_display_datas_0122[] = {
	{ 10,  6,  0,  0,  0,  0, SPR_AIRPORT_HELIPAD, PAL_NONE },
	{  0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences east
	{  0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences south
	TILE_SEQ_END()
};

// tarmac tiles with 2 corner fences bottom+right
static const DrawTileSeqStruct _station_display_datas_0123[] = {
	{ 15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences north
	{  0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences south
	TILE_SEQ_END()
};

// helidepot office with concrete underground and no fence
// concrete underground, fences top + left
static const DrawTileSeqStruct _station_display_datas_0124[] = {
	{  0,  0,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences left
	{  0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences east
	{  3,  3,  0, 10, 10, 60, SPR_AIRPORT_HELIDEPOT_OFFICE | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },  // helidepot office
	TILE_SEQ_END()
};

// N/S runway plain
static const DrawTileSeqStruct _station_display_datas_0125[] = {
	{  0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences east
	TILE_SEQ_END()
};

// N/S runway end
static const DrawTileSeqStruct _station_display_datas_0126[] = {
	{  0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences east
	TILE_SEQ_END()
};

// N/S runway plain
static const DrawTileSeqStruct _station_display_datas_0127[] = {
	{ 15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences bottom
	TILE_SEQ_END()
};

// N/S runway end
static const DrawTileSeqStruct _station_display_datas_0128[] = {
	{ 15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences bottom
	TILE_SEQ_END()
};

// West facing hangar
static const DrawTileSeqStruct _station_display_datas_0129[] = {
	{ 14,  0,  0,  2, 16, 28, SPR_NEWHANGAR_W | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	{  0,  0,  0,  2, 16, 28, SPR_NEWHANGAR_W_WALL, PAL_NONE },
	TILE_SEQ_END()
};

// North facing hangar
static const DrawTileSeqStruct _station_display_datas_0130[] = {
	{ 14,  0,  0,  2, 16, 28, SPR_NEWHANGAR_N | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

// East facing hangar
static const DrawTileSeqStruct _station_display_datas_0131[] = {
	{ 14,  0,  0,  2, 16, 28, SPR_NEWHANGAR_E | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },
	TILE_SEQ_END()
};

// helipad for district airport NS
// concrete underground
static const DrawTileSeqStruct _station_display_datas_0132[] = {
	{ 10,  6,  0,  0,  0,  0, SPR_AIRPORT_HELIPAD, PAL_NONE },
	{ 15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences bottom
	{  0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences right
	TILE_SEQ_END()
};

// helipad for district airport NS
// concrete underground
static const DrawTileSeqStruct _station_display_datas_0133[] = {
	{ 10,  6,  0,  0,  0,  0, SPR_AIRPORT_HELIPAD, PAL_NONE },
	{  0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences south
	TILE_SEQ_END()
};

// helidepot office with concrete underground and fence north
// concrete underground
static const DrawTileSeqStruct _station_display_datas_0134[] = {
	{  0,  0,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences north
	{  3,  3,  0, 10, 10, 60, SPR_AIRPORT_HELIDEPOT_OFFICE | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },  // helidepot office
	TILE_SEQ_END()
};

// helidepot office with concrete underground and fence east
// concrete underground
static const DrawTileSeqStruct _station_display_datas_0135[] = {
	{  0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences east
	{  3,  3,  0, 10, 10, 60, SPR_AIRPORT_HELIDEPOT_OFFICE | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },  // helidepot office
	TILE_SEQ_END()
};

// helidepot office with concrete underground and fence west
// concrete underground
static const DrawTileSeqStruct _station_display_datas_0136[] = {
	{ 15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences west
	{  3,  3,  0, 10, 10, 60, SPR_AIRPORT_HELIDEPOT_OFFICE | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },  // helidepot office
	TILE_SEQ_END()
};

// helidepot office with concrete underground and fence south
// concrete underground
static const DrawTileSeqStruct _station_display_datas_0137[] = {
	{  0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences south
	{  3,  3,  0, 10, 10, 60, SPR_AIRPORT_HELIDEPOT_OFFICE | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },  // helidepot office
	TILE_SEQ_END()
};

// terminal with fence to east
static const DrawTileSeqStruct _station_display_datas_0138[] = {
	{  0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences east
	TILE_SEQ_END()
};

// terminal with fence to south
static const DrawTileSeqStruct _station_display_datas_0139[] = {
	{  0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences south
	TILE_SEQ_END()
};

// terminal with fence to north
static const DrawTileSeqStruct _station_display_datas_0140[] = {
	{ 15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences west
	TILE_SEQ_END()
};

// concrete with fence to east
static const DrawTileSeqStruct _station_display_datas_0141[] = {
	{  0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences east
	TILE_SEQ_END()
};

// concrete with fence to south
static const DrawTileSeqStruct _station_display_datas_0142[] = {
	{  0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences south
	TILE_SEQ_END()
};

// helipad for district airport EW
// concrete underground
static const DrawTileSeqStruct _station_display_datas_0143[] = {
	{ 10,  6,  0,  0,  0,  0, SPR_AIRPORT_HELIPAD, PAL_NONE },
	{ 15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences west
	{  0,  0,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences north
	TILE_SEQ_END()
};

// helipad for district airport EW
// concrete underground
static const DrawTileSeqStruct _station_display_datas_0144[] = {
	{ 10,  6,  0,  0,  0,  0, SPR_AIRPORT_HELIPAD, PAL_NONE },
	{ 15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences west
	TILE_SEQ_END()
};

// turning radar with concrete underground fences on south -- needs 12 tiles
// concrete underground
//BEGIN
static const DrawTileSeqStruct _station_display_datas_0145[] = {
	{ 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_1, PAL_NONE },   // turning radar
	{  0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences south
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_0146[] = {
	{ 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_2, PAL_NONE },
	{  0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences south
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_0147[] = {
	{ 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_3, PAL_NONE },
	{  0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences south
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_0148[] = {
	{ 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_4, PAL_NONE },
	{  0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences south
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_0149[] = {
	{ 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_5, PAL_NONE },
	{  0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences south
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_0150[] = {
	{ 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_6, PAL_NONE },
	{  0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences south
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_0151[] = {
	{ 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_7, PAL_NONE },
	{  0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences south
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_0152[] = {
	{ 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_8, PAL_NONE },
	{  0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences south
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_0153[] = {
	{ 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_9, PAL_NONE },
	{  0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences south
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_0154[] = {
	{ 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_A, PAL_NONE },
	{  0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences south
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_0155[] = {
	{ 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_B, PAL_NONE },
	{  0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences south
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_0156[] = {
	{ 7,  7,  0,  2,  2,  8, SPR_AIRPORT_RADAR_C, PAL_NONE },
	{  0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences south
	TILE_SEQ_END()
};
//END

// helipad for helistation
// concrete underground
static const DrawTileSeqStruct _station_display_datas_0157[] = {
	{  0,  1,  2,  0,  0,  0, SPR_NEWHELIPAD, PAL_NONE },
	{ 15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences west
	{  0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences south
	TILE_SEQ_END()
};

// helipad for helistation
// concrete underground
static const DrawTileSeqStruct _station_display_datas_0158[] = {
	{  0,  1,  2,  0,  0,  0, SPR_NEWHELIPAD, PAL_NONE },
	{ 15,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences west
	{  0,  0,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences north
	TILE_SEQ_END()
};

// helipad for helistation
// concrete underground
static const DrawTileSeqStruct _station_display_datas_0159[] = {
	{  0,  1,  2,  0,  0,  0, SPR_NEWHELIPAD, PAL_NONE },
	{  0,  0,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences north
	TILE_SEQ_END()
};

// helidepot office with concrete underground - no fence
static const DrawTileSeqStruct _station_display_datas_0160[] = {
	{  3,  3,  0, 10, 10, 60, SPR_AIRPORT_HELIDEPOT_OFFICE | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE },  // helidepot office
	TILE_SEQ_END()
};

// concrete underground
static const DrawTileSeqStruct _station_display_datas_0161[] = {
	{  0,  0,  0,  1, 16,  6, SPR_AIRPORT_FENCE_Y | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences east
	{  0, 15,  0, 16,  1,  6, SPR_AIRPORT_FENCE_X | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE }, // fences south
	TILE_SEQ_END()
};

// half grass half SPR_AIRPORT_APRON
static const DrawTileSeqStruct _station_display_datas_0162[] = {
	{ 0,  0,  0,  0,  0,  0, SPR_GRASS_LEFT, PAL_NONE },
	TILE_SEQ_END()
};

// half grass half SPR_AIRPORT_APRON
static const DrawTileSeqStruct _station_display_datas_0163[] = {
	{ 0,  0,  0,  0,  0,  0, SPR_GRASS_RIGHT, PAL_NONE },
	TILE_SEQ_END()
};

static const DrawTileSprites _station_display_datas[] = {
	{ SPR_RAIL_TRACK_X,               PAL_NONE, _station_display_datas_0 },
	{ SPR_RAIL_TRACK_Y,               PAL_NONE, _station_display_datas_1 },
	{ SPR_RAIL_TRACK_X,               PAL_NONE, _station_display_datas_2 },
	{ SPR_RAIL_TRACK_Y,               PAL_NONE, _station_display_datas_3 },
	{ SPR_RAIL_TRACK_X,               PAL_NONE, _station_display_datas_4 },
	{ SPR_RAIL_TRACK_Y,               PAL_NONE, _station_display_datas_5 },
	{ SPR_RAIL_TRACK_X,               PAL_NONE, _station_display_datas_6 },
	{ SPR_RAIL_TRACK_Y,               PAL_NONE, _station_display_datas_7 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_nothing },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_9 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_10 },
	{ SPR_AIRPORT_AIRCRAFT_STAND,     PAL_NONE, _station_display_nothing },
	{ SPR_AIRPORT_TAXIWAY_NS_WEST,    PAL_NONE, _station_display_nothing },
	{ SPR_AIRPORT_TAXIWAY_EW_SOUTH,   PAL_NONE, _station_display_nothing },
	{ SPR_AIRPORT_TAXIWAY_XING_SOUTH, PAL_NONE, _station_display_nothing },
	{ SPR_AIRPORT_TAXIWAY_XING_WEST,  PAL_NONE, _station_display_nothing },
	{ SPR_AIRPORT_TAXIWAY_NS_CTR,     PAL_NONE, _station_display_nothing },
	{ SPR_AIRPORT_TAXIWAY_XING_EAST,  PAL_NONE, _station_display_nothing },
	{ SPR_AIRPORT_TAXIWAY_NS_EAST,    PAL_NONE, _station_display_nothing },
	{ SPR_AIRPORT_TAXIWAY_EW_NORTH,   PAL_NONE, _station_display_nothing },
	{ SPR_AIRPORT_TAXIWAY_EW_CTR,     PAL_NONE, _station_display_nothing },
	{ SPR_AIRPORT_TAXIWAY_EW_NORTH,   PAL_NONE, _station_display_datas_21 },
	{ SPR_AIRPORT_RUNWAY_EXIT_A,      PAL_NONE, _station_display_datas_22 },
	{ SPR_AIRPORT_RUNWAY_EXIT_B,      PAL_NONE, _station_display_datas_23 },
	{ SPR_AIRPORT_RUNWAY_EXIT_C,      PAL_NONE, _station_display_datas_24 },
	{ SPR_AIRPORT_RUNWAY_EXIT_D,      PAL_NONE, _station_display_datas_25 },
	{ SPR_AIRPORT_RUNWAY_END,         PAL_NONE, _station_display_datas_26 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_27 },
	{ SPR_FLAT_GRASS_TILE,            PAL_NONE, _station_display_datas_28 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_29 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_30 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_31 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_32 },
	{ SPR_AIRPORT_AIRCRAFT_STAND,     PAL_NONE, _station_display_datas_33 },
	{ SPR_AIRPORT_AIRCRAFT_STAND,     PAL_NONE, _station_display_datas_34 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_35 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_36 },
	{ SPR_FLAT_GRASS_TILE,            PAL_NONE, _station_display_nothing },
	{ SPR_FLAT_GRASS_TILE,            PAL_NONE, _station_display_datas_38 },
	{ SPR_FLAT_GRASS_TILE,            PAL_NONE, _station_display_datas_39 },
	{ SPR_FLAT_GRASS_TILE,            PAL_NONE, _station_display_datas_40 },
	{ SPR_FLAT_GRASS_TILE,            PAL_NONE, _station_display_datas_41 },
	{ SPR_FLAT_GRASS_TILE,            PAL_NONE, _station_display_datas_42 },
	{ SPR_FLAT_GRASS_TILE,            PAL_NONE, _station_display_datas_43 },
	{ SPR_FLAT_GRASS_TILE,            PAL_NONE, _station_display_datas_44 },
	{ SPR_FLAT_GRASS_TILE,            PAL_NONE, _station_display_datas_45 },
	{ SPR_FLAT_GRASS_TILE,            PAL_NONE, _station_display_datas_46 },
	{ SPR_FLAT_GRASS_TILE,            PAL_NONE, _station_display_datas_47 },
	{ SPR_FLAT_GRASS_TILE,            PAL_NONE, _station_display_datas_48 },
	{ SPR_FLAT_GRASS_TILE,            PAL_NONE, _station_display_datas_49 },
	{ SPR_FLAT_GRASS_TILE,            PAL_NONE, _station_display_datas_50 },
	{ SPR_FLAT_GRASS_TILE,            PAL_NONE, _station_display_datas_51 },
	{ SPR_AIRFIELD_TERM_A,            PAL_NONE, _station_display_nothing },
	{ SPR_AIRFIELD_TERM_B,            PAL_NONE, _station_display_nothing },
	{ SPR_AIRFIELD_TERM_C_GROUND | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE, _station_display_datas_54 },
	{ SPR_AIRFIELD_APRON_A,           PAL_NONE, _station_display_datas_55 },
	{ SPR_AIRFIELD_APRON_B,           PAL_NONE, _station_display_nothing },
	{ SPR_AIRFIELD_APRON_C,           PAL_NONE, _station_display_nothing },
	{ SPR_AIRFIELD_APRON_D,           PAL_NONE, _station_display_datas_58 },
	{ SPR_AIRFIELD_APRON_D,           PAL_NONE, _station_display_datas_59 },
	{ SPR_AIRFIELD_APRON_D,           PAL_NONE, _station_display_datas_60 },
	{ SPR_AIRFIELD_APRON_D,           PAL_NONE, _station_display_datas_61 },
	{ SPR_AIRFIELD_RUNWAY_NEAR_END,   PAL_NONE, _station_display_datas_62 },
	{ SPR_AIRFIELD_RUNWAY_MIDDLE,     PAL_NONE, _station_display_datas_63 },
	{ SPR_AIRFIELD_RUNWAY_FAR_END,    PAL_NONE, _station_display_datas_64 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_65 },
	{ SPR_FLAT_GRASS_TILE,            PAL_NONE, _station_display_datas_66 },
	{ SPR_TRUCK_STOP_NE_GROUND | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE, _station_display_datas_67 },
	{ SPR_TRUCK_STOP_SE_GROUND | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE, _station_display_datas_68 },
	{ SPR_TRUCK_STOP_SW_GROUND | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE, _station_display_datas_69 },
	{ SPR_TRUCK_STOP_NW_GROUND | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE, _station_display_datas_70 },
	{ SPR_BUS_STOP_NE_GROUND   | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE, _station_display_datas_71 },
	{ SPR_BUS_STOP_SE_GROUND   | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE, _station_display_datas_72 },
	{ SPR_BUS_STOP_SW_GROUND   | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE, _station_display_datas_73 },
	{ SPR_BUS_STOP_NW_GROUND   | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE, _station_display_datas_74 },
	{ SPR_FLAT_WATER_TILE,            PAL_NONE, _station_display_nothing },
	{ SPR_SHORE_TILEH_3,              PAL_NONE, _station_display_datas_76 },
	{ SPR_SHORE_TILEH_9,              PAL_NONE, _station_display_datas_77 },
	{ SPR_SHORE_TILEH_12,             PAL_NONE, _station_display_datas_78 },
	{ SPR_SHORE_TILEH_6,              PAL_NONE, _station_display_datas_79 },
	{ SPR_FLAT_WATER_TILE,            PAL_NONE, _station_display_datas_80 },
	{ SPR_FLAT_WATER_TILE,            PAL_NONE, _station_display_datas_81 },
	{ SPR_FLAT_WATER_TILE,            PAL_NONE, _station_display_datas_82 },
	{ SPR_AIRPORT_RUNWAY_END,         PAL_NONE, _station_display_nothing },
	{ SPR_AIRPORT_RUNWAY_EXIT_B,      PAL_NONE, _station_display_nothing },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_085 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_086 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_087 },
	{ SPR_AIRPORT_RUNWAY_END,         PAL_NONE, _station_display_datas_088 },
	{ SPR_AIRPORT_RUNWAY_EXIT_B,      PAL_NONE, _station_display_datas_089 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_090 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_091 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_092 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_093 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_094 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_095 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_096 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_097 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_098 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_099 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_0100 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_0101 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_0102 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_0103 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_0104 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_0105 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_0106 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_0107 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_0108 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_0109 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_0110 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_0111 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_0112 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_0113 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_0114 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_0115 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_0116 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_0117 },
	{ SPR_AIRPORT_RUNWAY_END,         PAL_NONE, _station_display_datas_0118 },
	{ SPR_AIRPORT_RUNWAY_END,         PAL_NONE, _station_display_datas_0119 },
	{ SPR_AIRPORT_RUNWAY_END,         PAL_NONE, _station_display_datas_0120 },
	{ SPR_AIRPORT_RUNWAY_END,         PAL_NONE, _station_display_datas_0121 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_0122 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_0123 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_0124 },
	{ SPR_NSRUNWAY1,                  PAL_NONE, _station_display_datas_0125 },
	{ SPR_NSRUNWAY_END,               PAL_NONE, _station_display_datas_0126 },
	{ SPR_NSRUNWAY1,                  PAL_NONE, _station_display_datas_0127 },
	{ SPR_NSRUNWAY_END,               PAL_NONE, _station_display_datas_0128 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_0129 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_0130 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_0131 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_0132 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_0133 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_0134 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_0135 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_0136 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_0137 },
	{ SPR_AIRPORT_AIRCRAFT_STAND,     PAL_NONE, _station_display_datas_0138 },
	{ SPR_AIRPORT_AIRCRAFT_STAND,     PAL_NONE, _station_display_datas_0139 },
	{ SPR_AIRPORT_AIRCRAFT_STAND,     PAL_NONE, _station_display_datas_0140 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_0141 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_0142 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_0143 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_0144 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_0145 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_0146 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_0147 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_0148 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_0149 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_0150 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_0151 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_0152 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_0153 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_0154 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_0155 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_0156 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_0157 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_0158 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_0159 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_0160 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_0161 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_0162 },
	{ SPR_AIRPORT_APRON,              PAL_NONE, _station_display_datas_0163 },
	{ SPR_FLAT_GRASS_TILE,            PAL_NONE, _station_display_datas_58 },
	{ SPR_FLAT_GRASS_TILE,            PAL_NONE, _station_display_datas_59 },
	{ SPR_FLAT_GRASS_TILE,            PAL_NONE, _station_display_datas_60 },
	{ SPR_FLAT_GRASS_TILE,            PAL_NONE, _station_display_datas_61 },
};
