/* $Id$ */

/*
* This file is part of OpenTTD.
* OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
* OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
*/

/* @file midifile.cpp Parser for standard MIDI files */

#include "midifile.hpp"
#include "../fileio_func.h"
#include "../fileio_type.h"
#include "../string_func.h"
#include "../core/endian_func.hpp"
#include "../base_media_base.h"
#include <algorithm>

#include "../console_func.h"
#include "../console_internal.h"


/* SMF reader based on description at: http://www.somascape.org/midi/tech/mfile.html */


MidiFile *_midifile_instance = NULL;

enum MidiStatus {
	// These take a channel number in the lower nibble
	MIDIST_NOTEOFF    = 0x80,
	MIDIST_NOTEON     = 0x90,
	MIDIST_POLYPRESS  = 0xA0,
	MIDIST_CONTROLLER = 0xB0,
	MIDIST_PROGCHG    = 0xC0,
	MIDIST_CHANPRESS  = 0xD0,
	MIDIST_PITCHBEND  = 0xE0,
	// These are full byte status codes
	MIDIST_SYSEX      = 0xF0,
	MIDIST_ENDSYSEX   = 0xF7, ///< only occurs in realtime data
	MIDIST_SMF_ESCAPE = 0xF7, ///< only occurs in SMF data
	MIDIST_TC_QFRAME  = 0xF1,
	MIDIST_SONGPOSPTR = 0xF2,
	MIDIST_SONGSEL    = 0xF3,
	MIDIST_TUNEREQ    = 0xF6,
	MIDIST_RT_CLOCK   = 0xF8,
	MIDIST_SYSRESET   = 0xFF, ///< only occurs in realtime data
	MIDIST_SMF_META   = 0xFF, ///< only occurs in SMF data
};
enum MidiController {
	// Controller number for MSB of high-res controllers
	MIDICT_BANKSELECT = 0,
	MIDICT_MODWHEEL,
	MIDICT_BREATH,
	MIDICT_FOOT,
	MIDICT_PORTAMENTO,
	MIDICT_DATAENTRY,
	MIDICT_CHANVOLUME,
	MIDICT_BALANCE,
	MIDICT_PAN,
	MIDICT_EXPRESSION,
	MIDICT_EFFECT1,
	MIDICT_EFFECT2,
	MIDICT_GENERAL1,
	MIDICT_GENERAL2,
	MIDICT_GENERAL3,
	MIDICT_GENERAL4,
	// Offset to add to MSB controller number to get LSB controller number
	MIDICTOFS_HIGHRES = 32,
	// Switch controllers
	MIDICT_SUSTAINSW = 64,
	MIDICT_PORTAMENTOSW,
	MIDICT_SOSTENUTOSW,
	MIDICT_SOFTPEDALSW,
	MIDICT_LEGATOSW,
	MIDICT_HOLD2SW,
	// Channel mode messages
	MIDICT_MODE_ALLSOUNDOFF = 120,
	MIDICT_MODE_RESETALLCTRL,
	MIDICT_MODE_LOCALCTL,
	MIDICT_MODE_ALLNOTESOFF,
	MIDICT_MODE_OMNI_OFF,
	MIDICT_MODE_OMNI_ON,
	MIDICT_MODE_MONO,
	MIDICT_MODE_POLY,
};

/**
 * Owning byte buffer readable as a stream.
 * RAII-compliant to make teardown in error situations easier.
 */
class ByteBuffer {
	byte *buf;
	size_t buflen;
	size_t pos;
public:
	/**
	 * Construct buffer from data in a file.
	 * If file does not have sufficient bytes available, the object is constructed
	 * in an error state, that causes all further function calls to fail.
	 * @param file file to read from at current position
	 * @param len number of bytes to read
	 */
	ByteBuffer(FILE *file, size_t len)
	{
		this->buf = MallocT<byte>(len);
		if (fread(this->buf, 1, len, file) == len) {
			this->buflen = len;
			this->pos = 0;
		} else {
			/* invalid state */
			this->buflen = 0;
		}
	}

	/**
	 * Destructor, frees the buffer.
	 */
	~ByteBuffer()
	{
		free(this->buf);
	}

	/**
	 * Return whether the buffer was constructed successfully.
	 * @return true is the buffer contains data
	 */
	bool IsValid() const
	{
		return this->buflen > 0;
	}

	/**
	 * Return whether reading has reached the end of the buffer.
	 * @return true if there are no more bytes available to read
	 */
	bool IsEnd() const
	{
		return this->pos >= this->buflen;
	}

	/**
	 * Read a single byte from the buffer.
	 * @param[out] b returns the read value
	 * @return true if a byte was available for reading
	 */
	bool ReadByte(byte &b)
	{
		if (this->IsEnd()) return false;
		b = this->buf[this->pos++];
		return true;
	}

