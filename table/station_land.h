#define TILE_SEQ_BEGIN(x) ADD_DWORD(x),
#define TILE_SEQ_LINE(a,b,c,d,e,f,g) a,b,c,d,e,f,ADD_WORD(g),
#define TILE_SEQ_SPEC(a,b,g) a,b,0x80,0,ADD_DWORD(g),
#define TILE_SEQ_END() 0x80

static const byte _station_display_datas_0[] = {
	TILE_SEQ_BEGIN(0x3F4)
	TILE_SEQ_LINE(  0,  0,  0, 16,  5,  2, 0x842E)
	TILE_SEQ_LINE(  0, 11,  0, 16,  5,  2, 0x8430)
	TILE_SEQ_END()
};

static const byte _station_display_datas_1[] = {
	TILE_SEQ_BEGIN(0x3F3)
	TILE_SEQ_LINE(  0,  0,  0,  5, 16,  2, 0x842F)
	TILE_SEQ_LINE( 11,  0,  0,  5, 16,  2, 0x842D)
	TILE_SEQ_END()
};

static const byte _station_display_datas_2[] = {
	TILE_SEQ_BEGIN(0x3F4)
	TILE_SEQ_LINE(  0,  0,  0, 16,  5,  2, 0x8431)
	TILE_SEQ_LINE(  0, 11,  0, 16,  5,  2, 0x8430)
	TILE_SEQ_END()
};

static const byte _station_display_datas_3[] = {
	TILE_SEQ_BEGIN(0x3F3)
	TILE_SEQ_LINE(  0,  0,  0,  5, 16,  2, 0x8432)
	TILE_SEQ_LINE( 11,  0,  0,  5, 16,  2, 0x842D)
	TILE_SEQ_END()
};

static const byte _station_display_datas_4[] = {
	TILE_SEQ_BEGIN(0x3F4)
	TILE_SEQ_LINE(  0,  0,  0, 16,  5,  7, 0x8434)
	TILE_SEQ_LINE(  0, 11,  0, 16,  5,  2, 0x8430)
	TILE_SEQ_LINE(  0,  0, 16, 16, 16, 10, 0x8437)
	TILE_SEQ_SPEC(  0,  0, 0x322443B)
	TILE_SEQ_END()
};

static const byte _station_display_datas_5[] = {
	TILE_SEQ_BEGIN(0x3F3)
	TILE_SEQ_LINE(  0,  0,  0,  5, 16,  2, 0x8435)
	TILE_SEQ_LINE( 11,  0,  0,  5, 16,  2, 0x842D)
	TILE_SEQ_LINE(  0,  0, 16, 16, 16, 10, 0x8438)
	TILE_SEQ_SPEC(  0,  0, 0x322443C)
	TILE_SEQ_END()
};

static const byte _station_display_datas_6[] = {
	TILE_SEQ_BEGIN(0x3F4)
	TILE_SEQ_LINE(  0,  0,  0, 16,  5,  2, 0x842E)
	TILE_SEQ_LINE(  0, 11,  0, 16,  5,  2, 0x8436)
	TILE_SEQ_LINE(  0,  0, 16, 16, 16, 10, 0x8439)
	TILE_SEQ_SPEC(  0,  0, 0x322443D)
	TILE_SEQ_END()
};

static const byte _station_display_datas_7[] = {
	TILE_SEQ_BEGIN(0x3F3)
	TILE_SEQ_LINE(  0,  0,  0,  5, 16,  2, 0x842F)
	TILE_SEQ_LINE( 11,  0,  0,  5, 16,  2, 0x8433)
	TILE_SEQ_LINE(  0,  0, 16, 16, 16, 10, 0x843A)
	TILE_SEQ_SPEC(  0,  0, 0x322443E)
	TILE_SEQ_END()
};

static const byte _station_display_datas_8[] = {
	TILE_SEQ_BEGIN(0xA4A)
	TILE_SEQ_END()
};

static const byte _station_display_datas_9[] = {
	TILE_SEQ_BEGIN(0xA4A)
	TILE_SEQ_LINE(  0,  0,  0, 16,  1,  6, 0x8A68)
	TILE_SEQ_END()
};

static const byte _station_display_datas_10[] = {
	TILE_SEQ_BEGIN(0xA4A)
	TILE_SEQ_LINE( 15,  0,  0,  1, 16,  6, 0x8A67)
	TILE_SEQ_END()
};

static const byte _station_display_datas_11[] = {
	TILE_SEQ_BEGIN(0xA4B)
	TILE_SEQ_END()
};

static const byte _station_display_datas_12[] = {
	TILE_SEQ_BEGIN(0xA4C)
	TILE_SEQ_END()
};

static const byte _station_display_datas_13[] = {
	TILE_SEQ_BEGIN(0xA4D)
	TILE_SEQ_END()
};

