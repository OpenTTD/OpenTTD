/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file framerate_type.h
 * Types for recording game performance data.
 *
 * @par Adding new measurements
 * Adding a new measurement requires multiple steps, which are outlined here.
 * The first thing to do is add a new member of the #PerformanceElement enum.
 * It must be added before \c PFE_MAX and should be added in a logical place.
 * For example, an element of the game loop would be added next to the other game loop elements, and a rendering element next to the other rendering elements.
 *
 * @par
 * Second is adding a member to the \link anonymous_namespace{framerate_gui.cpp}::_pf_data _pf_data \endlink array, in the same position as the new #PerformanceElement member.
 *
 * @par
 * Third is adding strings for the new element. There is an array in #ConPrintFramerate with strings used for the console command.
 * Additionally, there are two sets of strings in \c english.txt for two GUI uses, also in the #PerformanceElement order.
 * Search for \c STR_FRAMERATE_GAMELOOP and \c STR_FRAMETIME_CAPTION_GAMELOOP in \c english.txt to find those.
 *
 * @par
 * Last is actually adding the measurements. There are two ways to measure, either one-shot (a single function/block handling all processing),
 * or as an accumulated element (multiple functions/blocks that need to be summed across each frame/tick).
 * Use either the PerformanceMeasurer or the PerformanceAccumulator class respectively for the two cases.
 * Either class is used by instantiating an object of it at the beginning of the block to be measured, so it auto-destructs at the end of the block.
 * For PerformanceAccumulator, make sure to also call PerformanceAccumulator::Reset once at the beginning of a new frame. Usually the StateGameLoop function is appropriate for this.
 *
 * @see framerate_gui.cpp for implementation
 */

#ifndef FRAMERATE_TYPE_H
#define FRAMERATE_TYPE_H

#include "stdafx.h"
#include "core/enum_type.hpp"

/**
 * Elements of game performance that can be measured.
 *
 * @note When adding new elements here, make sure to also update all other locations depending on the length and order of this enum.
 * See <em>Adding new measurements</em> above.
 */