	/**
	 * Read a MIDI file variable length value.
	 * Each byte encodes 7 bits of the value, most-significant bits are encoded first.
	 * If the most significant bit in a byte is set, there are further bytes encoding the value.
	 * @param[out] res returns the read value
	 * @return true if there was data available
	 */
	bool ReadVariableLength(uint32 &res)
	{
		res = 0;
		byte b = 0;
		do {
			if (this->IsEnd()) return false;
			b = this->buf[this->pos++];
			res = (res << 7) | (b & 0x7F);
		} while (b & 0x80);
		return true;
	}

	/**
	 * Read bytes into a buffer.
	 * @param[out] dest buffer to copy info
	 * @param length number of bytes to read
	 * @return true if the requested number of bytes were available
	 */
	bool ReadBuffer(byte *dest, size_t length)
	{
		if (this->IsEnd()) return false;
		if (this->buflen - this->pos < length) return false;
		memcpy(dest, this->buf + this->pos, length);
		this->pos += length;
		return true;
	}

	/**
	 * Skip over a number of bytes in the buffer.
	 * @param count number of bytes to skip over
	 * @return true if there were enough bytes available
	 */
	bool Skip(size_t count)
	{
		if (this->IsEnd()) return false;
		if (this->buflen - this->pos < count) return false;
		this->pos += count;
		return true;
	}

	/**
	 * Go a number of bytes back to re-read.
	 * @param count number of bytes to go back
	 * @return true if at least count bytes had been read previously
	 */
	bool Rewind(size_t count)
	{
		if (count > this->pos) return false;
		this->pos -= count;
		return true;
	}
};

static bool ReadTrackChunk(FILE *file, MidiFile &target)
{
	byte buf[4];

	const byte magic[] = { 'M', 'T', 'r', 'k' };
	if (fread(buf, sizeof(magic), 1, file) != 1) {
		return false;
	}
	if (memcmp(magic, buf, sizeof(magic)) != 0) {
		return false;
	}

	/* Read chunk length and then the whole chunk */
	uint32 chunk_length;
	if (fread(&chunk_length, 1, 4, file) != 4) {
		return false;
	}
	chunk_length = FROM_BE32(chunk_length);

	size_t file_pos = ftell(file);
	ByteBuffer chunk(file, chunk_length);
	if (!chunk.IsValid()) {
		return false;
	}

	target.blocks.push_back(MidiFile::DataBlock());
	MidiFile::DataBlock *block = &target.blocks.back();

	byte last_status = 0;
	bool running_sysex = false;
	while (!chunk.IsEnd()) {
		/* Read deltatime for event, start new block */
		uint32 deltatime = 0;
		if (!chunk.ReadVariableLength(deltatime)) {
			return false;
		}
		if (deltatime > 0) {
			target.blocks.push_back(MidiFile::DataBlock(block->ticktime + deltatime));
			block = &target.blocks.back();
		}

		/* Read status byte */
		byte status;
		if (!chunk.ReadByte(status)) {
			return false;
		}

		if ((status & 0x80) == 0) {
			/* High bit not set means running status message, status is same as last
			 * convert to explicit status */
			chunk.Rewind(1);
			status = last_status;
			goto running_status;
		} else if ((status & 0xF0) != 0xF0) {
			/* Regular channel message */
			last_status = status;
		running_status:
			byte *data;
			switch (status & 0xF0) {
			case MIDIST_NOTEOFF:
			case MIDIST_NOTEON:
			case MIDIST_POLYPRESS:
			case MIDIST_CONTROLLER:
			case MIDIST_PITCHBEND:
				/* 3 byte messages */
				data = block->data.Append(3);
				data[0] = status;
				if (!chunk.ReadBuffer(&data[1], 2)) {
					return false;
				}
				break;
			case MIDIST_PROGCHG:
			case MIDIST_CHANPRESS:
				/* 2 byte messages */
				data = block->data.Append(2);
				data[0] = status;
				if (!chunk.ReadByte(data[1])) {
					return false;
				}
				break;
			default:
				NOT_REACHED();
			}
		} else if (status == MIDIST_SMF_META) {
			/* Meta event, read event type byte and data length */
			if (!chunk.ReadByte(buf[0])) {
				return false;
			}
			uint32 length = 0;
			if (!chunk.ReadVariableLength(length)) {
				return false;
			}
			switch (buf[0]) {
			case 0x2F:
				/* End of track, no more data */
				if (length != 0)
					return false;
				else
					return true;
			case 0x51:
				/* Tempo change */
				if (length != 3)
					return false;
				if (!chunk.ReadBuffer(buf, 3)) return false;
				target.tempos.push_back(MidiFile::TempoChange(block->ticktime, buf[0] << 16 | buf[1] << 8 | buf[2]));
				break;
			default:
				/* Unimportant meta event, skip over it */
				if (!chunk.Skip(length)) {
					return false;
				}
				break;
			}
		} else if (status == MIDIST_SYSEX || (status == MIDIST_SMF_ESCAPE && running_sysex)) {
			/* System exclusive message */
			uint32 length = 0;
			if (!chunk.ReadVariableLength(length)) return false;
			byte *data = block->data.Append(length + 1);
			data[0] = 0xF0;
			if (!chunk.ReadBuffer(data + 1, length)) return false;
			if (data[length] != 0xF7) {
				/* Engage Casio weirdo mode - convert to normal sysex */
				running_sysex = true;
				*block->data.Append() = 0xF7;
			} else {
				running_sysex = false;
			}
		} else if (status == MIDIST_SMF_ESCAPE) {
			/* Escape sequence */
			uint32 length = 0;
			if (!chunk.ReadVariableLength(length)) return false;
			byte *data = block->data.Append(length);
			if (!chunk.ReadBuffer(data, length)) return false;
		} else {
			/* Messages undefined in standard midi files:
			 * 0xF1 - MIDI time code quarter frame
			 * 0xF2 - Song position pointer
			 * 0xF3 - Song select
			 * 0xF4 - undefined/reserved
			 * 0xF5 - undefined/reserved
			 * 0xF6 - Tune request for analog synths
			 * 0xF8..0xFE - System real-time messages
			 */
			return false;
		}
	}

	NOT_REACHED();
}