static const byte _station_display_datas_14[] = {
	TILE_SEQ_BEGIN(0xA4E)
	TILE_SEQ_END()
};

static const byte _station_display_datas_15[] = {
	TILE_SEQ_BEGIN(0xA4F)
	TILE_SEQ_END()
};

static const byte _station_display_datas_16[] = {
	TILE_SEQ_BEGIN(0xA50)
	TILE_SEQ_END()
};

static const byte _station_display_datas_17[] = {
	TILE_SEQ_BEGIN(0xA51)
	TILE_SEQ_END()
};

static const byte _station_display_datas_18[] = {
	TILE_SEQ_BEGIN(0xA52)
	TILE_SEQ_END()
};

static const byte _station_display_datas_19[] = {
	TILE_SEQ_BEGIN(0xA53)
	TILE_SEQ_END()
};

static const byte _station_display_datas_20[] = {
	TILE_SEQ_BEGIN(0xA54)
	TILE_SEQ_END()
};

static const byte _station_display_datas_21[] = {
	TILE_SEQ_BEGIN(0xA53)
	TILE_SEQ_LINE( 15,  0,  0,  1, 16,  6, 0x8A67)
	TILE_SEQ_END()
};

static const byte _station_display_datas_22[] = {
	TILE_SEQ_BEGIN(0xA55)
	TILE_SEQ_LINE(  0, 15,  0, 16,  1,  6, 0x8A68)
	TILE_SEQ_END()
};

static const byte _station_display_datas_23[] = {
	TILE_SEQ_BEGIN(0xA56)
	TILE_SEQ_LINE(  0, 15,  0, 16,  1,  6, 0x8A68)
	TILE_SEQ_END()
};

static const byte _station_display_datas_24[] = {
	TILE_SEQ_BEGIN(0xA57)
	TILE_SEQ_LINE(  0, 15,  0, 16,  1,  6, 0x8A68)
	TILE_SEQ_END()
};

static const byte _station_display_datas_25[] = {
	TILE_SEQ_BEGIN(0xA58)
	TILE_SEQ_LINE(  0, 15,  0, 16,  1,  6, 0x8A68)
	TILE_SEQ_END()
};

static const byte _station_display_datas_26[] = {
	TILE_SEQ_BEGIN(0xA59)
	TILE_SEQ_LINE(  0, 15,  0, 16,  1,  6, 0x8A68)
	TILE_SEQ_END()
};

static const byte _station_display_datas_27[] = {
	TILE_SEQ_BEGIN(0xA4A)
	TILE_SEQ_LINE(  2,  0,  0, 11, 16, 40, 0x8A5A)
	TILE_SEQ_END()
};

static const byte _station_display_datas_28[] = {
	TILE_SEQ_BEGIN(0xF8D)
	TILE_SEQ_LINE(  3,  3,  0, 10, 10, 60, 0x8A5B)
	TILE_SEQ_LINE( 15,  0,  0,  1, 16,  6, 0x8A67)
	TILE_SEQ_END()
};

static const byte _station_display_datas_29[] = {
	TILE_SEQ_BEGIN(0xA4A)
	TILE_SEQ_LINE(  0,  1,  0, 14, 14, 30, 0x8A5C)
	TILE_SEQ_END()
};

static const byte _station_display_datas_30[] = {
	TILE_SEQ_BEGIN(0xA4A)
	TILE_SEQ_LINE(  3,  3,  0, 10, 11, 35, 0x8A5D)
	TILE_SEQ_END()
};

static const byte _station_display_datas_31[] = {
	TILE_SEQ_BEGIN(0xA4A)
	TILE_SEQ_LINE(  0,  3,  0, 16, 11, 40, 0x8A5E)
	TILE_SEQ_END()
};

static const byte _station_display_datas_32[] = {
	TILE_SEQ_BEGIN(0xA4A)
	TILE_SEQ_LINE( 14,  0,  0,  2, 16, 28, 0x8A5F)
	TILE_SEQ_LINE(  0,  0,  0,  2, 16, 28, 0xA60)
	TILE_SEQ_END()
};

static const byte _station_display_datas_33[] = {
	TILE_SEQ_BEGIN(0xA4B)
	TILE_SEQ_LINE(  7, 11,  0,  3,  3, 14, 0xA63)
	TILE_SEQ_LINE(  0,  0,  0, 16,  1,  6, 0x8A68)
	TILE_SEQ_END()
};

static const byte _station_display_datas_34[] = {
	TILE_SEQ_BEGIN(0xA4B)
	TILE_SEQ_LINE(  2,  7,  0,  3,  3, 14, 0xA64)
	TILE_SEQ_END()
};