enum PerformanceElement {
	PFE_FIRST = 0,
	PFE_GAMELOOP = 0,  ///< Speed of gameloop processing.
	PFE_GL_ECONOMY,    ///< Time spent processing cargo movement
	PFE_GL_TRAINS,     ///< Time spent processing trains
	PFE_GL_ROADVEHS,   ///< Time spend processing road vehicles
	PFE_GL_SHIPS,      ///< Time spent processing ships
	PFE_GL_AIRCRAFT,   ///< Time spent processing aircraft
	PFE_GL_LANDSCAPE,  ///< Time spent processing other world features
	PFE_GL_LINKGRAPH,  ///< Time spent waiting for link graph background jobs
	PFE_DRAWING,       ///< Speed of drawing world and GUI.
	PFE_DRAWWORLD,     ///< Time spent drawing world viewports in GUI
	PFE_VIDEO,         ///< Speed of painting drawn video buffer.
	PFE_SOUND,         ///< Speed of mixing audio samples
	PFE_ALLSCRIPTS,    ///< Sum of all GS/AI scripts
	PFE_GAMESCRIPT,    ///< Game script execution
	PFE_AI0,           ///< AI execution for player slot 1
	PFE_AI1,           ///< AI execution for player slot 2
	PFE_AI2,           ///< AI execution for player slot 3
	PFE_AI3,           ///< AI execution for player slot 4
	PFE_AI4,           ///< AI execution for player slot 5
	PFE_AI5,           ///< AI execution for player slot 6
	PFE_AI6,           ///< AI execution for player slot 7
	PFE_AI7,           ///< AI execution for player slot 8
	PFE_AI8,           ///< AI execution for player slot 9
	PFE_AI9,           ///< AI execution for player slot 10
	PFE_AI10,          ///< AI execution for player slot 11
	PFE_AI11,          ///< AI execution for player slot 12
	PFE_AI12,          ///< AI execution for player slot 13
	PFE_AI13,          ///< AI execution for player slot 14
	PFE_AI14,          ///< AI execution for player slot 15
	PFE_AI15,          ///< AI execution for player slot 16
	PFE_AI16,          ///< AI execution for player slot 17
	PFE_AI17,          ///< AI execution for player slot 18
	PFE_AI18,          ///< AI execution for player slot 19
	PFE_AI19,          ///< AI execution for player slot 20
	PFE_AI20,          ///< AI execution for player slot 21
	PFE_AI21,          ///< AI execution for player slot 22
	PFE_AI22,          ///< AI execution for player slot 23
	PFE_AI23,          ///< AI execution for player slot 24
	PFE_AI24,          ///< AI execution for player slot 25
	PFE_AI25,          ///< AI execution for player slot 26
	PFE_AI26,          ///< AI execution for player slot 27
	PFE_AI27,          ///< AI execution for player slot 28
	PFE_AI28,          ///< AI execution for player slot 29
	PFE_AI29,          ///< AI execution for player slot 30
	PFE_AI30,          ///< AI execution for player slot 31
	PFE_AI31,          ///< AI execution for player slot 32
	PFE_AI32,          ///< AI execution for player slot 33
	PFE_AI33,          ///< AI execution for player slot 34
	PFE_AI34,          ///< AI execution for player slot 35
	PFE_AI35,          ///< AI execution for player slot 36
	PFE_AI36,          ///< AI execution for player slot 37
	PFE_AI37,          ///< AI execution for player slot 38
	PFE_AI38,          ///< AI execution for player slot 39
	PFE_AI39,          ///< AI execution for player slot 40
	PFE_AI40,          ///< AI execution for player slot 41
	PFE_AI41,          ///< AI execution for player slot 42
	PFE_AI42,          ///< AI execution for player slot 43
	PFE_AI43,          ///< AI execution for player slot 44
	PFE_AI44,          ///< AI execution for player slot 45
	PFE_AI45,          ///< AI execution for player slot 46
	PFE_AI46,          ///< AI execution for player slot 47
	PFE_AI47,          ///< AI execution for player slot 48
	PFE_AI48,          ///< AI execution for player slot 49
	PFE_AI49,          ///< AI execution for player slot 50
	PFE_AI50,          ///< AI execution for player slot 51
	PFE_AI51,          ///< AI execution for player slot 52
	PFE_AI52,          ///< AI execution for player slot 53
	PFE_AI53,          ///< AI execution for player slot 54
	PFE_AI54,          ///< AI execution for player slot 55
	PFE_AI55,          ///< AI execution for player slot 56
	PFE_AI56,          ///< AI execution for player slot 57
	PFE_AI57,          ///< AI execution for player slot 58
	PFE_AI58,          ///< AI execution for player slot 59
	PFE_AI59,          ///< AI execution for player slot 60
	PFE_AI60,          ///< AI execution for player slot 61
	PFE_AI61,          ///< AI execution for player slot 62
	PFE_AI62,          ///< AI execution for player slot 63
	PFE_AI63,          ///< AI execution for player slot 64
	PFE_AI64,          ///< AI execution for player slot 65
	PFE_AI65,          ///< AI execution for player slot 66
	PFE_AI66,          ///< AI execution for player slot 67
	PFE_AI67,          ///< AI execution for player slot 68
	PFE_AI68,          ///< AI execution for player slot 69
	PFE_AI69,          ///< AI execution for player slot 70
	PFE_AI70,          ///< AI execution for player slot 71
	PFE_AI71,          ///< AI execution for player slot 72
	PFE_AI72,          ///< AI execution for player slot 73
	PFE_AI73,          ///< AI execution for player slot 74
	PFE_AI74,          ///< AI execution for player slot 75
	PFE_AI75,          ///< AI execution for player slot 76
	PFE_AI76,          ///< AI execution for player slot 77
	PFE_AI77,          ///< AI execution for player slot 78
	PFE_AI78,          ///< AI execution for player slot 79
	PFE_AI79,          ///< AI execution for player slot 80
	PFE_AI80,          ///< AI execution for player slot 81
	PFE_AI81,          ///< AI execution for player slot 82
	PFE_AI82,          ///< AI execution for player slot 83
	PFE_AI83,          ///< AI execution for player slot 84
	PFE_AI84,          ///< AI execution for player slot 85
	PFE_AI85,          ///< AI execution for player slot 86
	PFE_AI86,          ///< AI execution for player slot 87
	PFE_AI87,          ///< AI execution for player slot 88
	PFE_AI88,          ///< AI execution for player slot 89
	PFE_AI89,          ///< AI execution for player slot 90
	PFE_AI90,          ///< AI execution for player slot 91
	PFE_AI91,          ///< AI execution for player slot 92
	PFE_AI92,          ///< AI execution for player slot 93
	PFE_AI93,          ///< AI execution for player slot 94
	PFE_AI94,          ///< AI execution for player slot 95
	PFE_AI95,          ///< AI execution for player slot 96
	PFE_AI96,          ///< AI execution for player slot 97
	PFE_AI97,          ///< AI execution for player slot 98
	PFE_AI98,          ///< AI execution for player slot 99
	PFE_AI99,          ///< AI execution for player slot 100
	PFE_AI100,          ///< AI execution for player slot 101
	PFE_AI101,          ///< AI execution for player slot 102
	PFE_AI102,          ///< AI execution for player slot 103
	PFE_AI103,          ///< AI execution for player slot 104
	PFE_AI104,          ///< AI execution for player slot 105
	PFE_AI105,          ///< AI execution for player slot 106
	PFE_AI106,          ///< AI execution for player slot 107
	PFE_AI107,          ///< AI execution for player slot 108
	PFE_AI108,          ///< AI execution for player slot 109
	PFE_AI109,          ///< AI execution for player slot 110
	PFE_AI110,          ///< AI execution for player slot 111
	PFE_AI111,          ///< AI execution for player slot 112
	PFE_AI112,          ///< AI execution for player slot 113
	PFE_AI113,          ///< AI execution for player slot 114
	PFE_AI114,          ///< AI execution for player slot 115
	PFE_AI115,          ///< AI execution for player slot 116
	PFE_AI116,          ///< AI execution for player slot 117
	PFE_AI117,          ///< AI execution for player slot 118
	PFE_AI118,          ///< AI execution for player slot 119
	PFE_AI119,          ///< AI execution for player slot 120
	PFE_AI120,          ///< AI execution for player slot 121
	PFE_AI121,          ///< AI execution for player slot 122
	PFE_AI122,          ///< AI execution for player slot 123
	PFE_AI123,          ///< AI execution for player slot 124
	PFE_AI124,          ///< AI execution for player slot 125
	PFE_AI125,          ///< AI execution for player slot 126
	PFE_AI126,          ///< AI execution for player slot 127
	PFE_AI127,          ///< AI execution for player slot 128
	PFE_AI128,          ///< AI execution for player slot 129
	PFE_AI129,          ///< AI execution for player slot 130
	PFE_AI130,          ///< AI execution for player slot 131
	PFE_AI131,          ///< AI execution for player slot 132
	PFE_AI132,          ///< AI execution for player slot 133
	PFE_AI133,          ///< AI execution for player slot 134
	PFE_AI134,          ///< AI execution for player slot 135
	PFE_AI135,          ///< AI execution for player slot 136
	PFE_AI136,          ///< AI execution for player slot 137
	PFE_AI137,          ///< AI execution for player slot 138
	PFE_AI138,          ///< AI execution for player slot 139
	PFE_AI139,          ///< AI execution for player slot 140
	PFE_AI140,          ///< AI execution for player slot 141
	PFE_AI141,          ///< AI execution for player slot 142
	PFE_AI142,          ///< AI execution for player slot 143
	PFE_AI143,          ///< AI execution for player slot 144
	PFE_AI144,          ///< AI execution for player slot 145
	PFE_AI145,          ///< AI execution for player slot 146
	PFE_AI146,          ///< AI execution for player slot 147
	PFE_AI147,          ///< AI execution for player slot 148
	PFE_AI148,          ///< AI execution for player slot 149
	PFE_AI149,          ///< AI execution for player slot 150
	PFE_AI150,          ///< AI execution for player slot 151
	PFE_AI151,          ///< AI execution for player slot 152
	PFE_AI152,          ///< AI execution for player slot 153
	PFE_AI153,          ///< AI execution for player slot 154
	PFE_AI154,          ///< AI execution for player slot 155
	PFE_AI155,          ///< AI execution for player slot 156
	PFE_AI156,          ///< AI execution for player slot 157
	PFE_AI157,          ///< AI execution for player slot 158
	PFE_AI158,          ///< AI execution for player slot 159
	PFE_AI159,          ///< AI execution for player slot 160
	PFE_AI160,          ///< AI execution for player slot 161
	PFE_AI161,          ///< AI execution for player slot 162
	PFE_AI162,          ///< AI execution for player slot 163
	PFE_AI163,          ///< AI execution for player slot 164
	PFE_AI164,          ///< AI execution for player slot 165
	PFE_AI165,          ///< AI execution for player slot 166
	PFE_AI166,          ///< AI execution for player slot 167
	PFE_AI167,          ///< AI execution for player slot 168
	PFE_AI168,          ///< AI execution for player slot 169
	PFE_AI169,          ///< AI execution for player slot 170
	PFE_AI170,          ///< AI execution for player slot 171
	PFE_AI171,          ///< AI execution for player slot 172
	PFE_AI172,          ///< AI execution for player slot 173
	PFE_AI173,          ///< AI execution for player slot 174
	PFE_AI174,          ///< AI execution for player slot 175
	PFE_AI175,          ///< AI execution for player slot 176
	PFE_AI176,          ///< AI execution for player slot 177
	PFE_AI177,          ///< AI execution for player slot 178
	PFE_AI178,          ///< AI execution for player slot 179
	PFE_AI179,          ///< AI execution for player slot 180
	PFE_AI180,          ///< AI execution for player slot 181
	PFE_AI181,          ///< AI execution for player slot 182
	PFE_AI182,          ///< AI execution for player slot 183
	PFE_AI183,          ///< AI execution for player slot 184
	PFE_AI184,          ///< AI execution for player slot 185
	PFE_AI185,          ///< AI execution for player slot 186
	PFE_AI186,          ///< AI execution for player slot 187
	PFE_AI187,          ///< AI execution for player slot 188
	PFE_AI188,          ///< AI execution for player slot 189
	PFE_AI189,          ///< AI execution for player slot 190
	PFE_AI190,          ///< AI execution for player slot 191
	PFE_AI191,          ///< AI execution for player slot 192
	PFE_AI192,          ///< AI execution for player slot 193
	PFE_AI193,          ///< AI execution for player slot 194
	PFE_AI194,          ///< AI execution for player slot 195
	PFE_AI195,          ///< AI execution for player slot 196
	PFE_AI196,          ///< AI execution for player slot 197
	PFE_AI197,          ///< AI execution for player slot 198
	PFE_AI198,          ///< AI execution for player slot 199
	PFE_AI199,          ///< AI execution for player slot 200
	PFE_AI200,          ///< AI execution for player slot 201
	PFE_AI201,          ///< AI execution for player slot 202
	PFE_AI202,          ///< AI execution for player slot 203
	PFE_AI203,          ///< AI execution for player slot 204
	PFE_AI204,          ///< AI execution for player slot 205
	PFE_AI205,          ///< AI execution for player slot 206
	PFE_AI206,          ///< AI execution for player slot 207
	PFE_AI207,          ///< AI execution for player slot 208
	PFE_AI208,          ///< AI execution for player slot 209
	PFE_AI209,          ///< AI execution for player slot 210
	PFE_AI210,          ///< AI execution for player slot 211
	PFE_AI211,          ///< AI execution for player slot 212
	PFE_AI212,          ///< AI execution for player slot 213
	PFE_AI213,          ///< AI execution for player slot 214
	PFE_AI214,          ///< AI execution for player slot 215
	PFE_AI215,          ///< AI execution for player slot 216
	PFE_AI216,          ///< AI execution for player slot 217
	PFE_AI217,          ///< AI execution for player slot 218
	PFE_AI218,          ///< AI execution for player slot 219
	PFE_AI219,          ///< AI execution for player slot 220
	PFE_AI220,          ///< AI execution for player slot 221
	PFE_AI221,          ///< AI execution for player slot 222
	PFE_AI222,          ///< AI execution for player slot 223
	PFE_AI223,          ///< AI execution for player slot 224
	PFE_AI224,          ///< AI execution for player slot 225
	PFE_AI225,          ///< AI execution for player slot 226
	PFE_AI226,          ///< AI execution for player slot 227
	PFE_AI227,          ///< AI execution for player slot 228
	PFE_AI228,          ///< AI execution for player slot 229
	PFE_AI229,          ///< AI execution for player slot 230
	PFE_AI230,          ///< AI execution for player slot 231
	PFE_AI231,          ///< AI execution for player slot 232
	PFE_AI232,          ///< AI execution for player slot 233
	PFE_AI233,          ///< AI execution for player slot 234
	PFE_AI234,          ///< AI execution for player slot 235
	PFE_AI235,          ///< AI execution for player slot 236
	PFE_AI236,          ///< AI execution for player slot 237
	PFE_AI237,          ///< AI execution for player slot 238
	PFE_AI238,          ///< AI execution for player slot 239
	PFE_AI239,          ///< AI execution for player slot 240
	PFE_MAX,           ///< End of enum, must be last.
};