template<typename T>
bool TicktimeAscending(const T &a, const T &b)
{
	return a.ticktime < b.ticktime;
}

static bool FixupMidiData(MidiFile &target)
{
	/* Sort all tempo changes and events */
	std::sort(target.tempos.begin(), target.tempos.end(), TicktimeAscending<MidiFile::TempoChange>);
	std::sort(target.blocks.begin(), target.blocks.end(), TicktimeAscending<MidiFile::DataBlock>);

	if (target.tempos.size() == 0) {
		/* No tempo information, assume 120 bpm (500,000 microseconds per beat */
		target.tempos.push_back(MidiFile::TempoChange(0, 500000));
	}
	/* Add sentinel tempo at end */
	target.tempos.push_back(MidiFile::TempoChange(UINT32_MAX, 0));

	/* Merge blocks with identical tick times */
	std::vector<MidiFile::DataBlock> merged_blocks;
	uint32 last_ticktime = 0;
	for (size_t i = 0; i < target.blocks.size(); i++) {
		MidiFile::DataBlock &block = target.blocks[i];
		if (block.data.Length() == 0) {
			continue;
		} else if (block.ticktime > last_ticktime || merged_blocks.size() == 0) {
			merged_blocks.push_back(block);
			last_ticktime = block.ticktime;
		} else {
			byte *datadest = merged_blocks.back().data.Append(block.data.Length());
			memcpy(datadest, block.data.Begin(), block.data.Length());
		}
	}
	std::swap(merged_blocks, target.blocks);

	/* Annotate blocks with real time */
	last_ticktime = 0;
	uint32 last_realtime = 0;
	size_t cur_tempo = 0, cur_block = 0;
	while (cur_block < target.blocks.size()) {
		MidiFile::DataBlock &block = target.blocks[cur_block];
		MidiFile::TempoChange &tempo = target.tempos[cur_tempo];
		MidiFile::TempoChange &next_tempo = target.tempos[cur_tempo+1];
		if (block.ticktime <= next_tempo.ticktime) {
			/* block is within the current tempo */
			int64 tickdiff = block.ticktime - last_ticktime;
			last_ticktime = block.ticktime;
			last_realtime += uint32(tickdiff * tempo.tempo / target.tickdiv);
			block.realtime = last_realtime;
			cur_block++;
		} else {
			/* tempo change occurs before this block */
			int64 tickdiff = next_tempo.ticktime - last_ticktime;
			last_ticktime = next_tempo.ticktime;
			last_realtime += uint32(tickdiff * tempo.tempo / target.tickdiv); // current tempo until the tempo change
			cur_tempo++;
		}
	}

	return true;
}

/**
 * Read the header of a standard MIDI file.
 * @param[in] filename name of file to read from
 * @param[out] header filled with data read
 * @return true if the file could be opened and contained a header with correct format
 */
bool MidiFile::ReadSMFHeader(const char *filename, SMFHeader &header)
{
	FILE *file = FioFOpenFile(filename, "rb", Subdirectory::BASESET_DIR);
	if (!file) return false;
	bool result = ReadSMFHeader(file, header);
	FioFCloseFile(file);
	return result;
}

/**
 * Read the header of a standard MIDI file.
 * The function will consume 14 bytes from the current file pointer position.
 * @param[in] file open file to read from (should be in binary mode)
 * @param[out] header filled with data read
 * @return true if a header in correct format could be read from the file
 */
