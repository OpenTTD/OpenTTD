/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/* @file midifile.hpp Parser for standard MIDI files */

#ifndef MUSIC_MIDIFILE_HPP
#define MUSIC_MIDIFILE_HPP

#include "../stdafx.h"
#include "midi.h"

struct MusicSongInfo;

struct MidiFile {
	struct DataBlock {
		uint32_t ticktime;        ///< tick number since start of file this block should be triggered at
		uint32_t realtime;        ///< real-time (microseconds) since start of file this block should be triggered at
		std::vector<byte> data; ///< raw midi data contained in block
		DataBlock(uint32_t _ticktime = 0) : ticktime(_ticktime) { }
	};
	struct TempoChange {
		uint32_t ticktime; ///< tick number since start of file this tempo change occurs at
		uint32_t tempo;    ///< new tempo in microseconds per tick
		TempoChange(uint32_t _ticktime, uint32_t _tempo) : ticktime(_ticktime), tempo(_tempo) { }
	};

	std::vector<DataBlock> blocks;   ///< sequential time-annotated data of file, merged to a single track
	std::vector<TempoChange> tempos; ///< list of tempo changes in file
	uint16_t tickdiv;                  ///< ticks per quarter note

	MidiFile();
	~MidiFile();

	bool LoadFile(const std::string &filename);
	bool LoadMpsData(const byte *data, size_t length);
	bool LoadSong(const MusicSongInfo &song);
	void MoveFrom(MidiFile &other);

	bool WriteSMF(const std::string &filename);

	static std::string GetSMFFile(const MusicSongInfo &song);
	static bool ReadSMFHeader(const std::string &filename, SMFHeader &header);
	static bool ReadSMFHeader(FILE *file, SMFHeader &header);
};

#endif /* MUSIC_MIDIFILE_HPP */
