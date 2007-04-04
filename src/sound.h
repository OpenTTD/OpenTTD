/* $Id$ */

/** @file sound.h */

#ifndef SOUND_H
#define SOUND_H

#include "helpers.hpp"

struct MusicFileSettings {
	byte playlist;
	byte music_vol;
	byte effect_vol;
	byte custom_1[33];
	byte custom_2[33];
	bool playing;
	bool shuffle;
	char extmidi[80];
};

VARDEF MusicFileSettings msf;

struct FileEntry {
	uint32 file_offset;
	uint32 file_size;
	uint16 rate;
	uint8 bits_per_sample;
	uint8 channels;
	uint8 volume;
	uint8 priority;
};

bool SoundInitialize(const char *filename);
uint GetNumOriginalSounds();

enum SoundFx {
	SND_BEGIN = 0,
	SND_02_SPLAT = 0,                          //  0 == 0x00 !
	SND_03_FACTORY_WHISTLE,
	SND_04_TRAIN,
	SND_05_TRAIN_THROUGH_TUNNEL,
	SND_06_SHIP_HORN,
	SND_07_FERRY_HORN,
	SND_08_PLANE_TAKE_OFF,
	SND_09_JET,
	SND_0A_TRAIN_HORN,
	SND_0B_MINING_MACHINERY,
	SND_0C_ELECTRIC_SPARK,
	SND_0D_STEAM,
	SND_0E_LEVEL_CROSSING,
	SND_0F_VEHICLE_BREAKDOWN,
	SND_10_TRAIN_BREAKDOWN,
	SND_11_CRASH,
	SND_12_EXPLOSION,                      // 16 == 0x10
	SND_13_BIG_CRASH,
	SND_14_CASHTILL,
	SND_15_BEEP,                           // 19 == 0x13
	SND_16_MORSE,                          // 20 == 0x14
	SND_17_SKID_PLANE,
	SND_18_HELICOPTER,
	SND_19_BUS_START_PULL_AWAY,
	SND_1A_BUS_START_PULL_AWAY_WITH_HORN,
	SND_1B_TRUCK_START,
	SND_1C_TRUCK_START_2,
	SND_1D_APPLAUSE,
	SND_1E_OOOOH,
	SND_1F_SPLAT,                          // 29 == 0x1D
	SND_20_SPLAT_2,                        // 30 == 0x1E
	SND_21_JACKHAMMER,
	SND_22_CAR_HORN,
	SND_23_CAR_HORN_2,
	SND_24_SHEEP,
	SND_25_COW,
	SND_26_HORSE,
	SND_27_BLACKSMITH_ANVIL,
	SND_28_SAWMILL,                        // 38 == 0x26 !
	SND_00_GOOD_YEAR,                      // 39 == 0x27 !
	SND_01_BAD_YEAR,                       // 40 == 0x28 !
	SND_29_RIP,                            // 41 == 0x29 !
	SND_2A_EXTRACT_AND_POP,
	SND_2B_COMEDY_HIT,
	SND_2C_MACHINERY,
	SND_2D_RIP_2,
	SND_2E_EXTRACT_AND_POP,
	SND_2F_POP,
	SND_30_CARTOON_SOUND,
	SND_31_EXTRACT,
	SND_32_POP_2,
	SND_33_PLASTIC_MINE,
	SND_34_WIND,
	SND_35_COMEDY_BREAKDOWN,
	SND_36_CARTOON_CRASH,
	SND_37_BALLOON_SQUEAK,
	SND_38_CHAINSAW,
	SND_39_HEAVY_WIND,
	SND_3A_COMEDY_BREAKDOWN_2,
	SND_3B_JET_OVERHEAD,
	SND_3C_COMEDY_CAR,
	SND_3D_ANOTHER_JET_OVERHEAD,
	SND_3E_COMEDY_CAR_2,
	SND_3F_COMEDY_CAR_3,
	SND_40_COMEDY_CAR_START_AND_PULL_AWAY,
	SND_41_MAGLEV,
	SND_42_LOON_BIRD,
	SND_43_LION,
	SND_44_MONKEYS,
	SND_45_PLANE_CRASHING,
	SND_46_PLANE_ENGINE_SPUTTERING,
	SND_47_MAGLEV_2,
	SND_48_DISTANT_BIRD,                    // 72 == 0x48
	SND_END
};

/** Define basic enum properties */
template <> struct EnumPropsT<SoundFx> : MakeEnumPropsT<SoundFx, byte, SND_BEGIN, SND_END, SND_END> {};
typedef TinyEnumT<SoundFx> SoundFxByte;

void SndPlayTileFx(SoundFx sound, TileIndex tile);
void SndPlayVehicleFx(SoundFx sound, const Vehicle *v);
void SndPlayFx(SoundFx sound);
void SndCopyToPool();

#endif /* SOUND_H */