static const byte _station_display_datas_35[] = {
	TILE_SEQ_BEGIN(0xA4A)
	TILE_SEQ_LINE(  3,  2,  0,  3,  3, 14, 0xA65)
	TILE_SEQ_END()
};

static const byte _station_display_datas_36[] = {
	TILE_SEQ_BEGIN(0xA4A)
	TILE_SEQ_LINE(  0,  8,  0, 14,  3, 14, 0xA66)
	TILE_SEQ_END()
};

static const byte _station_display_datas_37[] = {
	TILE_SEQ_BEGIN(0xF8D)
	TILE_SEQ_END()
};

static const byte _station_display_datas_38[] = {
	TILE_SEQ_BEGIN(0xF8D)
	TILE_SEQ_LINE(  0,  0,  0,  1, 16,  6, 0x8A67)
	TILE_SEQ_END()
};

static const byte _station_display_datas_39[] = {
	TILE_SEQ_BEGIN(0xF8D)
	TILE_SEQ_LINE(  7,  7,  0,  2,  2,  8, 0xA78)
	TILE_SEQ_LINE( 15,  0,  0,  1, 16,  6, 0x8A67)
	TILE_SEQ_END()
};

static const byte _station_display_datas_40[] = {
	TILE_SEQ_BEGIN(0xF8D)
	TILE_SEQ_LINE(  7,  7,  0,  2,  2,  8, 0xA79)
	TILE_SEQ_LINE( 15,  0,  0,  1, 16,  6, 0x8A67)
	TILE_SEQ_END()
};

static const byte _station_display_datas_41[] = {
	TILE_SEQ_BEGIN(0xF8D)
	TILE_SEQ_LINE(  7,  7,  0,  2,  2,  8, 0xA7A)
	TILE_SEQ_LINE( 15,  0,  0,  1, 16,  6, 0x8A67)
	TILE_SEQ_END()
};

static const byte _station_display_datas_42[] = {
	TILE_SEQ_BEGIN(0xF8D)
	TILE_SEQ_LINE(  7,  7,  0,  2,  2,  8, 0xA7B)
	TILE_SEQ_LINE( 15,  0,  0,  1, 16,  6, 0x8A67)
	TILE_SEQ_END()
};

static const byte _station_display_datas_43[] = {
	TILE_SEQ_BEGIN(0xF8D)
	TILE_SEQ_LINE(  7,  7,  0,  2,  2,  8, 0xA7C)
	TILE_SEQ_LINE( 15,  0,  0,  1, 16,  6, 0x8A67)
	TILE_SEQ_END()
};

static const byte _station_display_datas_44[] = {
	TILE_SEQ_BEGIN(0xF8D)
	TILE_SEQ_LINE(  7,  7,  0,  2,  2,  8, 0xA7D)
	TILE_SEQ_LINE( 15,  0,  0,  1, 16,  6, 0x8A67)
	TILE_SEQ_END()
};

static const byte _station_display_datas_45[] = {
	TILE_SEQ_BEGIN(0xF8D)
	TILE_SEQ_LINE(  7,  7,  0,  2,  2,  8, 0xA7E)
	TILE_SEQ_LINE( 15,  0,  0,  1, 16,  6, 0x8A67)
	TILE_SEQ_END()
};

static const byte _station_display_datas_46[] = {
	TILE_SEQ_BEGIN(0xF8D)
	TILE_SEQ_LINE(  7,  7,  0,  2,  2,  8, 0xA7F)
	TILE_SEQ_LINE( 15,  0,  0,  1, 16,  6, 0x8A67)
	TILE_SEQ_END()
};

static const byte _station_display_datas_47[] = {
	TILE_SEQ_BEGIN(0xF8D)
	TILE_SEQ_LINE(  7,  7,  0,  2,  2,  8, 0xA80)
	TILE_SEQ_LINE( 15,  0,  0,  1, 16,  6, 0x8A67)
	TILE_SEQ_END()
};

static const byte _station_display_datas_48[] = {
	TILE_SEQ_BEGIN(0xF8D)
	TILE_SEQ_LINE(  7,  7,  0,  2,  2,  8, 0xA81)
	TILE_SEQ_LINE( 15,  0,  0,  1, 16,  6, 0x8A67)
	TILE_SEQ_END()
};

static const byte _station_display_datas_49[] = {
	TILE_SEQ_BEGIN(0xF8D)
	TILE_SEQ_LINE(  7,  7,  0,  2,  2,  8, 0xA82)
	TILE_SEQ_LINE( 15,  0,  0,  1, 16,  6, 0x8A67)
	TILE_SEQ_END()
};

static const byte _station_display_datas_50[] = {
	TILE_SEQ_BEGIN(0xF8D)
	TILE_SEQ_LINE(  7,  7,  0,  2,  2,  8, 0xA83)
	TILE_SEQ_LINE( 15,  0,  0,  1, 16,  6, 0x8A67)
	TILE_SEQ_END()
};