bool MidiFile::ReadSMFHeader(FILE *file, SMFHeader &header)
{
	/* Try to read header, fixed size */
	byte buffer[14];
	if (fread(buffer, sizeof(buffer), 1, file) != 1)
		return false;

	/* Check magic, 'MThd' followed by 4 byte length indicator (always = 6 in SMF) */
	const byte magic[] = { 'M', 'T', 'h', 'd', 0x00, 0x00, 0x00, 0x06 };
	if (MemCmpT(buffer, magic, sizeof(magic)) != 0)
		return false;

	/* Read the parameters of the file */
	header.format = (buffer[8] << 8) | buffer[9];
	header.tracks = (buffer[10] << 8) | buffer[11];
	header.tickdiv = (buffer[12] << 8) | buffer[13];
	return true;
}

/**
 * Load a standard MIDI file.
 * @param filename name of the file to load
 * @returns true if loaded was successful
 */
bool MidiFile::LoadFile(const char *filename)
{
	_midifile_instance = this;

	this->blocks.clear();
	this->tempos.clear();
	this->tickdiv = 0;

	bool success = false;
	FILE *file = FioFOpenFile(filename, "rb", Subdirectory::BASESET_DIR);
	if (file == NULL) return false;

	SMFHeader header;
	if (!ReadSMFHeader(file, header)) goto cleanup;

	/* Only format 0 (single-track) and format 1 (multi-track single-song) are accepted for now */
	if (header.format != 0 && header.format != 1) goto cleanup;
	/* Doesn't support SMPTE timecode files */
	if ((header.tickdiv & 0x8000) != 0) goto cleanup;

	this->tickdiv = header.tickdiv;

	for (; header.tracks > 0; header.tracks--) {
		if (!ReadTrackChunk(file, *this)) {
			goto cleanup;
		}
	}

	success = FixupMidiData(*this);

cleanup:
	FioFCloseFile(file);
	return success;
}


struct MpsMachine {
	struct Channel {
		byte cur_program;    ///< program selected, used for velocity scaling (lookup into programvelocities array)
		byte running_status; ///< last midi status code seen
		uint16 delay;        ///< frames until next command
		uint32 playpos;      ///< next byte to play this channel from
		uint32 startpos;     ///< start position of master track
		uint32 returnpos;    ///< next return position after playing a segment
		Channel() : cur_program(0xFF), running_status(0), delay(0), playpos(0), startpos(0), returnpos(0) { }
	};
	Channel channels[16];
	std::vector<uint32> segments; ///< pointers into songdata to repeatable data segments
	int16 tempo_ticks;            ///< ticker that increments when playing a frame, decrements before playing a frame
	uint16 song_tempo;            ///< threshold for actually playing a frame
	bool shouldplayflag;          ///< not-end-of-song

	static const byte programvelocities[128];
	static int glitch[2];

	const byte *songdata; ///< raw data array
	size_t songdatalen;   ///< length of song data
	MidiFile &target;     ///< recipient of data

	static void AddMidiData(MidiFile::DataBlock &block, byte b1, byte b2)
	{
		*block.data.Append() = b1;
		*block.data.Append() = b2;
	}
	static void AddMidiData(MidiFile::DataBlock &block, byte b1, byte b2, byte b3)
	{
		*block.data.Append() = b1;
		*block.data.Append() = b2;
		*block.data.Append() = b3;
	}

	/**
	 * Construct a TTD DOS music format decoder.
	 * @param songdata Buffer of song data from CAT file, ownership remains with caller
	 * @param songdatalen Length of the data buffer in bytes
	 * @param target MidiFile object to add decoded data to
	 */
	MpsMachine(const byte *data, size_t length, MidiFile &target)
		: songdata(data), songdatalen(length), target(target)
	{
		uint32 pos = 0;
		int loopmax;
		int loopidx;

		/* First byte is the initial "tempo" */
		this->song_tempo = this->songdata[pos++];

		/* Next byte is a count of callable segments */
		loopmax = this->songdata[pos++];
		for (loopidx = 0; loopidx < loopmax; loopidx++) {
			/* Segments form a linked list in the stream,
			 * first two bytes in each is an offset to the next.
			 * Two bytes between offset to next and start of data
			 * are unaccounted for. */
			this->segments.push_back(pos + 4);
			pos += FROM_LE16(*(const int16 *)(this->songdata + pos));
		}

		/* After segments follows list of master tracks for each channel,
		 * also prefixed with a byte counting actual tracks. */
		loopmax = this->songdata[pos++];
		for (loopidx = 0; loopidx < loopmax; loopidx++) {
			/* Similar structure to segments list, but also has
			 * the MIDI channel number as a byte before the offset
			 * to next track. */
			byte ch = this->songdata[pos++];
			this->channels[ch].startpos = pos + 4;
			pos += FROM_LE16(*(const int16 *)(this->songdata + pos));
		}
	}

