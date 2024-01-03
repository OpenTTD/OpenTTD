/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/* @file midi.h Declarations for MIDI data */

#ifndef MUSIC_MIDI_H
#define MUSIC_MIDI_H

#include "../stdafx.h"

/** Header of a Stanard MIDI File */
struct SMFHeader {
	uint16_t format;
	uint16_t tracks;
	uint16_t tickdiv;
};

/** MIDI status byte codes */
enum MidiStatus {
	/* Bytes with top bit unset are data bytes i.e. not status bytes */
	/* Channel status messages, require channel number in lower nibble */
	MIDIST_NOTEOFF     = 0x80,
	MIDIST_NOTEON      = 0x90,
	MIDIST_POLYPRESS   = 0xA0,
	MIDIST_CONTROLLER  = 0xB0,
	MIDIST_PROGCHG     = 0xC0,
	MIDIST_CHANPRESS   = 0xD0,
	MIDIST_PITCHBEND   = 0xE0,
	/* Full byte status messages */
	MIDIST_SYSEX       = 0xF0,
	MIDIST_TC_QFRAME   = 0xF1,
	MIDIST_SONGPOSPTR  = 0xF2,
	MIDIST_SONGSEL     = 0xF3,
	/* not defined:      0xF4, */
	/* not defined:      0xF5, */
	MIDIST_TUNEREQ     = 0xF6,
	MIDIST_ENDSYSEX    = 0xF7, ///< only occurs in realtime data
	MIDIST_SMF_ESCAPE  = 0xF7, ///< only occurs in SMF data
	MIDIST_RT_CLOCK    = 0xF8,
	/* not defined:      0xF9, */
	MIDIST_RT_START    = 0xFA,
	MIDIST_RT_CONTINUE = 0xFB,
	MIDIST_RT_STOP     = 0xFC,
	/* not defined:      0xFD, */
	MIDIST_RT_ACTSENS  = 0xFE,
	MIDIST_SYSRESET    = 0xFF, ///< only occurs in realtime data
	MIDIST_SMF_META    = 0xFF, ///< only occurs in SMF data
};

/**
 * MIDI controller numbers.
 * Complete list per General MIDI, missing values are not defined.
 */