static const byte _station_display_datas_51[] = {
	TILE_SEQ_BEGIN(0xF8D)
	TILE_SEQ_LINE(  7,  7,  0,  2,  2, 70, 0xA29)
	TILE_SEQ_LINE(  0,  0,  0,  1, 16,  6, 0x8A67)
	TILE_SEQ_END()
};

static const byte _station_display_datas_52[] = {
	TILE_SEQ_BEGIN(0xA69)
	TILE_SEQ_END()
};

static const byte _station_display_datas_53[] = {
	TILE_SEQ_BEGIN(0xA6A)
	TILE_SEQ_END()
};

static const byte _station_display_datas_54[] = {
	TILE_SEQ_BEGIN(0x8A6B)
	TILE_SEQ_LINE(  0,  0,  0, 15, 15, 30, 0x8A6C)
	TILE_SEQ_END()
};

static const byte _station_display_datas_55[] = {
	TILE_SEQ_BEGIN(0xA6D)
	TILE_SEQ_LINE( 15,  0,  0,  1, 16,  6, 0x8A67)
	TILE_SEQ_END()
};

static const byte _station_display_datas_56[] = {
	TILE_SEQ_BEGIN(0xA6E)
	TILE_SEQ_END()
};

static const byte _station_display_datas_57[] = {
	TILE_SEQ_BEGIN(0xA6F)
	TILE_SEQ_END()
};

static const byte _station_display_datas_58[] = {
	TILE_SEQ_BEGIN(0xA70)
	TILE_SEQ_LINE(  0,  0,  0,  1, 16,  6, 0x8A67)
	TILE_SEQ_LINE(  4, 11,  0,  1,  1, 20, 0x8A74)
	TILE_SEQ_END()
};

static const byte _station_display_datas_59[] = {
	TILE_SEQ_BEGIN(0xA70)
	TILE_SEQ_LINE(  0,  0,  0,  1, 16,  6, 0x8A67)
	TILE_SEQ_LINE(  4, 11,  0,  1,  1, 20, 0x8A75)
	TILE_SEQ_END()
};

static const byte _station_display_datas_60[] = {
	TILE_SEQ_BEGIN(0xA70)
	TILE_SEQ_LINE(  0,  0,  0,  1, 16,  6, 0x8A67)
	TILE_SEQ_LINE(  4, 11,  0,  1,  1, 20, 0x8A76)
	TILE_SEQ_END()
};

static const byte _station_display_datas_61[] = {
	TILE_SEQ_BEGIN(0xA70)
	TILE_SEQ_LINE(  0,  0,  0,  1, 16,  6, 0x8A67)
	TILE_SEQ_LINE(  4, 11,  0,  1,  1, 20, 0x8A77)
	TILE_SEQ_END()
};

static const byte _station_display_datas_62[] = {
	TILE_SEQ_BEGIN(0xA71)
	TILE_SEQ_LINE(  0, 15,  0, 16,  1,  6, 0x8A68)
	TILE_SEQ_END()
};

static const byte _station_display_datas_63[] = {
	TILE_SEQ_BEGIN(0xA72)
	TILE_SEQ_LINE(  0, 15,  0, 16,  1,  6, 0x8A68)
	TILE_SEQ_END()
};

static const byte _station_display_datas_64[] = {
	TILE_SEQ_BEGIN(0xA73)
	TILE_SEQ_LINE(  0, 15,  0, 16,  1,  6, 0x8A68)
	TILE_SEQ_END()
};

static const byte _station_display_datas_65[] = {
	TILE_SEQ_BEGIN(0xA4A)
	TILE_SEQ_LINE( 14,  0,  0,  2, 16, 28, 0x8A61)
	TILE_SEQ_LINE(  0,  0,  0,  2, 16, 28, 0xA62)
	TILE_SEQ_END()
};

static const byte _station_display_datas_66[] = {
	TILE_SEQ_BEGIN(0xF8D)
	TILE_SEQ_LINE(  0,  0,  0, 16, 16, 60, 0x8A49)
	TILE_SEQ_END()
};

static const byte _station_display_datas_67[] = {
	TILE_SEQ_BEGIN(0x8A94)
	TILE_SEQ_LINE(  0, 15,  0, 13,  1, 10, 0x8A98)
	TILE_SEQ_LINE( 13,  0,  0,  3, 16, 10, 0x8A9C)
	TILE_SEQ_LINE(  2,  0,  0, 11,  1, 10, 0x8AA0)
	TILE_SEQ_END()
};

static const byte _station_display_datas_68[] = {
	TILE_SEQ_BEGIN(0x8A95)
	TILE_SEQ_LINE( 15,  3,  0,  1, 13, 10, 0x8A99)
	TILE_SEQ_LINE(  0,  0,  0, 16,  3, 10, 0x8A9D)
	TILE_SEQ_LINE(  0,  3,  0,  1, 11, 10, 0x8AA1)
	TILE_SEQ_END()
};

