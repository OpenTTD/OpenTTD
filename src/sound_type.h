/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file sound_type.h Types related to sounds. */

#ifndef SOUND_TYPE_H
#define SOUND_TYPE_H

struct SoundEntry {
	class RandomAccessFile *file;
	size_t file_offset;
	size_t file_size;
	uint16_t rate;
	uint8_t bits_per_sample;
	uint8_t channels;
	uint8_t volume;
	uint8_t priority;
	byte grf_container_ver; ///< NewGRF container version if the sound is from a NewGRF.
};

/**
 * Sound effects from baseset.
 *
 * This enum contains the sound effects from the sound baseset.
 * For hysterical raisins the order of sound effects in the baseset
 * is different to the order they are referenced in TTD/NewGRF.
 *  - The first two sound effects from the baseset are inserted at position 39.
 *    (see translation table _sound_idx)
 *  - The order in the enum is the order using in TTD/NewGRF.
 *  - The naming of the enum values includes the position in the baseset.
 * That is, for sound effects 0x02 to 0x28 the naming is off-by-two.
 */
enum SoundFx {
	SND_BEGIN = 0,
	SND_02_CONSTRUCTION_WATER = 0,         ///<  0 == 0x00  Construction: water infrastructure
	SND_03_FACTORY,                        ///<  1 == 0x01  Industry producing: factory: whistle
	SND_04_DEPARTURE_STEAM,                ///<  2 == 0x02  Station departure: steam engine
	SND_05_TRAIN_THROUGH_TUNNEL,           ///<  3 == 0x03  Train enters tunnel: steam engine
	SND_06_DEPARTURE_CARGO_SHIP,           ///<  4 == 0x04  Station departure: cargo ships
	SND_07_DEPARTURE_FERRY,                ///<  5 == 0x05  Station departure: passenger ships
	SND_08_TAKEOFF_PROPELLER,              ///<  6 == 0x06  Takeoff: propeller plane (non-toyland)
	SND_09_TAKEOFF_JET,                    ///<  7 == 0x07  Takeoff: regular jet plane
	SND_0A_DEPARTURE_TRAIN,                ///<  8 == 0x08  Station departure: diesel and electric engine
	SND_0B_MINE,                           ///<  9 == 0x09  Industry animation: coal/copper/gold mine: headgear
	SND_0C_POWER_STATION,                  ///< 10 == 0x0A  Industry animation: power station: spark
	SND_0D_UNUSED,                         ///< 11 == 0x0B  unused (1)
	SND_0E_LEVEL_CROSSING,                 ///< 12 == 0x0C  Train passes through level crossing
	SND_0F_BREAKDOWN_ROADVEHICLE,          ///< 13 == 0x0D  Breakdown: road vehicle (non-toyland)
	SND_10_BREAKDOWN_TRAIN_SHIP,           ///< 14 == 0x0E  Breakdown: train or ship (non-toyland)
	SND_11_UNUSED,                         ///< 15 == 0x0F  unused (2)
	SND_12_EXPLOSION,                      ///< 16 == 0x10  Destruction, crashes, disasters, ...
	SND_13_TRAIN_COLLISION,                ///< 15 == 0x11  Train+train crash
	SND_14_CASHTILL,                       ///< 18 == 0x12  Income from cargo delivery
	SND_15_BEEP,                           ///< 19 == 0x13  GUI button click
	SND_16_NEWS_TICKER,                    ///< 20 == 0x14  News ticker
	SND_17_SKID_PLANE,                     ///< 21 == 0x15  Plane landing / touching ground
	SND_18_TAKEOFF_HELICOPTER,             ///< 22 == 0x16  Takeoff: helicopter
	SND_19_DEPARTURE_OLD_RV_1,             ///< 23 == 0x17  Station departure: truck and old bus (1) (non-toyland)
	SND_1A_DEPARTURE_OLD_RV_2,             ///< 24 == 0x18  Station departure: truck and old bus (2) (random variation of SND_19_DEPARTURE_OLD_RV_1) (non-toyland)
	SND_1B_DEPARTURE_MODERN_BUS,           ///< 25 == 0x19  Station departure: modern bus (non-toyland)
	SND_1C_DEPARTURE_OLD_BUS,              ///< 26 == 0x1A  Station departure: old bus (non-toyland)
	SND_1D_APPLAUSE,                       ///< 27 == 0x1B  News: first vehicle at station
	SND_1E_NEW_ENGINE,                     ///< 28 == 0x1C  News: new engine available
	SND_1F_CONSTRUCTION_OTHER,             ///< 29 == 0x1D  Construction: other (non-water, non-rail, non-bridge)
	SND_20_CONSTRUCTION_RAIL,              ///< 30 == 0x1E  Construction: rail infrastructure
	SND_21_ROAD_WORKS,                     ///< 31 == 0x1F  Road reconstruction animation
	SND_22_UNUSED,                         ///< 32 == 0x20  unused (3)
	SND_23_UNUSED,                         ///< 33 == 0x21  unused (4)
	SND_24_FARM_1,                         ///< 34 == 0x22  Industry producing: farm (1): sheep
	SND_25_FARM_2,                         ///< 35 == 0x23  Industry producing: farm (2): cow
	SND_26_FARM_3,                         ///< 36 == 0x24  Industry producing: farm (3): horse
	SND_27_CONSTRUCTION_BRIDGE,            ///< 37 == 0x25  Construction: bridge
	SND_28_SAWMILL,                        ///< 38 == 0x26  Industry producing: sawmill
	SND_00_GOOD_YEAR,                      ///< 39 == 0x27  New year: performance improved
	SND_01_BAD_YEAR,                       ///< 40 == 0x28  New year: performance declined
	SND_29_SUGAR_MINE_2,                   ///< 41 == 0x29  Industry animation: sugar mine (2): shaking sieve
	SND_2A_TOY_FACTORY_3,                  ///< 42 == 0x2A  Industry animation: toy factory (3): eject product
	SND_2B_TOY_FACTORY_2,                  ///< 43 == 0x2B  Industry animation: toy factory (2): stamp product
	SND_2C_TOY_FACTORY_1,                  ///< 44 == 0x2C  Industry animation: toy factory (1): conveyor belt
	SND_2D_SUGAR_MINE_1,                   ///< 45 == 0x2D  Industry animation: sugar mine (1): shaking sieve
	SND_2E_BUBBLE_GENERATOR,               ///< 46 == 0x2E  Industry animation: bubble generator (1): generate bubble
	SND_2F_BUBBLE_GENERATOR_FAIL,          ///< 47 == 0x2F  Industry animation: bubble generator (2a): bubble pop
	SND_30_TOFFEE_QUARRY,                  ///< 48 == 0x30  Industry animation: toffee quarry: drill
	SND_31_BUBBLE_GENERATOR_SUCCESS,       ///< 49 == 0x31  Industry animation: bubble generator (2b): bubble slurped
	SND_32_UNUSED,                         ///< 50 == 0x32  unused (5)
	SND_33_PLASTIC_MINE,                   ///< 51 == 0x33  Industry producing: plastic fountain
	SND_34_ARCTIC_SNOW_1,                  ///< 52 == 0x34  Tree ambient: arctic snow (1): wind
	SND_35_BREAKDOWN_ROADVEHICLE_TOYLAND,  ///< 53 == 0x35  Breakdown: road vehicle (toyland)
	SND_36_LUMBER_MILL_3,                  ///< 54 == 0x36  Industry animation: lumber mill (3): crashing tree
	SND_37_LUMBER_MILL_2,                  ///< 55 == 0x37  Industry animation: lumber mill (2): falling tree
	SND_38_LUMBER_MILL_1,                  ///< 56 == 0x38  Industry animation: lumber mill (1): chainsaw
	SND_39_ARCTIC_SNOW_2,                  ///< 57 == 0x39  Tree ambient: arctic snow (2): heavy wind
	SND_3A_BREAKDOWN_TRAIN_SHIP_TOYLAND,   ///< 58 == 0x3A  Breakdown: train or ship (toyland)
	SND_3B_TAKEOFF_JET_FAST,               ///< 59 == 0x3B  Takeoff: supersonic plane (fast)
	SND_3C_DEPARTURE_BUS_TOYLAND_1,        ///< 60 == 0x3C  Station departure: bus (1) (toyland)
	SND_3D_TAKEOFF_JET_BIG,                ///< 61 == 0x3D  Takeoff: huge jet plane (high capacity)
	SND_3E_DEPARTURE_BUS_TOYLAND_2,        ///< 62 == 0x3E  Station departure: bus (2) (toyland)
	SND_3F_DEPARTURE_TRUCK_TOYLAND_1,      ///< 63 == 0x3F  Station departure: truck (1) (toyland)
	SND_40_DEPARTURE_TRUCK_TOYLAND_2,      ///< 64 == 0x40  Station departure: truck (2) (toyland)
	SND_41_DEPARTURE_MAGLEV,               ///< 65 == 0x41  Station departure: maglev engine
	SND_42_RAINFOREST_1,                   ///< 66 == 0x42  Tree ambient: rainforest ambient (1): bird (1)
	SND_43_RAINFOREST_2,                   ///< 67 == 0x43  Tree ambient: rainforest ambient (2): lion
	SND_44_RAINFOREST_3,                   ///< 68 == 0x44  Tree ambient: rainforest ambient (3): monkeys
	SND_45_TAKEOFF_PROPELLER_TOYLAND_1,    ///< 69 == 0x45  Takeoff: propeller plane (1) (toyland)
	SND_46_TAKEOFF_PROPELLER_TOYLAND_2,    ///< 70 == 0x46  Takeoff: propeller plane (2) (toyland)
	SND_47_DEPARTURE_MONORAIL,             ///< 71 == 0x47  Station departure: monorail engine
	SND_48_RAINFOREST_4,                   ///< 72 == 0x48  Tree ambient: rainforest ambient (4): bird (2)
	SND_END
};

/** The number of sounds in the original sample.cat */
static const uint ORIGINAL_SAMPLE_COUNT = 73;

typedef uint16_t SoundID;

static const SoundID INVALID_SOUND = 0xFFFF;

#endif /* SOUND_TYPE_H */