DECLARE_POSTFIX_INCREMENT(PerformanceElement)

/** Type used to hold a performance timing measurement */
typedef uint64_t TimingMeasurement;

/**
 * RAII class for measuring simple elements of performance.
 * Construct an object with the appropriate element parameter when processing begins,
 * time is automatically taken when the object goes out of scope again.
 *
 * Call Paused at the start of a frame if the processing of this element is paused.
 */
class PerformanceMeasurer {
	PerformanceElement elem;
	TimingMeasurement start_time;
public:
	PerformanceMeasurer(PerformanceElement elem);
	~PerformanceMeasurer();
	void SetExpectedRate(double rate);
	static void SetInactive(PerformanceElement elem);
	static void Paused(PerformanceElement elem);
};

/**
 * RAII class for measuring multi-step elements of performance.
 * At the beginning of a frame, call Reset on the element, then construct an object in the scope where
 * each processing cycle happens. The measurements are summed between resets.
 *
 * Usually StateGameLoop is an appropriate function to place Reset calls in, but for elements with
 * more isolated scopes it can also be appropriate to Reset somewhere else.
 * An example is the CallVehicleTicks function where all the vehicle type elements are reset.
 *
 * The PerformanceMeasurer::Paused function can also be used with elements otherwise measured with this class.
 */
class PerformanceAccumulator {
	PerformanceElement elem;
	TimingMeasurement start_time;
public:
	PerformanceAccumulator(PerformanceElement elem);
	~PerformanceAccumulator();
	static void Reset(PerformanceElement elem);
};

void ShowFramerateWindow();
void ProcessPendingPerformanceMeasurements();

#endif /* FRAMERATE_TYPE_H */