static const byte _station_display_datas_69[] = {
	TILE_SEQ_BEGIN(0x8A96)
	TILE_SEQ_LINE(  3,  0,  0, 13,  1, 10, 0x8A9A)
	TILE_SEQ_LINE(  0,  0,  0,  3, 16, 10, 0x8A9E)
	TILE_SEQ_LINE(  3, 15,  0, 11,  1, 10, 0x8AA2)
	TILE_SEQ_END()
};

static const byte _station_display_datas_70[] = {
	TILE_SEQ_BEGIN(0x8A97)
	TILE_SEQ_LINE(  0,  0,  0,  1, 13, 10, 0x8A9B)
	TILE_SEQ_LINE(  0, 13,  0, 16,  3, 10, 0x8A9F)
	TILE_SEQ_LINE( 15,  2,  0,  1, 11, 10, 0x8AA3)
	TILE_SEQ_END()
};

static const byte _station_display_datas_71[] = {
	TILE_SEQ_BEGIN(0x8A84)
	TILE_SEQ_LINE(  2,  0,  0, 11,  1, 10, 0x8A88)
	TILE_SEQ_LINE( 13,  0,  0,  3, 16, 10, 0x8A8C)
	TILE_SEQ_LINE(  0, 13,  0, 13,  3, 10, 0x8A90)
	TILE_SEQ_END()
};

static const byte _station_display_datas_72[] = {
	TILE_SEQ_BEGIN(0x8A85)
	TILE_SEQ_LINE(  0,  3,  0,  1, 11, 10, 0x8A89)
	TILE_SEQ_LINE(  0,  0,  0, 16,  3, 10, 0x8A8D)
	TILE_SEQ_LINE( 13,  3,  0,  3, 13, 10, 0x8A91)
	TILE_SEQ_END()
};

static const byte _station_display_datas_73[] = {
	TILE_SEQ_BEGIN(0x8A86)
	TILE_SEQ_LINE(  3, 15,  0, 11,  1, 10, 0x8A8A)
	TILE_SEQ_LINE(  0,  0,  0,  3, 16, 10, 0x8A8E)
	TILE_SEQ_LINE(  3,  0,  0, 13,  3, 10, 0x8A92)
	TILE_SEQ_END()
};

static const byte _station_display_datas_74[] = {
	TILE_SEQ_BEGIN(0x8A87)
	TILE_SEQ_LINE( 15,  2,  0,  1, 11, 10, 0x8A8B)
	TILE_SEQ_LINE(  0, 13,  0, 16,  3, 10, 0x8A8F)
	TILE_SEQ_LINE(  0,  0,  0,  3, 13, 10, 0x8A93)
	TILE_SEQ_END()
};

static const byte _station_display_datas_75[] = {
	TILE_SEQ_BEGIN(0xFDD)
	TILE_SEQ_END()
};

static const byte _station_display_datas_76[] = {
	TILE_SEQ_BEGIN(0xFE4)
	TILE_SEQ_LINE(  0,  4,  0, 16,  8,  8, 0xAA7)
	TILE_SEQ_END()
};

static const byte _station_display_datas_77[] = {
	TILE_SEQ_BEGIN(0xFE5)
	TILE_SEQ_LINE(  4,  0,  0,  8, 16,  8, 0xAA8)
	TILE_SEQ_END()
};

static const byte _station_display_datas_78[] = {
	TILE_SEQ_BEGIN(0xFE3)
	TILE_SEQ_LINE(  0,  4,  0, 16,  8,  8, 0xAA9)
	TILE_SEQ_END()
};

static const byte _station_display_datas_79[] = {
	TILE_SEQ_BEGIN(0xFE2)
	TILE_SEQ_LINE(  4,  0,  0,  8, 16,  8, 0xAAA)
	TILE_SEQ_END()
};

static const byte _station_display_datas_80[] = {
	TILE_SEQ_BEGIN(0xFDD)
	TILE_SEQ_LINE(  0,  4,  0, 16,  8,  8, 0xAAB)
	TILE_SEQ_END()
};

static const byte _station_display_datas_81[] = {
	TILE_SEQ_BEGIN(0xFDD)
	TILE_SEQ_LINE(  4,  0,  0,  8, 16,  8, 0xAAC)
	TILE_SEQ_END()
};

static const byte _station_display_datas_82[] = {
	TILE_SEQ_BEGIN(0xFEC)
	TILE_SEQ_END()
};

// end of runway 
const byte _station_display_datas_083[] = {
	TILE_SEQ_BEGIN(0xA59)
	TILE_SEQ_END()
};