	/**
	 * Read an SMF-style variable length value (note duration) from songdata.
	 * @param pos Position to read from, updated to point to next byte after the value read
	 * @return Value read from data stream
	 */
	uint16 ReadVariableLength(uint32 &pos)
	{
		byte b = 0;
		uint16 res = 0;
		do {
			b = this->songdata[pos++];
			res = (res << 7) + (b & 0x7F);
		} while (b & 0x80);
		return res;
	}

	/**
	 * Prepare for playback from the beginning. Resets the song pointer for every track to the beginning.
	 */
	void RestartSong()
	{
		for (int ch = 0; ch < 16; ch++) {
			Channel &chandata = this->channels[ch];
			if (chandata.startpos != 0) {
				/* Active track, set position to beginning */
				chandata.playpos = chandata.startpos;
				chandata.delay = this->ReadVariableLength(chandata.playpos);
			} else {
				/* Inactive track, mark as such */
				chandata.playpos = 0;
				chandata.delay = 0;
			}
		}
	}

	/**
	 * Play one frame of data from one channel
	 */
	uint16 PlayChannelFrame(MidiFile::DataBlock &outblock, int channel)
	{
		uint16 newdelay = 0;
		byte b1, b2;
		Channel &chandata = this->channels[channel];

		do {
			/* Read command/status byte */
			b1 = this->songdata[chandata.playpos++];

			/* Command 0xFE, call segment from master track */
			if (b1 == 0xFE) {
				b1 = this->songdata[chandata.playpos++];
				chandata.returnpos = chandata.playpos;
				/* "glitch" values can cause intentional misplaying, try it :) */
				chandata.playpos = this->segments[(b1 * glitch[1] + glitch[0]) % this->segments.size()];
				newdelay = this->ReadVariableLength(chandata.playpos);
				if (newdelay == 0) {
					continue;
				}
				return newdelay;
			}

			/* Command 0xFD, return from segment to master track */
			if (b1 == 0xFD) {
				chandata.playpos = chandata.returnpos;
				chandata.returnpos = 0;
				newdelay = this->ReadVariableLength(chandata.playpos);
				if (newdelay == 0) {
					continue;
				}
				return newdelay;
			}

			/* Command 0xFF, end of song */
			if (b1 == 0xFF) {
				this->shouldplayflag = false;
				return 0;
			}

			/* Regular MIDI channel message status byte */
			if (b1 >= 0x80) {
				/* Save the status byte as running status for the channel
				 * and read another byte for first parameter to command */
				chandata.running_status = b1;
				b1 = this->songdata[chandata.playpos++];
			}

			switch (chandata.running_status & 0xF0) {
			case MIDIST_NOTEOFF:
			case MIDIST_NOTEON:
				b2 = this->songdata[chandata.playpos++];
				if (b2 != 0) {
					/* Note on, read velocity and scale according to rules */
					int16 velocity;
					if (channel == 9) {
						/* Percussion */
						velocity = (int16)b2 * 0x50;
					} else {
						/* Regular channel */
						velocity = b2 * programvelocities[chandata.cur_program];
					}
					b2 = (velocity / 128) & 0x00FF;
					AddMidiData(outblock, MIDIST_NOTEON + channel, b1, b2);
				} else {
					/* Note off */
					AddMidiData(outblock, MIDIST_NOTEON + channel, b1, 0);
				}
				break;
			case MIDIST_CONTROLLER:
				b2 = this->songdata[chandata.playpos++];
				if (b1 == 0x7E) {
					/* Unknown what the purpose of this is.
					 * Occurs in "Can't get There from Here" and
					 * in "Aliens Ate my Railway" a few times each.
					 * General MIDI controller 0x7E is "Mono mode on"
					 * so maybe intended for more limited synths.
					 */
					break;
				} else if (b1 == 0) {
					/* Special case for tempo change, usually
					 * controller 0 is "Bank select". */
					if (b2 != 0) {
						this->song_tempo = ((int)b2) * 48 / 60;
					}
					break;
				} else if (b1 == 0x5B) {
					/* Controller 0x5B is "Reverb send level",
					 * this just enables reverb to a fixed level. */
					b2 = 0x1E;
				}
				AddMidiData(outblock, MIDIST_CONTROLLER + channel, b1, b2);
				break;
			case MIDIST_PROGCHG:
				if (b1 == 0x7E) {
					/* Program change to "Applause" is originally used
					 * to cause the song to loop, but that gets handled
					 * separately in the output driver here.
					 * Just end the song. */
					this->shouldplayflag = false;
					break;
				}
				/* Used for note velocity scaling lookup */
				chandata.cur_program = b1;
				/* Two programs translated to a third, this is likely to
				 * provide three different velocity scalings of "brass". */
				if (b1 == 0x57 || b1 == 0x3F) {
					b1 = 0x3E;
				}
				AddMidiData(outblock, MIDIST_PROGCHG + channel, b1);
				break;
			case MIDIST_PITCHBEND:
				b2 = this->songdata[chandata.playpos++];
				AddMidiData(outblock, MIDIST_PITCHBEND + channel, b1, b2);
				break;
			default:
				break;
			}

			newdelay = this->ReadVariableLength(chandata.playpos);
		} while (newdelay == 0);

		return newdelay;
	}