enum MidiController {
	/* Standard continuous controllers (MSB control) */
	MIDICT_BANKSELECT        =   0,
	MIDICT_MODWHEEL          =   1,
	MIDICT_BREATH            =   2,
	MIDICT_FOOT              =   4,
	MIDICT_PORTAMENTO        =   5,
	MIDICT_DATAENTRY         =   6,
	MIDICT_CHANVOLUME        =   7,
	MIDICT_BALANCE           =   8,
	MIDICT_PAN               =  10,
	MIDICT_EXPRESSION        =  11,
	MIDICT_EFFECT1           =  12,
	MIDICT_EFFECT2           =  13,
	MIDICT_GENERAL1          =  16,
	MIDICT_GENERAL2          =  17,
	MIDICT_GENERAL3          =  18,
	MIDICT_GENERAL4          =  19,
	/* Offset from MSB to LSB of continuous controllers */
	MIDICTOFS_HIGHRES        =  32,
	/* Stanard continuous controllers (LSB control) */
	MIDICT_BANKSELECT_LO     = MIDICTOFS_HIGHRES + MIDICT_BANKSELECT,
	MIDICT_MODWHEEL_LO       = MIDICTOFS_HIGHRES + MIDICT_MODWHEEL,
	MIDICT_BREATH_LO         = MIDICTOFS_HIGHRES + MIDICT_BREATH,
	MIDICT_FOOT_LO           = MIDICTOFS_HIGHRES + MIDICT_FOOT,
	MIDICT_PORTAMENTO_LO     = MIDICTOFS_HIGHRES + MIDICT_PORTAMENTO,
	MIDICT_DATAENTRY_LO      = MIDICTOFS_HIGHRES + MIDICT_DATAENTRY,
	MIDICT_CHANVOLUME_LO     = MIDICTOFS_HIGHRES + MIDICT_CHANVOLUME,
	MIDICT_BALANCE_LO        = MIDICTOFS_HIGHRES + MIDICT_BALANCE,
	MIDICT_PAN_LO            = MIDICTOFS_HIGHRES + MIDICT_PAN,
	MIDICT_EXPRESSION_LO     = MIDICTOFS_HIGHRES + MIDICT_EXPRESSION,
	MIDICT_EFFECT1_LO        = MIDICTOFS_HIGHRES + MIDICT_EFFECT1,
	MIDICT_EFFECT2_LO        = MIDICTOFS_HIGHRES + MIDICT_EFFECT2,
	MIDICT_GENERAL1_LO       = MIDICTOFS_HIGHRES + MIDICT_GENERAL1,
	MIDICT_GENERAL2_LO       = MIDICTOFS_HIGHRES + MIDICT_GENERAL2,
	MIDICT_GENERAL3_LO       = MIDICTOFS_HIGHRES + MIDICT_GENERAL3,
	MIDICT_GENERAL4_LO       = MIDICTOFS_HIGHRES + MIDICT_GENERAL4,
	/* Switch controllers */
	MIDICT_SUSTAINSW         =  64,
	MIDICT_PORTAMENTOSW      =  65,
	MIDICT_SOSTENUTOSW       =  66,
	MIDICT_SOFTPEDALSW       =  67,
	MIDICT_LEGATOSW          =  68,
	MIDICT_HOLD2SW           =  69,
	/* Standard low-resolution controllers */
	MIDICT_SOUND1            =  70,
	MIDICT_SOUND2            =  71,
	MIDICT_SOUND3            =  72,
	MIDICT_SOUND4            =  73,
	MIDICT_SOUND5            =  74,
	MIDICT_SOUND6            =  75,
	MIDICT_SOUND7            =  76,
	MIDICT_SOUND8            =  77,
	MIDICT_SOUND9            =  78,
	MIDICT_SOUND10           =  79,
	MIDICT_GENERAL5          =  80,
	MIDICT_GENERAL6          =  81,
	MIDICT_GENERAL7          =  82,
	MIDICT_GENERAL8          =  83,
	MIDICT_PORTAMENTOCTL     =  84,
	MIDICT_PRF_HIGHRESVEL    =  88,
	MIDICT_EFFECTS1          =  91,
	MIDICT_EFFECTS2          =  92,
	MIDICT_EFFECTS3          =  93,
	MIDICT_EFFECTS4          =  94,
	MIDICT_EFFECTS5          =  95,
	/* Registered/unregistered parameters control */
	MIDICT_DATA_INCREMENT    =  96,
	MIDICT_DATA_DECREMENT    =  97,
	MIDICT_NRPN_SELECT_LO    =  98,
	MIDICT_NRPN_SELECT_HI    =  99,
	MIDICT_RPN_SELECT_LO     = 100,
	MIDICT_RPN_SELECT_HI     = 101,
	/* Channel mode messages */
	MIDICT_MODE_ALLSOUNDOFF  = 120,
	MIDICT_MODE_RESETALLCTRL = 121,
	MIDICT_MODE_LOCALCTL     = 122,
	MIDICT_MODE_ALLNOTESOFF  = 123,
	MIDICT_MODE_OMNI_OFF     = 124,
	MIDICT_MODE_OMNI_ON      = 125,
	MIDICT_MODE_MONO         = 126,
	MIDICT_MODE_POLY         = 127,
};


/** Well-known MIDI system exclusive message values for use with the MidiGetStandardSysexMessage function. */
enum class MidiSysexMessage {
	/** Reset device to General MIDI defaults */
	ResetGM,
	/** Reset device to (Roland) General Standard defaults */
	ResetGS,
	/** Reset device to (Yamaha) XG defaults */
	ResetXG,
	/** Set up Roland SoundCanvas reverb room as TTD does */
	RolandSetReverb,
};

const byte *MidiGetStandardSysexMessage(MidiSysexMessage msg, size_t &length);

#endif /* MUSIC_MIDI_H */