// runway tiles
const byte _station_display_datas_084[] = {
	TILE_SEQ_BEGIN(0xA56)
	TILE_SEQ_END()
};

// control tower with concrete underground and no fence
const byte _station_display_datas_085[] = {
	TILE_SEQ_BEGIN(0xA4A) // concrete underground
	TILE_SEQ_LINE(  3,  3,  0, 10, 10, 60, 0x8A5B)  // control tower
	TILE_SEQ_END()
};

// new airportdepot, facing west
const byte _station_display_datas_086[] = {
	TILE_SEQ_BEGIN(0xA4A)
	TILE_SEQ_LINE( 14, 0,  0,  2, 16, 28, 0x8A61)
	TILE_SEQ_LINE(  0, 0,  0,  2, 16, 28, 0xA62)
	TILE_SEQ_END()
};

// asphalt tile with fences in north
const byte _station_display_datas_087[] = {
	TILE_SEQ_BEGIN(0xA4A)
	TILE_SEQ_LINE( 0,  0, 0,  1, 16,  6, 0x8A67)
	TILE_SEQ_END()
};

// end of runway 
const byte _station_display_datas_088[] = {
	TILE_SEQ_BEGIN(0xA59)
	TILE_SEQ_LINE(  0,  0,  0, 16,  1,  6, 0x8A68) // fences
	TILE_SEQ_END()
};

// runway tiles
const byte _station_display_datas_089[] = {
	TILE_SEQ_BEGIN(0xA56)
	TILE_SEQ_LINE(  0,  0,  0, 16,  1,  6, 0x8A68) // fences	
	TILE_SEQ_END()
};

// turning radar with concrete underground fences on south -- needs 12 tiles
//BEGIN
const byte _station_display_datas_090[] = {
	TILE_SEQ_BEGIN(0xA4A) // concrete underground
	TILE_SEQ_LINE(  7,  7,  0,  2,  2,  8, 0xA78)   // turning radar
	TILE_SEQ_LINE(15,  0,  0,  1, 16,  6, 0x8A67)  //fences
	TILE_SEQ_END()
};

const byte _station_display_datas_091[] = {
	TILE_SEQ_BEGIN(0xA4A) // concrete underground
	TILE_SEQ_LINE(  7,  7,  0,  2,  2,  8, 0xA79)
	TILE_SEQ_LINE( 15,  0, 0,  1, 16,  6, 0x8A67)
	TILE_SEQ_END()
};

const byte _station_display_datas_092[] = {
	TILE_SEQ_BEGIN(0xA4A) // concrete underground
	TILE_SEQ_LINE(  7,  7,  0,  2,  2,  8, 0xA7A)
	TILE_SEQ_LINE( 15,  0, 0,  1, 16,  6, 0x8A67)
	TILE_SEQ_END()
};

const byte _station_display_datas_093[] = {
	TILE_SEQ_BEGIN(0xA4A) // concrete underground
	TILE_SEQ_LINE(  7,  7,  0,  2,  2,  8, 0xA7B)
	TILE_SEQ_LINE( 15,  0, 0,  1, 16,  6, 0x8A67)
	TILE_SEQ_END()
};

const byte _station_display_datas_094[] = {
	TILE_SEQ_BEGIN(0xA4A) // concrete underground
	TILE_SEQ_LINE(  7,  7,  0,  2,  2,  8, 0xA7C)
	TILE_SEQ_LINE( 15,  0, 0,  1, 16,  6, 0x8A67)
	TILE_SEQ_END()
};

const byte _station_display_datas_095[] = {
	TILE_SEQ_BEGIN(0xA4A) // concrete underground
	TILE_SEQ_LINE(  7,  7,  0,  2,  2,  8, 0xA7D)
	TILE_SEQ_LINE( 15,  0, 0,  1, 16,  6, 0x8A67)
	TILE_SEQ_END()
};

const byte _station_display_datas_096[] = {
	TILE_SEQ_BEGIN(0xA4A) // concrete underground
	TILE_SEQ_LINE(  7,  7,  0,  2,  2,  8, 0xA7E)
	TILE_SEQ_LINE( 15,  0, 0,  1, 16,  6, 0x8A67)
	TILE_SEQ_END()
};

const byte _station_display_datas_097[] = {
	TILE_SEQ_BEGIN(0xA4A) // concrete underground
	TILE_SEQ_LINE(  7,  7,  0,  2,  2,  8, 0xA7F)
	TILE_SEQ_LINE( 15,  0, 0,  1, 16,  6, 0x8A67)
	TILE_SEQ_END()
};

const byte _station_display_datas_098[] = {
	TILE_SEQ_BEGIN(0xA4A) // concrete underground
	TILE_SEQ_LINE(  7,  7,  0,  2,  2,  8, 0xA80)
	TILE_SEQ_LINE( 15,  0, 0,  1, 16,  6, 0x8A67)
	TILE_SEQ_END()
};