	/**
	 * Play one frame of data into a block.
	 */
	bool PlayFrame(MidiFile::DataBlock &block)
	{
		/* Update tempo/ticks counter */
		this->tempo_ticks -= this->song_tempo;
		if (this->tempo_ticks > 0) {
			return true;
		}
		this->tempo_ticks += 148;

		/* Look over all channels, play those active */
		for (int ch = 0; ch < 16; ch++) {
			Channel &chandata = this->channels[ch];
			if (chandata.playpos != 0) {
				if (chandata.delay == 0) {
					chandata.delay = this->PlayChannelFrame(block, ch);
				}
				chandata.delay--;
			}
		}

		return this->shouldplayflag;
	}

	/**
	 * Perform playback of whole song.
	 */
	bool PlayInto()
	{
		/* Guessed values based on what sounds right.
		 * A tickdiv of 96 is common, and 6.5 ms per tick
		 * leads to an initial musical tempo of ~96 bpm.
		 * However this has very little relation to the actual
		 * tempo of the music, since that gets controlled by
		 * the "other" tempo values read from the data. How
		 * those values relate to actual musical tempo has
		 * yet to be discovered. */
		this->target.tickdiv = 96;
		this->target.tempos.push_back(MidiFile::TempoChange(0, 6500*this->target.tickdiv));

		/* Initialize playback simulation */
		this->RestartSong();
		this->shouldplayflag = true;
		this->song_tempo = (int32)this->song_tempo * 24 / 60;
		this->tempo_ticks = this->song_tempo;

		/* Always reset percussion channel to program 0 */
		this->target.blocks.push_back(MidiFile::DataBlock());
		AddMidiData(this->target.blocks.back(), MIDIST_PROGCHG+9, 0x00);

		/* Technically should be an endless loop, but having
		 * a maximum (about 10 minutes) avoids getting stuck,
		 * in case of corrupted data. */
		for (uint32 tick = 0; tick < 100000; tick+=1) {
			this->target.blocks.push_back(MidiFile::DataBlock());
			auto &block = this->target.blocks.back();
			block.ticktime = tick;
			if (!this->PlayFrame(block)) {
				break;
			}
		}
		return true;
	}
};
/* Base note velocities for various GM programs */
const byte MpsMachine::programvelocities[128] = {
	100,100,100,100,100, 90,100,100,100,100,100, 90,100,100,100,100,
	100,100, 85,100,100,100,100,100,100,100,100,100, 90, 90,110, 80,
	100,100,100, 90, 70,100,100,100,100,100,100,100,100,100,100,100,
	100,100, 90,100,100,100,100,100,100,120,100,100,100,120,100,127,
	100,100, 90,100,100,100,100,100,100, 95,100,100,100,100,100,100,
	100,100,100,100,100,100,100,115,100,100,100,100,100,100,100,100,
	100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,
	100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,
};
int MpsMachine::glitch[2] = { 0, 1 };

/**
 * Create MIDI data from song data for the original Microprose music drivers.
 * @param data pointer to block of data
 * @param length size of data in bytes
 * @return true if the data could be loaded
 */
bool MidiFile::LoadMpsData(const byte *data, size_t length)
{
	_midifile_instance = this;

	MpsMachine machine(data, length, *this);
	return machine.PlayInto() && FixupMidiData(*this);
}

/**
 * Move data from other to this, and clears other.
 * @param other object containing loaded data to take over
 */
void MidiFile::MoveFrom(MidiFile &other)
{
	std::swap(this->blocks, other.blocks);
	std::swap(this->tempos, other.tempos);
	this->tickdiv = other.tickdiv;

	_midifile_instance = this;

	other.blocks.clear();
	other.tempos.clear();
	other.tickdiv = 0;
}

static void WriteVariableLen(FILE *f, uint32 value)
{
	if (value < 0x7F) {
		byte tb = value;
		fwrite(&tb, 1, 1, f);
	} else if (value < 0x3FFF) {
		byte tb[2];
		tb[1] =  value & 0x7F;         value >>= 7;
		tb[0] = (value & 0x7F) | 0x80; value >>= 7;
		fwrite(tb, 1, sizeof(tb), f);
	} else if (value < 0x1FFFFF) {
		byte tb[3];
		tb[2] =  value & 0x7F;         value >>= 7;
		tb[1] = (value & 0x7F) | 0x80; value >>= 7;
		tb[0] = (value & 0x7F) | 0x80; value >>= 7;
		fwrite(tb, 1, sizeof(tb), f);
	} else if (value < 0x0FFFFFFF) {
		byte tb[4];
		tb[3] =  value & 0x7F;         value >>= 7;
		tb[2] = (value & 0x7F) | 0x80; value >>= 7;
		tb[1] = (value & 0x7F) | 0x80; value >>= 7;
		tb[0] = (value & 0x7F) | 0x80; value >>= 7;
		fwrite(tb, 1, sizeof(tb), f);
	}
}

/**
 * Write a Standard MIDI File containing the decoded music.
 * @param filename Name of file to write to
 * @return True if the file was written to completion
 */
bool MidiFile::WriteSMF(const char *filename)
{
	FILE *f = FioFOpenFile(filename, "wb", Subdirectory::NO_DIRECTORY);
	if (!f) {
		return false;
	}

	uint16 u16;
	byte bb;

	/* SMF header */
	const byte filemagic[] = { 'M', 'T', 'h', 'd', 0x00, 0x00, 0x00, 0x06 };
	fwrite(filemagic, sizeof(filemagic), 1, f);
	fwrite(&(u16 = 0), sizeof(u16), 1, f);
	fwrite(&(u16 = TO_BE16(1)), sizeof(u16), 1, f);
	fwrite(&(u16 = TO_BE16(this->tickdiv)), sizeof(u16), 1, f);

	/* Track header, block length is written after everything
	 * else has already been written and length is known. */
	const byte trackmagic[] = { 'M', 'T', 'r', 'k', 0, 0, 0, 0 };
	fwrite(trackmagic, sizeof(trackmagic), 1, f);
	size_t tracksizepos = ftell(f) - 4;

	/* Write blocks in sequence */
	uint32 lasttime = 0;
	size_t nexttempoindex = 0;
	for (size_t bi = 0; bi < this->blocks.size(); bi++) {
	redoblock:
		DataBlock &block = this->blocks[bi];
		TempoChange &nexttempo = this->tempos[nexttempoindex];

		uint32 timediff = block.ticktime - lasttime;

		/* Check if there is a tempo change before this block */
		if (nexttempo.ticktime < block.ticktime) {
			timediff = nexttempo.ticktime - lasttime;
		}

		/* Write delta time for block */
		lasttime += timediff;
		bool needtime = false;
		WriteVariableLen(f, timediff);

		/* Write tempo change if there is one */
		if (nexttempo.ticktime <= block.ticktime) {
			byte tempobuf[6] = { MIDIST_SMF_META, 0x51, 0x03, 0, 0, 0 };
			tempobuf[3] = (nexttempo.tempo & 0x00FF0000) >> 16;
			tempobuf[4] = (nexttempo.tempo & 0x0000FF00) >>  8;
			tempobuf[5] = (nexttempo.tempo & 0x000000FF);
			fwrite(tempobuf, 1, 6, f);
			nexttempoindex++;
			needtime = true;
		}
		/* If a tempo change occurred between two blocks, rather than
		 * at start of this one, start over with delta time for the block. */
		if (nexttempo.ticktime < block.ticktime) {
			goto redoblock;
		}

		/* Write each block data command */
		byte *dp = block.data.Begin();
		while (dp < block.data.End()) {
			/* Always zero delta time inside blocks */
			if (needtime) {
				fwrite(&(bb = 0), 1, 1, f);
			}
			needtime = true;

			/* Check message type and write appropriate number of bytes */
			switch (*dp & 0xF0) {
			case MIDIST_NOTEOFF:
			case MIDIST_NOTEON:
			case MIDIST_POLYPRESS:
			case MIDIST_CONTROLLER:
			case MIDIST_PITCHBEND:
				fwrite(dp, 1, 3, f);
				dp += 3;
				continue;
			case MIDIST_PROGCHG:
			case MIDIST_CHANPRESS:
				fwrite(dp, 1, 2, f);
				dp += 2;
				continue;
			}

			/* Sysex needs to measure length and write that as well */
			if (*dp == MIDIST_SYSEX) {
				fwrite(dp, 1, 1, f);
				dp++;
				byte *sysexend = dp;
				while (*sysexend++ != MIDIST_ENDSYSEX);
				ptrdiff_t sysexlen = sysexend - dp;
				WriteVariableLen(f, sysexlen);
				fwrite(dp, 1, sysexend - dp, f);
				dp = sysexend;
				continue;
			}

			/* Fail for any other commands */
			fclose(f);
			return false;
		}
	}

	/* End of track marker */
	fwrite(&(bb = 0x00), 1, 1, f);
	fwrite(&(bb = MIDIST_SMF_META), 1, 1, f);
	fwrite(&(bb = 0x2F), 1, 1, f);
	fwrite(&(bb = 0x00), 1, 1, f);

	/* Fill out the RIFF block length */
	size_t trackendpos = ftell(f);
	fseek(f, tracksizepos, SEEK_SET);
	uint32 tracksize = trackendpos - tracksizepos - 4;
	tracksize = TO_BE32(tracksize);
	fwrite(&tracksize, 4, 1, f);

	fclose(f);
	return true;
}