const byte _station_display_datas_099[] = {
	TILE_SEQ_BEGIN(0xA4A) // concrete underground
	TILE_SEQ_LINE(  7,  7,  0,  2,  2,  8, 0xA81)
	TILE_SEQ_LINE( 15,  0, 0,  1, 16,  6, 0x8A67)
	TILE_SEQ_END()
};

const byte _station_display_datas_0100[] = {
	TILE_SEQ_BEGIN(0xA4A) // concrete underground
	TILE_SEQ_LINE(  7,  7,  0,  2,  2,  8, 0xA82)
	TILE_SEQ_LINE( 15,  0, 0,  1, 16,  6, 0x8A67)
	TILE_SEQ_END()
};

const byte _station_display_datas_0101[] = {
	TILE_SEQ_BEGIN(0xA4A) // concrete underground
	TILE_SEQ_LINE(  7,  7,  0,  2,  2,  8, 0xA83)
	TILE_SEQ_LINE( 15,  0, 0,  1, 16,  6, 0x8A67)
	TILE_SEQ_END()
};
//END

// turning radar with concrete underground fences on north -- needs 12 tiles
//BEGIN
const byte _station_display_datas_0102[] = {
	TILE_SEQ_BEGIN(0xA4A) // concrete underground
	TILE_SEQ_LINE(  7,  7,  0,  2,  2,  8, 0xA78)   // turning radar
	TILE_SEQ_LINE( 0,  0, 0,  1, 16,  6, 0x8A67)
	TILE_SEQ_END()
};

const byte _station_display_datas_0103[] = {
	TILE_SEQ_BEGIN(0xA4A) // concrete underground
	TILE_SEQ_LINE(  7,  7,  0,  2,  2,  8, 0xA79)
	TILE_SEQ_LINE( 0,  0, 0,  1, 16,  6, 0x8A67)
	TILE_SEQ_END()
};

const byte _station_display_datas_0104[] = {
	TILE_SEQ_BEGIN(0xA4A) // concrete underground
	TILE_SEQ_LINE(  7,  7,  0,  2,  2,  8, 0xA7A)
	TILE_SEQ_LINE( 0,  0, 0,  1, 16,  6, 0x8A67)
	TILE_SEQ_END()
};

const byte _station_display_datas_0105[] = {
	TILE_SEQ_BEGIN(0xA4A) // concrete underground
	TILE_SEQ_LINE(  7,  7,  0,  2,  2,  8, 0xA7B)
	TILE_SEQ_LINE( 0,  0, 0,  1, 16,  6, 0x8A67)
	TILE_SEQ_END()
};

const byte _station_display_datas_0106[] = {
	TILE_SEQ_BEGIN(0xA4A) // concrete underground
	TILE_SEQ_LINE(  7,  7,  0,  2,  2,  8, 0xA7C)
	TILE_SEQ_LINE( 0,  0, 0,  1, 16,  6, 0x8A67)
	TILE_SEQ_END()
};

const byte _station_display_datas_0107[] = {
	TILE_SEQ_BEGIN(0xA4A) // concrete underground
	TILE_SEQ_LINE(  7,  7,  0,  2,  2,  8, 0xA7D)
	TILE_SEQ_LINE( 0,  0, 0,  1, 16,  6, 0x8A67)
	TILE_SEQ_END()
};

const byte _station_display_datas_0108[] = {
	TILE_SEQ_BEGIN(0xA4A) // concrete underground
	TILE_SEQ_LINE(  7,  7,  0,  2,  2,  8, 0xA7E)
	TILE_SEQ_LINE( 0,  0, 0,  1, 16,  6, 0x8A67)
	TILE_SEQ_END()
};

const byte _station_display_datas_0109[] = {
	TILE_SEQ_BEGIN(0xA4A) // concrete underground
	TILE_SEQ_LINE(  7,  7,  0,  2,  2,  8, 0xA7F)
	TILE_SEQ_LINE( 0,  0, 0,  1, 16,  6, 0x8A67)
	TILE_SEQ_END()
};

const byte _station_display_datas_0110[] = {
	TILE_SEQ_BEGIN(0xA4A) // concrete underground
	TILE_SEQ_LINE(  7,  7,  0,  2,  2,  8, 0xA80)
	TILE_SEQ_LINE( 0,  0, 0,  1, 16,  6, 0x8A67)
	TILE_SEQ_END()
};

const byte _station_display_datas_0111[] = {
	TILE_SEQ_BEGIN(0xA4A) // concrete underground
	TILE_SEQ_LINE(  7,  7,  0,  2,  2,  8, 0xA81)
	TILE_SEQ_LINE( 0,  0, 0,  1, 16,  6, 0x8A67)
	TILE_SEQ_END()
};