/**
 * Get the name of a Standard MIDI File for a given song.
 * For songs already in SMF format, just returns the original.
 * Otherwise the song is converted and a temporary file written,
 * and the temporary file's name returned.
 * @note Caller is responsible for freeing the returned string.
 * @param song Song definition to query
 * @return Full filename string, NULL if failed
 */
char *MidiFile::GetSMFFile(const MusicSongInfo &song)
{
	if (song.filetype == MTT_STANDARDMIDI) {
		return stredup(song.filename);
	}

	if (song.filetype != MTT_MPSMIDI) return NULL;

	const char *lastpathsep = strrchr(song.filename, PATHSEPCHAR);
	if (lastpathsep == NULL) {
		lastpathsep = song.filename;
	}

	char basename[MAX_PATH];
	{
		/* Remove all '.' characters from filename */
		char *wp = basename;
		for (const char *rp = lastpathsep + 1; *rp != '\0'; rp++) {
			if (*rp != '.') *wp++ = *rp;
		}
		*wp++ = '\0';
	}

	char tempdirname[MAX_PATH];
	FioGetFullPath(tempdirname, lastof(tempdirname), Searchpath::SP_AUTODOWNLOAD_DIR, Subdirectory::BASESET_DIR, basename);
	if (AppendPathSeparator(tempdirname, lastof(tempdirname)) == false) return NULL;
	FioCreateDirectory(tempdirname);

	char output_filename[MAX_PATH];
	seprintf(output_filename, lastof(output_filename), "%s%d.mid", tempdirname, song.cat_index);

	if (FileExists(output_filename)) {
		/* If the file already exists, assume it's the correct decoded data */
		return stredup(output_filename);
	}

	byte *data;
	size_t datalen;
	data = GetMusicCatEntryData(song.filename, song.cat_index, datalen);
	if (data == NULL) return NULL;

	MidiFile midifile;
	if (!midifile.LoadMpsData(data, datalen)) {
		free(data);
		return NULL;
	}
	free(data);

	if (midifile.WriteSMF(output_filename)) {
		return stredup(output_filename);
	} else {
		return NULL;
	}
}


static bool CmdGlitchMusic(byte argc, char *argv[])
{
	if (argc < 2) {
		IConsolePrint(CC_WARNING, "Make music from the DOS version glitchy. Usage: 'glitchmusic <num1> [<num2>]'");
		IConsolePrint(CC_WARNING, "If the first number is 0, glitching is disabled. Otherwise both must be positive integers.");
		return true;
	}
	int arg1 = 0, arg2 = 1;
	arg1 = atoi(argv[1]);
	if (argc >= 3 && arg1 != 0) {
		arg2 = atoi(argv[2]);
	}

	if (arg1 < 0) {
		IConsolePrint(CC_ERROR, "First argument must be 0 or greater");
		return false;
	}
	if (arg2 < 1) {
		IConsolePrint(CC_ERROR, "Second argument must be 1 or greater");
		return false;
	}

	MpsMachine::glitch[0] = arg1;
	MpsMachine::glitch[1] = arg2;
	return true;
}

static bool CmdDumpSMF(byte argc, char *argv[])
{
	if (argc == 0) {
		IConsolePrint(CC_WARNING, "Write the current song to a Standard MIDI File. Usage: 'dumpsmf <filename>'");
		return true;
	}
	if (argc != 2) {
		IConsolePrint(CC_WARNING, "You must specify a filename to write MIDI data to.");
		return false;
	}

	if (_midifile_instance == NULL) {
		IConsolePrint(CC_ERROR, "There is no MIDI file loaded currently, make sure music is playing, and you're using a driver that works with raw MIDI.");
		return false;
	}

	char fnbuf[MAX_PATH] = { 0 };
	FioGetFullPath(fnbuf, lastof(fnbuf), Searchpath::SP_PERSONAL_DIR, Subdirectory::SCREENSHOT_DIR, argv[1]);
	IConsolePrintF(CC_INFO, "Dumping MIDI to: %s", fnbuf);

	if (_midifile_instance->WriteSMF(fnbuf)) {
		IConsolePrint(CC_INFO, "File written successfully.");
		return true;
	} else {
		IConsolePrint(CC_ERROR, "An error occurred writing MIDI file.");
		return false;
	}
}

static void RegisterConsoleMidiCommands()
{
	static bool registered = false;
	if (!registered) {
		IConsoleCmdRegister("dumpsmf", CmdDumpSMF);
		IConsoleCmdRegister("glitchmusic", CmdGlitchMusic);
		registered = true;
	}
}

MidiFile::MidiFile()
{
	RegisterConsoleMidiCommands();
}

MidiFile::~MidiFile()
{
	if (_midifile_instance == this)
		_midifile_instance = NULL;
}