const byte _station_display_datas_0112[] = {
	TILE_SEQ_BEGIN(0xA4A) // concrete underground
	TILE_SEQ_LINE(  7,  7,  0,  2,  2,  8, 0xA82)
	TILE_SEQ_LINE( 0,  0, 0,  1, 16,  6, 0x8A67)
	TILE_SEQ_END()
};

const byte _station_display_datas_0113[] = {
	TILE_SEQ_BEGIN(0xA4A) // concrete underground
	TILE_SEQ_LINE(  7,  7,  0,  2,  2,  8, 0xA83)
	TILE_SEQ_LINE( 0,  0, 0,  1, 16,  6, 0x8A67)
	TILE_SEQ_END()
};
//END

// helipad for international airport
const byte _station_display_datas_0114[] = {
	TILE_SEQ_BEGIN(0xA4A)	// concrete underground
	TILE_SEQ_LINE(10,  6, 0,  0, 0,  0, SPR_OPENTTD_BASE + 31)	// helipad
	TILE_SEQ_LINE( 15,  0,  0,  1, 16,  6, 0x8A67)	// fences bottom
	TILE_SEQ_END()
};

static const byte * const _station_display_datas[] = {
	_station_display_datas_0,
	_station_display_datas_1,
	_station_display_datas_2,
	_station_display_datas_3,
	_station_display_datas_4,
	_station_display_datas_5,
	_station_display_datas_6,
	_station_display_datas_7,
	_station_display_datas_8,
	_station_display_datas_9,
	_station_display_datas_10,
	_station_display_datas_11,
	_station_display_datas_12,
	_station_display_datas_13,
	_station_display_datas_14,
	_station_display_datas_15,
	_station_display_datas_16,
	_station_display_datas_17,
	_station_display_datas_18,
	_station_display_datas_19,
	_station_display_datas_20,
	_station_display_datas_21,
	_station_display_datas_22,
	_station_display_datas_23,
	_station_display_datas_24,
	_station_display_datas_25,
	_station_display_datas_26,
	_station_display_datas_27,
	_station_display_datas_28,
	_station_display_datas_29,
	_station_display_datas_30,
	_station_display_datas_31,
	_station_display_datas_32,
	_station_display_datas_33,
	_station_display_datas_34,
	_station_display_datas_35,
	_station_display_datas_36,
	_station_display_datas_37,
	_station_display_datas_38,
	_station_display_datas_39,
	_station_display_datas_40,
	_station_display_datas_41,
	_station_display_datas_42,
	_station_display_datas_43,
	_station_display_datas_44,
	_station_display_datas_45,
	_station_display_datas_46,
	_station_display_datas_47,
	_station_display_datas_48,
	_station_display_datas_49,
	_station_display_datas_50,
	_station_display_datas_51,
	_station_display_datas_52,
	_station_display_datas_53,
	_station_display_datas_54,
	_station_display_datas_55,
	_station_display_datas_56,
	_station_display_datas_57,
	_station_display_datas_58,
	_station_display_datas_59,
	_station_display_datas_60,
	_station_display_datas_61,
	_station_display_datas_62,
	_station_display_datas_63,
	_station_display_datas_64,
	_station_display_datas_65,
	_station_display_datas_66,
	_station_display_datas_67,
	_station_display_datas_68,
	_station_display_datas_69,
	_station_display_datas_70,
	_station_display_datas_71,
	_station_display_datas_72,
	_station_display_datas_73,
	_station_display_datas_74,
	_station_display_datas_75,
	_station_display_datas_76,
	_station_display_datas_77,
	_station_display_datas_78,
	_station_display_datas_79,
	_station_display_datas_80,
	_station_display_datas_81,
	_station_display_datas_82,
	_station_display_datas_083,
	_station_display_datas_084,
	_station_display_datas_085,
	_station_display_datas_086,
	_station_display_datas_087,
	_station_display_datas_088,
	_station_display_datas_089,
	_station_display_datas_090,		
	_station_display_datas_091,	
	_station_display_datas_092,	
	_station_display_datas_093,	
	_station_display_datas_094,	
	_station_display_datas_095,	
	_station_display_datas_096,	
	_station_display_datas_097,	
	_station_display_datas_098,	
	_station_display_datas_099,	
	_station_display_datas_0100,
	_station_display_datas_0101,		
	_station_display_datas_0102,		
	_station_display_datas_0103,	
	_station_display_datas_0104,	
	_station_display_datas_0105,	
	_station_display_datas_0106,	
	_station_display_datas_0107,	
	_station_display_datas_0108,	
	_station_display_datas_0109,	
	_station_display_datas_0110,	
	_station_display_datas_0111,	
	_station_display_datas_0112,
	_station_display_datas_0113,
  _station_display_datas_0114,
};
