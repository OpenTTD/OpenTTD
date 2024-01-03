/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file dmusic.cpp Playing music via DirectMusic. */

#define INITGUID
#include "../stdafx.h"
#ifdef WIN32_LEAN_AND_MEAN
#	undef WIN32_LEAN_AND_MEAN // Don't exclude rarely-used stuff from Windows headers
#endif
#include "../debug.h"
#include "../os/windows/win32.h"
#include "../core/mem_func.hpp"
#include "../thread.h"
#include "../fileio_func.h"
#include "../base_media_base.h"
#include "dmusic.h"
#include "midifile.hpp"
#include "midi.h"

#include <windows.h>
#include <dmksctrl.h>
#include <dmusicc.h>
#include <mutex>

#include "../safeguards.h"

#if defined(_MSC_VER)
#	pragma comment(lib, "ole32.lib")
#endif /* defined(_MSC_VER) */

static const int MS_TO_REFTIME = 1000 * 10; ///< DirectMusic time base is 100 ns.
static const int MIDITIME_TO_REFTIME = 10;  ///< Time base of the midi file reader is 1 us.


#define FOURCC_INFO  mmioFOURCC('I', 'N', 'F', 'O')
#define FOURCC_fmt   mmioFOURCC('f', 'm', 't', ' ')
#define FOURCC_data  mmioFOURCC('d', 'a', 't', 'a')

/** A DLS file. */
struct DLSFile {
	/** An instrument region maps a note range to wave data. */
	struct DLSRegion {
		RGNHEADER hdr;
		WAVELINK wave;
		WSMPL wave_sample;

		std::vector<WLOOP> wave_loops;
		std::vector<CONNECTION> articulators;
	};

	/** Instrument definition read from a DLS file. */
	struct DLSInstrument {
		INSTHEADER hdr;

		std::vector<CONNECTION> articulators;
		std::vector<DLSRegion> regions;
	};

	/** Wave data definition from a DLS file. */
	struct DLSWave {
		long file_offset;

		PCMWAVEFORMAT fmt;
		std::vector<BYTE> data;

		WSMPL wave_sample;
		std::vector<WLOOP> wave_loops;

		bool operator ==(long offset) const
		{
			return this->file_offset == offset;
		}
	};

	std::vector<DLSInstrument> instruments;
	std::vector<POOLCUE> pool_cues;
	std::vector<DLSWave> waves;

	/** Try loading a DLS file into memory. */
	bool LoadFile(const wchar_t *file);

private:
	/** Load an articulation structure from a DLS file. */
	bool ReadDLSArticulation(FILE *f, DWORD list_length, std::vector<CONNECTION> &out);
	/** Load a list of regions from a DLS file. */
	bool ReadDLSRegionList(FILE *f, DWORD list_length, DLSInstrument &instrument);
	/** Load a single region from a DLS file. */
	bool ReadDLSRegion(FILE *f, DWORD list_length, std::vector<DLSRegion> &out);
	/** Load a list of instruments from a DLS file. */
	bool ReadDLSInstrumentList(FILE *f, DWORD list_length);
	/** Load a single instrument from a DLS file. */
	bool ReadDLSInstrument(FILE *f, DWORD list_length);
	/** Load a list of waves from a DLS file. */
	bool ReadDLSWaveList(FILE *f, DWORD list_length);
	/** Load a single wave from a DLS file. */
	bool ReadDLSWave(FILE *f, DWORD list_length, long offset);
};

/** A RIFF chunk header. */
PACK_N(struct ChunkHeader {
	FOURCC type;  ///< Chunk type.
	DWORD length; ///< Length of the chunk, not including the chunk header itself.
}, 2);

/** Buffer format for a DLS wave download. */
PACK_N(struct WAVE_DOWNLOAD {
	DMUS_DOWNLOADINFO   dlInfo;
	ULONG               ulOffsetTable[2];
	DMUS_WAVE           dmWave;
	DMUS_WAVEDATA       dmWaveData;
}, 2);

struct PlaybackSegment {
	uint32_t start, end;
	size_t start_block;
	bool loop;
};

static struct {
	bool shutdown;    ///< flag to indicate playback thread shutdown
	bool playing;     ///< flag indicating that playback is active
	bool do_start;    ///< flag for starting playback of next_file at next opportunity
	bool do_stop;     ///< flag for stopping playback at next opportunity

	int preload_time; ///< preload time for music blocks.
	byte new_volume;  ///< volume setting to change to

	MidiFile next_file;           ///< upcoming file to play
	PlaybackSegment next_segment; ///< segment info for upcoming file
} _playback;

/** Handle to our worker thread. */
static std::thread _dmusic_thread;
/** Event to signal the thread that it should look at a state change. */
static HANDLE _thread_event = nullptr;
/** Lock access to playback data that is not thread-safe. */
static std::mutex _thread_mutex;

/** The direct music object manages buffers and ports. */
static IDirectMusic *_music = nullptr;
/** The port object lets us send MIDI data to the synthesizer. */
static IDirectMusicPort *_port = nullptr;
/** The buffer object collects the data to sent. */
static IDirectMusicBuffer *_buffer = nullptr;
/** List of downloaded DLS instruments. */
static std::vector<IDirectMusicDownload *> _dls_downloads;


static FMusicDriver_DMusic iFMusicDriver_DMusic;


bool DLSFile::ReadDLSArticulation(FILE *f, DWORD list_length, std::vector<CONNECTION> &out)
{
	while (list_length > 0) {
		ChunkHeader chunk;
		if (fread(&chunk, sizeof(chunk), 1, f) != 1) return false;
		list_length -= chunk.length + sizeof(chunk);

		if (chunk.type == FOURCC_ART1) {
			CONNECTIONLIST conns;
			if (fread(&conns, sizeof(conns), 1, f) != 1) return false;
			fseek(f, conns.cbSize - sizeof(conns), SEEK_CUR);

			/* Read all defined articulations. */
			for (ULONG i = 0; i < conns.cConnections; i++) {
				CONNECTION con;
				if (fread(&con, sizeof(con), 1, f) != 1) return false;
				out.push_back(con);
			}
		} else {
			fseek(f, chunk.length, SEEK_CUR);
		}
	}

	return true;
}

bool DLSFile::ReadDLSRegion(FILE *f, DWORD list_length, std::vector<DLSRegion> &out)
{
	out.push_back(DLSRegion());
	DLSRegion &region = out.back();

	/* Set default values. */
	region.wave_sample.cbSize = 0;

	while (list_length > 0) {
		ChunkHeader chunk;
		if (fread(&chunk, sizeof(chunk), 1, f) != 1) return false;
		list_length -= chunk.length + sizeof(chunk);

		if (chunk.type == FOURCC_LIST) {
			/* Unwrap list header. */
			if (fread(&chunk.type, sizeof(chunk.type), 1, f) != 1) return false;
			chunk.length -= sizeof(chunk.type);
		}

		switch (chunk.type) {
			case FOURCC_RGNH:
				if (fread(&region.hdr, sizeof(region.hdr), 1, f) != 1) return false;
				break;

			case FOURCC_WSMP:
				if (fread(&region.wave_sample, sizeof(region.wave_sample), 1, f) != 1) return false;
				fseek(f, region.wave_sample.cbSize - sizeof(region.wave_sample), SEEK_CUR);

				/* Read all defined sample loops. */
				for (ULONG i = 0; i < region.wave_sample.cSampleLoops; i++) {
					WLOOP loop;
					if (fread(&loop, sizeof(loop), 1, f) != 1) return false;
					region.wave_loops.push_back(loop);
				}
				break;

			case FOURCC_WLNK:
				if (fread(&region.wave, sizeof(region.wave), 1, f) != 1) return false;
				break;

			case FOURCC_LART: // List chunk
				if (!this->ReadDLSArticulation(f, chunk.length, region.articulators)) return false;
				break;

			case FOURCC_INFO:
				/* We don't care about info stuff. */
				fseek(f, chunk.length, SEEK_CUR);
				break;

			default:
				Debug(driver, 7, "DLS: Ignoring unknown chunk {}{}{}{}", (char)(chunk.type & 0xFF), (char)((chunk.type >> 8) & 0xFF), (char)((chunk.type >> 16) & 0xFF), (char)((chunk.type >> 24) & 0xFF));
				fseek(f, chunk.length, SEEK_CUR);
				break;
		}
	}

	return true;
}

bool DLSFile::ReadDLSRegionList(FILE *f, DWORD list_length, DLSInstrument &instrument)
{
	while (list_length > 0) {
		ChunkHeader chunk;
		if (fread(&chunk, sizeof(chunk), 1, f) != 1) return false;
		list_length -= chunk.length + sizeof(chunk);

		if (chunk.type == FOURCC_LIST) {
			FOURCC list_type;
			if (fread(&list_type, sizeof(list_type), 1, f) != 1) return false;

			if (list_type == FOURCC_RGN) {
				this->ReadDLSRegion(f, chunk.length - sizeof(list_type), instrument.regions);
			} else {
				Debug(driver, 7, "DLS: Ignoring unknown list chunk of type {}{}{}{}", (char)(list_type & 0xFF), (char)((list_type >> 8) & 0xFF), (char)((list_type >> 16) & 0xFF), (char)((list_type >> 24) & 0xFF));
				fseek(f, chunk.length - sizeof(list_type), SEEK_CUR);
			}
		} else {
			Debug(driver, 7, "DLS: Ignoring chunk {}{}{}{}", (char)(chunk.type & 0xFF), (char)((chunk.type >> 8) & 0xFF), (char)((chunk.type >> 16) & 0xFF), (char)((chunk.type >> 24) & 0xFF));
			fseek(f, chunk.length, SEEK_CUR);
		}
	}

	return true;
}

bool DLSFile::ReadDLSInstrument(FILE *f, DWORD list_length)
{
	this->instruments.push_back(DLSInstrument());
	DLSInstrument &instrument = this->instruments.back();

	while (list_length > 0) {
		ChunkHeader chunk;
		if (fread(&chunk, sizeof(chunk), 1, f) != 1) return false;
		list_length -= chunk.length + sizeof(chunk);

		if (chunk.type == FOURCC_LIST) {
			/* Unwrap list header. */
			if (fread(&chunk.type, sizeof(chunk.type), 1, f) != 1) return false;
			chunk.length -= sizeof(chunk.type);
		}

		switch (chunk.type) {
			case FOURCC_INSH:
				if (fread(&instrument.hdr, sizeof(instrument.hdr), 1, f) != 1) return false;
				break;

			case FOURCC_LART: // List chunk
				if (!this->ReadDLSArticulation(f, chunk.length, instrument.articulators)) return false;
				break;

			case FOURCC_LRGN: // List chunk
				if (!this->ReadDLSRegionList(f, chunk.length, instrument)) return false;
				break;

			case FOURCC_INFO:
				/* We don't care about info stuff. */
				fseek(f, chunk.length, SEEK_CUR);
				break;

			default:
				Debug(driver, 7, "DLS: Ignoring unknown chunk {}{}{}{}", (char)(chunk.type & 0xFF), (char)((chunk.type >> 8) & 0xFF), (char)((chunk.type >> 16) & 0xFF), (char)((chunk.type >> 24) & 0xFF));
				fseek(f, chunk.length, SEEK_CUR);
				break;
		}
	}

	return true;
}

bool DLSFile::ReadDLSInstrumentList(FILE *f, DWORD list_length)
{
	while (list_length > 0) {
		ChunkHeader chunk;
		if (fread(&chunk, sizeof(chunk), 1, f) != 1) return false;
		list_length -= chunk.length + sizeof(chunk);

		if (chunk.type == FOURCC_LIST) {
			FOURCC list_type;
			if (fread(&list_type, sizeof(list_type), 1, f) != 1) return false;

			if (list_type == FOURCC_INS) {
				Debug(driver, 6, "DLS: Reading instrument {}", (int)instruments.size());

				if (!this->ReadDLSInstrument(f, chunk.length - sizeof(list_type))) return false;
			} else {
				Debug(driver, 7, "DLS: Ignoring unknown list chunk of type {}{}{}{}", (char)(list_type & 0xFF), (char)((list_type >> 8) & 0xFF), (char)((list_type >> 16) & 0xFF), (char)((list_type >> 24) & 0xFF));
				fseek(f, chunk.length - sizeof(list_type), SEEK_CUR);
			}
		} else {
			Debug(driver, 7, "DLS: Ignoring chunk {}{}{}{}", (char)(chunk.type & 0xFF), (char)((chunk.type >> 8) & 0xFF), (char)((chunk.type >> 16) & 0xFF), (char)((chunk.type >> 24) & 0xFF));
			fseek(f, chunk.length, SEEK_CUR);
		}
	}

	return true;
}

bool DLSFile::ReadDLSWave(FILE *f, DWORD list_length, long offset)
{
	this->waves.push_back(DLSWave());
	DLSWave &wave = this->waves.back();

	/* Set default values. */
	MemSetT(&wave.wave_sample, 0);
	wave.wave_sample.cbSize = sizeof(WSMPL);
	wave.wave_sample.usUnityNote = 60;
	wave.file_offset = offset; // Store file offset so we can resolve the wave pool table later on.

	while (list_length > 0) {
		ChunkHeader chunk;
		if (fread(&chunk, sizeof(chunk), 1, f) != 1) return false;
		list_length -= chunk.length + sizeof(chunk);

		if (chunk.type == FOURCC_LIST) {
			/* Unwrap list header. */
			if (fread(&chunk.type, sizeof(chunk.type), 1, f) != 1) return false;
			chunk.length -= sizeof(chunk.type);
		}

		switch (chunk.type) {
			case FOURCC_fmt:
				if (fread(&wave.fmt, sizeof(wave.fmt), 1, f) != 1) return false;
				if (chunk.length > sizeof(wave.fmt)) fseek(f, chunk.length - sizeof(wave.fmt), SEEK_CUR);
				break;

			case FOURCC_WSMP:
				if (fread(&wave.wave_sample, sizeof(wave.wave_sample), 1, f) != 1) return false;
				fseek(f, wave.wave_sample.cbSize - sizeof(wave.wave_sample), SEEK_CUR);

				/* Read all defined sample loops. */
				for (ULONG i = 0; i < wave.wave_sample.cSampleLoops; i++) {
					WLOOP loop;
					if (fread(&loop, sizeof(loop), 1, f) != 1) return false;
					wave.wave_loops.push_back(loop);
				}
				break;

			case FOURCC_data:
				wave.data.resize(chunk.length);
				if (fread(&wave.data[0], sizeof(BYTE), wave.data.size(), f) != wave.data.size()) return false;
				break;

			case FOURCC_INFO:
				/* We don't care about info stuff. */
				fseek(f, chunk.length, SEEK_CUR);
				break;

			default:
				Debug(driver, 7, "DLS: Ignoring unknown chunk {}{}{}{}", (char)(chunk.type & 0xFF), (char)((chunk.type >> 8) & 0xFF), (char)((chunk.type >> 16) & 0xFF), (char)((chunk.type >> 24) & 0xFF));
				fseek(f, chunk.length, SEEK_CUR);
				break;
		}
	}

	return true;
}

bool DLSFile::ReadDLSWaveList(FILE *f, DWORD list_length)
{
	long base_offset = ftell(f);

	while (list_length > 0) {
		long chunk_offset = ftell(f);

		ChunkHeader chunk;
		if (fread(&chunk, sizeof(chunk), 1, f) != 1) return false;
		list_length -= chunk.length + sizeof(chunk);

		if (chunk.type == FOURCC_LIST) {
			FOURCC list_type;
			if (fread(&list_type, sizeof(list_type), 1, f) != 1) return false;

			if (list_type == FOURCC_wave) {
				Debug(driver, 6, "DLS: Reading wave {}", waves.size());

				if (!this->ReadDLSWave(f, chunk.length - sizeof(list_type), chunk_offset - base_offset)) return false;
			} else {
				Debug(driver, 7, "DLS: Ignoring unknown list chunk of type {}{}{}{}", (char)(list_type & 0xFF), (char)((list_type >> 8) & 0xFF), (char)((list_type >> 16) & 0xFF), (char)((list_type >> 24) & 0xFF));
				fseek(f, chunk.length - sizeof(list_type), SEEK_CUR);
			}
		} else {
			Debug(driver, 7, "DLS: Ignoring chunk {}{}{}{}", (char)(chunk.type & 0xFF), (char)((chunk.type >> 8) & 0xFF), (char)((chunk.type >> 16) & 0xFF), (char)((chunk.type >> 24) & 0xFF));
			fseek(f, chunk.length, SEEK_CUR);
		}
	}

	return true;
}

bool DLSFile::LoadFile(const wchar_t *file)
{
	Debug(driver, 2, "DMusic: Try to load DLS file {}", FS2OTTD(file));

	FILE *f = _wfopen(file, L"rb");
	if (f == nullptr) return false;

	FileCloser f_scope(f);

	/* Check DLS file header. */
	ChunkHeader hdr;
	FOURCC dls_type;
	if (fread(&hdr, sizeof(hdr), 1, f) != 1) return false;
	if (fread(&dls_type, sizeof(dls_type), 1, f) != 1) return false;
	if (hdr.type != FOURCC_RIFF || dls_type != FOURCC_DLS) return false;

	hdr.length -= sizeof(FOURCC);

	Debug(driver, 2, "DMusic: Parsing DLS file");

	DLSHEADER header;
	MemSetT(&header, 0);

	/* Iterate over all chunks in the file. */
	while (hdr.length > 0) {
		ChunkHeader chunk;
		if (fread(&chunk, sizeof(chunk), 1, f) != 1) return false;
		hdr.length -= chunk.length + sizeof(chunk);

		if (chunk.type == FOURCC_LIST) {
			/* Unwrap list header. */
			if (fread(&chunk.type, sizeof(chunk.type), 1, f) != 1) return false;
			chunk.length -= sizeof(chunk.type);
		}

		switch (chunk.type) {
			case FOURCC_COLH:
				if (fread(&header, sizeof(header), 1, f) != 1) return false;
				break;

			case FOURCC_LINS: // List chunk
				if (!this->ReadDLSInstrumentList(f, chunk.length)) return false;
				break;

			case FOURCC_WVPL: // List chunk
				if (!this->ReadDLSWaveList(f, chunk.length)) return false;
				break;

			case FOURCC_PTBL:
				POOLTABLE ptbl;
				if (fread(&ptbl, sizeof(ptbl), 1, f) != 1) return false;
				fseek(f, ptbl.cbSize - sizeof(ptbl), SEEK_CUR);

				/* Read all defined cues. */
				for (ULONG i = 0; i < ptbl.cCues; i++) {
					POOLCUE cue;
					if (fread(&cue, sizeof(cue), 1, f) != 1) return false;
					this->pool_cues.push_back(cue);
				}
				break;

			case FOURCC_INFO:
				/* We don't care about info stuff. */
				fseek(f, chunk.length, SEEK_CUR);
				break;

			default:
				Debug(driver, 7, "DLS: Ignoring unknown chunk {}{}{}{}", (char)(chunk.type & 0xFF), (char)((chunk.type >> 8) & 0xFF), (char)((chunk.type >> 16) & 0xFF), (char)((chunk.type >> 24) & 0xFF));
				fseek(f, chunk.length, SEEK_CUR);
				break;
		}
	}

	/* Have we read as many instruments as indicated? */
	if (header.cInstruments != this->instruments.size()) return false;

	/* Resolve wave pool table. */
	for (std::vector<POOLCUE>::iterator cue = this->pool_cues.begin(); cue != this->pool_cues.end(); cue++) {
		std::vector<DLSWave>::iterator w = std::find(this->waves.begin(), this->waves.end(), cue->ulOffset);
		if (w != this->waves.end()) {
			cue->ulOffset = (ULONG)(w - this->waves.begin());
		} else {
			cue->ulOffset = 0;
		}
	}

	return true;
}


static byte ScaleVolume(byte original, byte scale)
{
	return original * scale / 127;
}

static void TransmitChannelMsg(IDirectMusicBuffer *buffer, REFERENCE_TIME rt, byte status, byte p1, byte p2 = 0)
{
	if (buffer->PackStructured(rt, 0, status | (p1 << 8) | (p2 << 16)) == E_OUTOFMEMORY) {
		/* Buffer is full, clear it and try again. */
		_port->PlayBuffer(buffer);
		buffer->Flush();

		buffer->PackStructured(rt, 0, status | (p1 << 8) | (p2 << 16));
	}
}

static void TransmitSysex(IDirectMusicBuffer *buffer, REFERENCE_TIME rt, const byte *&msg_start, size_t &remaining)
{
	/* Find end of message. */
	const byte *msg_end = msg_start;
	while (*msg_end != MIDIST_ENDSYSEX) msg_end++;
	msg_end++; // Also include SysEx end byte.

	if (buffer->PackUnstructured(rt, 0, msg_end - msg_start, const_cast<LPBYTE>(msg_start)) == E_OUTOFMEMORY) {
		/* Buffer is full, clear it and try again. */
		_port->PlayBuffer(buffer);
		buffer->Flush();

		buffer->PackUnstructured(rt, 0, msg_end - msg_start, const_cast<LPBYTE>(msg_start));
	}

	/* Update position in buffer. */
	remaining -= msg_end - msg_start;
	msg_start = msg_end;
}

static void TransmitStandardSysex(IDirectMusicBuffer *buffer, REFERENCE_TIME rt, MidiSysexMessage msg)
{
	size_t length = 0;
	const byte *data = MidiGetStandardSysexMessage(msg, length);
	TransmitSysex(buffer, rt, data, length);
}

/** Transmit 'Note off' messages to all MIDI channels. */
static void TransmitNotesOff(IDirectMusicBuffer *buffer, REFERENCE_TIME block_time, REFERENCE_TIME cur_time)
{
	for (int ch = 0; ch < 16; ch++) {
		TransmitChannelMsg(buffer, block_time + 10, MIDIST_CONTROLLER | ch, MIDICT_MODE_ALLNOTESOFF, 0);
		TransmitChannelMsg(buffer, block_time + 10, MIDIST_CONTROLLER | ch, MIDICT_SUSTAINSW, 0);
		TransmitChannelMsg(buffer, block_time + 10, MIDIST_CONTROLLER | ch, MIDICT_MODE_RESETALLCTRL, 0);
	}

	/* Performing a GM reset stops all sound and resets all parameters. */
	TransmitStandardSysex(buffer, block_time + 20, MidiSysexMessage::ResetGM);
	TransmitStandardSysex(buffer, block_time + 30, MidiSysexMessage::RolandSetReverb);

	/* Explicitly flush buffer to make sure the messages are processed,
	 * as we want sound to stop immediately. */
	_port->PlayBuffer(buffer);
	buffer->Flush();

	/* Wait until message time has passed. */
	Sleep(Clamp((block_time - cur_time) / MS_TO_REFTIME, 5, 1000));
}

static void MidiThreadProc()
{
	Debug(driver, 2, "DMusic: Entering playback thread");

	REFERENCE_TIME last_volume_time = 0; // timestamp of the last volume change
	REFERENCE_TIME block_time = 0;       // timestamp of the last block sent to the port
	REFERENCE_TIME playback_start_time;  // timestamp current file began playback
	MidiFile current_file;               // file currently being played from
	PlaybackSegment current_segment;     // segment info for current playback
	size_t current_block = 0;            // next block index to send
	byte current_volume = 0;             // current effective volume setting
	byte channel_volumes[16];            // last seen volume controller values in raw data

	/* Get pointer to the reference clock of our output port. */
	IReferenceClock *clock;
	_port->GetLatencyClock(&clock);

	REFERENCE_TIME cur_time;
	clock->GetTime(&cur_time);

	_port->PlayBuffer(_buffer);
	_buffer->Flush();

	DWORD next_timeout = 1000;
	while (true) {
		/* Wait for a signal from the GUI thread or until the time for the next event has come. */
		DWORD wfso = WaitForSingleObject(_thread_event, next_timeout);

		if (_playback.shutdown) {
			_playback.playing = false;
			break;
		}

		if (_playback.do_stop) {
			Debug(driver, 2, "DMusic thread: Stopping playback");

			/* Turn all notes off and wait a bit to allow the messages to be handled. */
			clock->GetTime(&cur_time);
			TransmitNotesOff(_buffer, block_time, cur_time);

			_playback.playing = false;
			_playback.do_stop = false;
			block_time = 0;
			next_timeout = 1000;
			continue;
		}

		if (wfso == WAIT_OBJECT_0) {
			if (_playback.do_start) {
				Debug(driver, 2, "DMusic thread: Starting playback");
				{
					/* New scope to limit the time the mutex is locked. */
					std::lock_guard<std::mutex> lock(_thread_mutex);

					current_file.MoveFrom(_playback.next_file);
					std::swap(_playback.next_segment, current_segment);
					current_segment.start_block = 0;
					current_block = 0;
					_playback.playing = true;
					_playback.do_start = false;
				}

				/* Reset playback device between songs. */
				clock->GetTime(&cur_time);
				TransmitNotesOff(_buffer, block_time, cur_time);

				MemSetT<byte>(channel_volumes, 127, lengthof(channel_volumes));

				/* Take the current time plus the preload time as the music start time. */
				clock->GetTime(&playback_start_time);
				playback_start_time += _playback.preload_time * MS_TO_REFTIME;
			}
		}

		if (_playback.playing) {
			/* skip beginning of file? */
			if (current_segment.start > 0 && current_block == 0 && current_segment.start_block == 0) {
				/* find first block after start time and pretend playback started earlier
				 * this is to allow all blocks prior to the actual start to still affect playback,
				 * as they may contain important controller and program changes */
				size_t preload_bytes = 0;
				for (size_t bl = 0; bl < current_file.blocks.size(); bl++) {
					MidiFile::DataBlock &block = current_file.blocks[bl];
					preload_bytes += block.data.size();
					if (block.ticktime >= current_segment.start) {
						if (current_segment.loop) {
							Debug(driver, 2, "DMusic: timer: loop from block {} (ticktime {}, realtime {:.3f}, bytes {})", bl, block.ticktime, ((int)block.realtime) / 1000.0, preload_bytes);
							current_segment.start_block = bl;
							break;
						} else {
							/* Skip the transmission delay compensation performed in the Win32 MIDI driver.
							 * The DMusic driver will most likely be used with the MS softsynth, which is not subject to transmission delays.
							 */
							Debug(driver, 2, "DMusic: timer: start from block {} (ticktime {}, realtime {:.3f}, bytes {})", bl, block.ticktime, ((int)block.realtime) / 1000.0, preload_bytes);
							playback_start_time -= block.realtime * MIDITIME_TO_REFTIME;
							break;
						}
					}
				}
			}

			/* Get current playback timestamp. */
			REFERENCE_TIME current_time;
			clock->GetTime(&current_time);

			/* Check for volume change. */
			if (current_volume != _playback.new_volume) {
				if (current_time - last_volume_time > 10 * MS_TO_REFTIME) {
					Debug(driver, 2, "DMusic thread: volume change");
					current_volume = _playback.new_volume;
					last_volume_time = current_time;
					for (int ch = 0; ch < 16; ch++) {
						int vol = ScaleVolume(channel_volumes[ch], current_volume);
						TransmitChannelMsg(_buffer, block_time + 1, MIDIST_CONTROLLER | ch, MIDICT_CHANVOLUME, vol);
					}
					_port->PlayBuffer(_buffer);
					_buffer->Flush();
				}
			}

			while (current_block < current_file.blocks.size()) {
				MidiFile::DataBlock &block = current_file.blocks[current_block];

				/* check that block isn't at end-of-song override */
				if (current_segment.end > 0 && block.ticktime >= current_segment.end) {
					if (current_segment.loop) {
						Debug(driver, 2, "DMusic thread: Looping song");
						current_block = current_segment.start_block;
						playback_start_time = current_time - current_file.blocks[current_block].realtime * MIDITIME_TO_REFTIME;
					} else {
						_playback.do_stop = true;
					}
					next_timeout = 0;
					break;
				}
				/* check that block is not in the future */
				REFERENCE_TIME playback_time = current_time - playback_start_time;
				if (block.realtime * MIDITIME_TO_REFTIME > playback_time +  3 *_playback.preload_time * MS_TO_REFTIME) {
					/* Stop the thread loop until we are at the preload time of the next block. */
					next_timeout = Clamp(((int64_t)block.realtime * MIDITIME_TO_REFTIME - playback_time) / MS_TO_REFTIME - _playback.preload_time, 0, 1000);
					Debug(driver, 9, "DMusic thread: Next event in {} ms (music {}, ref {})", next_timeout, block.realtime * MIDITIME_TO_REFTIME, playback_time);
					break;
				}

				/* Timestamp of the current block. */
				block_time = playback_start_time + block.realtime * MIDITIME_TO_REFTIME;
				Debug(driver, 9, "DMusic thread: Streaming block {} (cur={}, block={})", current_block, (long long)(current_time / MS_TO_REFTIME), (long long)(block_time / MS_TO_REFTIME));

				const byte *data = block.data.data();
				size_t remaining = block.data.size();
				byte last_status = 0;
				while (remaining > 0) {
					/* MidiFile ought to have converted everything out of running status,
					 * but handle it anyway just to be safe */
					byte status = data[0];
					if (status & 0x80) {
						last_status = status;
						data++;
						remaining--;
					} else {
						status = last_status;
					}
					switch (status & 0xF0) {
						case MIDIST_PROGCHG:
						case MIDIST_CHANPRESS:
							/* 2 byte channel messages */
							TransmitChannelMsg(_buffer, block_time, status, data[0]);
							data++;
							remaining--;
							break;
						case MIDIST_NOTEOFF:
						case MIDIST_NOTEON:
						case MIDIST_POLYPRESS:
						case MIDIST_PITCHBEND:
							/* 3 byte channel messages */
							TransmitChannelMsg(_buffer, block_time, status, data[0], data[1]);
							data += 2;
							remaining -= 2;
							break;
						case MIDIST_CONTROLLER:
							/* controller change */
							if (data[0] == MIDICT_CHANVOLUME) {
								/* volume controller, adjust for user volume */
								channel_volumes[status & 0x0F] = data[1];
								int vol = ScaleVolume(data[1], current_volume);
								TransmitChannelMsg(_buffer, block_time, status, data[0], vol);
							} else {
								/* handle other controllers normally */
								TransmitChannelMsg(_buffer, block_time, status, data[0], data[1]);
							}
							data += 2;
							remaining -= 2;
							break;
						case 0xF0:
							/* system messages */
							switch (status) {
								case MIDIST_SYSEX: /* system exclusive */
									TransmitSysex(_buffer, block_time, data, remaining);
									break;
								case MIDIST_TC_QFRAME: /* time code quarter frame */
								case MIDIST_SONGSEL: /* song select */
									data++;
									remaining--;
									break;
								case MIDIST_SONGPOSPTR: /* song position pointer */
									data += 2;
									remaining -= 2;
									break;
								default: /* remaining have no data bytes */
									break;
							}
							break;
					}
				}

				current_block++;
			}

			/* Anything in the playback buffer? Send it down the port. */
			DWORD used_buffer = 0;
			_buffer->GetUsedBytes(&used_buffer);
			if (used_buffer > 0) {
				_port->PlayBuffer(_buffer);
				_buffer->Flush();
			}

			/* end? */
			if (current_block == current_file.blocks.size()) {
				if (current_segment.loop) {
					current_block = current_segment.start_block;
					playback_start_time = block_time - current_file.blocks[current_block].realtime * MIDITIME_TO_REFTIME;
				} else {
					_playback.do_stop = true;
				}
				next_timeout = 0;
			}
		}
	}

	Debug(driver, 2, "DMusic: Exiting playback thread");

	/* Turn all notes off and wait a bit to allow the messages to be handled by real hardware. */
	clock->GetTime(&cur_time);
	TransmitNotesOff(_buffer, block_time, cur_time);
	Sleep(_playback.preload_time * 4);

	clock->Release();
}

static void * DownloadArticulationData(int base_offset, void *data, const std::vector<CONNECTION> &artic)
{
	DMUS_ARTICULATION2 *art = (DMUS_ARTICULATION2 *)data;
	art->ulArtIdx = base_offset + 1;
	art->ulFirstExtCkIdx = 0;
	art->ulNextArtIdx = 0;

	CONNECTIONLIST *con_list = (CONNECTIONLIST *)(art + 1);
	con_list->cbSize = sizeof(CONNECTIONLIST);
	con_list->cConnections = (ULONG)artic.size();
	MemCpyT((CONNECTION *)(con_list + 1), &artic.front(), artic.size());

	return (CONNECTION *)(con_list + 1) + artic.size();
}

static const char *LoadDefaultDLSFile(const char *user_dls)
{
	DMUS_PORTCAPS caps;
	MemSetT(&caps, 0);
	caps.dwSize = sizeof(DMUS_PORTCAPS);
	_port->GetCaps(&caps);

	/* Nothing to unless it is a synth with instrument download that doesn't come with GM voices by default. */
	if ((caps.dwFlags & (DMUS_PC_DLS | DMUS_PC_DLS2)) != 0 && (caps.dwFlags & DMUS_PC_GMINHARDWARE) == 0) {
		DLSFile dls_file;

		if (user_dls == nullptr) {
			/* Try loading the default GM DLS file stored in the registry. */
			HKEY hkDM;
			if (SUCCEEDED(RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\DirectMusic", 0, KEY_READ, &hkDM))) {
				wchar_t dls_path[MAX_PATH];
				DWORD buf_size = sizeof(dls_path); // Buffer size as to be given in bytes!
				if (SUCCEEDED(RegQueryValueEx(hkDM, L"GMFilePath", nullptr, nullptr, (LPBYTE)dls_path, &buf_size))) {
					wchar_t expand_path[MAX_PATH * 2];
					ExpandEnvironmentStrings(dls_path, expand_path, lengthof(expand_path));
					if (!dls_file.LoadFile(expand_path)) Debug(driver, 1, "Failed to load default GM DLS file from registry");
				}
				RegCloseKey(hkDM);
			}

			/* If we couldn't load the file from the registry, try again at the default install path of the GM DLS file. */
			if (dls_file.instruments.empty()) {
				static const wchar_t *DLS_GM_FILE = L"%windir%\\System32\\drivers\\gm.dls";
				wchar_t path[MAX_PATH];
				ExpandEnvironmentStrings(DLS_GM_FILE, path, lengthof(path));

				if (!dls_file.LoadFile(path)) return "Can't load GM DLS collection";
			}
		} else {
			if (!dls_file.LoadFile(OTTD2FS(user_dls).c_str())) return "Can't load GM DLS collection";
		}

		/* Get download port and allocate download IDs. */
		IDirectMusicPortDownload *download_port = nullptr;
		if (FAILED(_port->QueryInterface(IID_IDirectMusicPortDownload, (LPVOID *)&download_port))) return "Can't get download port";

		DWORD dlid_wave = 0, dlid_inst = 0;
		if (FAILED(download_port->GetDLId(&dlid_wave, (DWORD)dls_file.waves.size())) || FAILED(download_port->GetDLId(&dlid_inst, (DWORD)dls_file.instruments.size()))) {
			download_port->Release();
			return "Can't get enough DLS ids";
		}

		DWORD dwAppend = 0;
		download_port->GetAppend(&dwAppend);

		/* Download wave data. */
		for (DWORD i = 0; i < dls_file.waves.size(); i++) {
			IDirectMusicDownload *dl_wave = nullptr;
			if (FAILED(download_port->AllocateBuffer((DWORD)(sizeof(WAVE_DOWNLOAD) + dwAppend * dls_file.waves[i].fmt.wf.nBlockAlign + dls_file.waves[i].data.size()), &dl_wave))) {
				download_port->Release();
				return "Can't allocate wave download buffer";
			}

			WAVE_DOWNLOAD *wave;
			DWORD wave_size = 0;
			if (FAILED(dl_wave->GetBuffer((LPVOID *)&wave, &wave_size))) {
				dl_wave->Release();
				download_port->Release();
				return "Can't get wave download buffer";
			}

			/* Fill download data. */
			MemSetT(wave, 0);
			wave->dlInfo.dwDLType = DMUS_DOWNLOADINFO_WAVE;
			wave->dlInfo.cbSize = wave_size;
			wave->dlInfo.dwDLId = dlid_wave + i;
			wave->dlInfo.dwNumOffsetTableEntries = 2;
			wave->ulOffsetTable[0] = offsetof(WAVE_DOWNLOAD, dmWave);
			wave->ulOffsetTable[1] = offsetof(WAVE_DOWNLOAD, dmWaveData);
			wave->dmWave.ulWaveDataIdx = 1;
			MemCpyT((PCMWAVEFORMAT *)&wave->dmWave.WaveformatEx, &dls_file.waves[i].fmt, 1);
			wave->dmWaveData.cbSize = (DWORD)dls_file.waves[i].data.size();
			MemCpyT(wave->dmWaveData.byData, &dls_file.waves[i].data[0], dls_file.waves[i].data.size());

			_dls_downloads.push_back(dl_wave);
			if (FAILED(download_port->Download(dl_wave))) {
				download_port->Release();
				return "Downloading DLS wave failed";
			}
		}

		/* Download instrument data. */
		for (DWORD i = 0; i < dls_file.instruments.size(); i++) {
			DWORD offsets = 1 + (DWORD)dls_file.instruments[i].regions.size();

			/* Calculate download size for the instrument. */
			size_t i_size = sizeof(DMUS_DOWNLOADINFO) + sizeof(DMUS_INSTRUMENT);
			if (!dls_file.instruments[i].articulators.empty()) {
				/* Articulations are stored as two chunks, one containing meta data and one with the actual articulation data. */
				offsets += 2;
				i_size += sizeof(DMUS_ARTICULATION2) + sizeof(CONNECTIONLIST) + sizeof(CONNECTION) * dls_file.instruments[i].articulators.size();
			}

			for (std::vector<DLSFile::DLSRegion>::iterator rgn = dls_file.instruments[i].regions.begin(); rgn != dls_file.instruments[i].regions.end(); rgn++) {
				if (!rgn->articulators.empty()) {
					offsets += 2;
					i_size += sizeof(DMUS_ARTICULATION2) + sizeof(CONNECTIONLIST) + sizeof(CONNECTION) * rgn->articulators.size();
				}

				/* Region size depends on the number of wave loops. The size of the
				 * declared structure already accounts for one loop. */
				if (rgn->wave_sample.cbSize != 0) {
					i_size += sizeof(DMUS_REGION) - sizeof(DMUS_REGION::WLOOP) + sizeof(WLOOP) * rgn->wave_loops.size();
				} else {
					i_size += sizeof(DMUS_REGION) - sizeof(DMUS_REGION::WLOOP) + sizeof(WLOOP) * dls_file.waves[dls_file.pool_cues[rgn->wave.ulTableIndex].ulOffset].wave_loops.size();
				}
			}

			i_size += offsets * sizeof(ULONG);

			/* Allocate download buffer. */
			IDirectMusicDownload *dl_inst = nullptr;
			if (FAILED(download_port->AllocateBuffer((DWORD)i_size, &dl_inst))) {
				download_port->Release();
				return "Can't allocate instrument download buffer";
			}

			void *instrument;
			DWORD inst_size = 0;
			if (FAILED(dl_inst->GetBuffer((LPVOID *)&instrument, &inst_size))) {
				dl_inst->Release();
				download_port->Release();
				return "Can't get instrument download buffer";
			}
			char *inst_base = (char *)instrument;

			/* Fill download header. */
			DMUS_DOWNLOADINFO *d_info = (DMUS_DOWNLOADINFO *)instrument;
			d_info->dwDLType = DMUS_DOWNLOADINFO_INSTRUMENT2;
			d_info->cbSize = inst_size;
			d_info->dwDLId = dlid_inst + i;
			d_info->dwNumOffsetTableEntries = offsets;
			instrument = d_info + 1;

			/* Download offset table; contains the offsets of all chunks relative to the buffer start. */
			ULONG *offset_table = (ULONG *)instrument;
			instrument = offset_table + offsets;
			int last_offset = 0;

			/* Instrument header. */
			DMUS_INSTRUMENT *inst_data = (DMUS_INSTRUMENT *)instrument;
			MemSetT(inst_data, 0);
			offset_table[last_offset++] = (char *)inst_data - inst_base;
			inst_data->ulPatch = (dls_file.instruments[i].hdr.Locale.ulBank & F_INSTRUMENT_DRUMS) | ((dls_file.instruments[i].hdr.Locale.ulBank & 0x7F7F) << 8) | (dls_file.instruments[i].hdr.Locale.ulInstrument & 0x7F);
			instrument = inst_data + 1;

			/* Write global articulations. */
			if (!dls_file.instruments[i].articulators.empty()) {
				inst_data->ulGlobalArtIdx = last_offset;
				offset_table[last_offset++] = (char *)instrument - inst_base;
				offset_table[last_offset++] = (char *)instrument + sizeof(DMUS_ARTICULATION2) - inst_base;

				instrument = DownloadArticulationData(inst_data->ulGlobalArtIdx, instrument, dls_file.instruments[i].articulators);
				assert((char *)instrument - inst_base <= (ptrdiff_t)inst_size);
			}

			/* Write out regions. */
			inst_data->ulFirstRegionIdx = last_offset;
			for (uint j = 0; j < dls_file.instruments[i].regions.size(); j++) {
				DLSFile::DLSRegion &rgn = dls_file.instruments[i].regions[j];

				DMUS_REGION *inst_region = (DMUS_REGION *)instrument;
				offset_table[last_offset++] = (char *)inst_region - inst_base;
				inst_region->RangeKey = rgn.hdr.RangeKey;
				inst_region->RangeVelocity = rgn.hdr.RangeVelocity;
				inst_region->fusOptions = rgn.hdr.fusOptions;
				inst_region->usKeyGroup = rgn.hdr.usKeyGroup;
				inst_region->ulFirstExtCkIdx = 0;

				ULONG wave_id = dls_file.pool_cues[rgn.wave.ulTableIndex].ulOffset;
				inst_region->WaveLink = rgn.wave;
				inst_region->WaveLink.ulTableIndex = wave_id + dlid_wave;

				/* The wave sample data will be taken from the region, if defined, otherwise from the wave itself. */
				if (rgn.wave_sample.cbSize != 0) {
					inst_region->WSMP = rgn.wave_sample;
					if (!rgn.wave_loops.empty()) MemCpyT(inst_region->WLOOP, &rgn.wave_loops.front(), rgn.wave_loops.size());

					instrument = (char *)(inst_region + 1) - sizeof(DMUS_REGION::WLOOP) + sizeof(WLOOP) * rgn.wave_loops.size();
				} else {
					inst_region->WSMP = rgn.wave_sample;
					if (!dls_file.waves[wave_id].wave_loops.empty()) MemCpyT(inst_region->WLOOP, &dls_file.waves[wave_id].wave_loops.front(), dls_file.waves[wave_id].wave_loops.size());

					instrument = (char *)(inst_region + 1) - sizeof(DMUS_REGION::WLOOP) + sizeof(WLOOP) * dls_file.waves[wave_id].wave_loops.size();
				}

				/* Write local articulator data. */
				if (!rgn.articulators.empty()) {
					inst_region->ulRegionArtIdx = last_offset;
					offset_table[last_offset++] = (char *)instrument - inst_base;
					offset_table[last_offset++] = (char *)instrument + sizeof(DMUS_ARTICULATION2) - inst_base;

					instrument = DownloadArticulationData(inst_region->ulRegionArtIdx, instrument, rgn.articulators);
				} else {
					inst_region->ulRegionArtIdx = 0;
				}
				assert((char *)instrument - inst_base <= (ptrdiff_t)inst_size);

				/* Link to the next region unless this was the last one.*/
				inst_region->ulNextRegionIdx = j < dls_file.instruments[i].regions.size() - 1 ? last_offset : 0;
			}

			_dls_downloads.push_back(dl_inst);
			if (FAILED(download_port->Download(dl_inst))) {
				download_port->Release();
				return "Downloading DLS instrument failed";
			}
		}

		download_port->Release();
	}

	return nullptr;
}


const char *MusicDriver_DMusic::Start(const StringList &parm)
{
	/* Initialize COM */
	if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) return "COM initialization failed";

	/* Create the DirectMusic object */
	if (FAILED(CoCreateInstance(
				CLSID_DirectMusic,
				nullptr,
				CLSCTX_INPROC,
				IID_IDirectMusic,
				(LPVOID*)&_music
			))) {
		return "Failed to create the music object";
	}

	/* Assign sound output device. */
	if (FAILED(_music->SetDirectSound(nullptr, nullptr))) return "Can't set DirectSound interface";

	/* MIDI events need to be send to the synth in time before their playback time
	 * has come. By default, we try send any events at least 50 ms before playback. */
	_playback.preload_time = GetDriverParamInt(parm, "preload", 50);

	int pIdx = GetDriverParamInt(parm, "port", -1);
	if (_debug_driver_level > 0) {
		/* Print all valid output ports. */
		char desc[DMUS_MAX_DESCRIPTION];

		DMUS_PORTCAPS caps;
		MemSetT(&caps, 0);
		caps.dwSize = sizeof(DMUS_PORTCAPS);

		Debug(driver, 1, "Detected DirectMusic ports:");
		for (int i = 0; _music->EnumPort(i, &caps) == S_OK; i++) {
			if (caps.dwClass == DMUS_PC_OUTPUTCLASS) {
				Debug(driver, 1, " {}: {}{}", i, convert_from_fs(caps.wszDescription, desc, lengthof(desc)), i == pIdx ? " (selected)" : "");
			}
		}
	}

	GUID guidPort;
	if (pIdx >= 0) {
		/* Check if the passed port is a valid port. */
		DMUS_PORTCAPS caps;
		MemSetT(&caps, 0);
		caps.dwSize = sizeof(DMUS_PORTCAPS);
		if (FAILED(_music->EnumPort(pIdx, &caps))) return "Supplied port parameter is not a valid port";
		if (caps.dwClass != DMUS_PC_OUTPUTCLASS) return "Supplied port parameter is not an output port";
		guidPort = caps.guidPort;
	} else {
		if (FAILED(_music->GetDefaultPort(&guidPort))) return "Can't query default music port";
	}

	/* Create new port. */
	DMUS_PORTPARAMS params;
	MemSetT(&params, 0);
	params.dwSize = sizeof(DMUS_PORTPARAMS);
	params.dwValidParams = DMUS_PORTPARAMS_CHANNELGROUPS;
	params.dwChannelGroups = 1;
	if (FAILED(_music->CreatePort(guidPort, &params, &_port, nullptr))) return "Failed to create port";
	/* Activate port. */
	if (FAILED(_port->Activate(TRUE))) return "Failed to activate port";

	/* Create playback buffer. */
	DMUS_BUFFERDESC desc;
	MemSetT(&desc, 0);
	desc.dwSize = sizeof(DMUS_BUFFERDESC);
	desc.guidBufferFormat = KSDATAFORMAT_SUBTYPE_DIRECTMUSIC;
	desc.cbBuffer = 1024;
	if (FAILED(_music->CreateMusicBuffer(&desc, &_buffer, nullptr))) return "Failed to create music buffer";

	/* On soft-synths (e.g. the default DirectMusic one), we might need to load a wavetable set to get music. */
	const char *dls = LoadDefaultDLSFile(GetDriverParam(parm, "dls"));
	if (dls != nullptr) return dls;

	/* Create playback thread and synchronization primitives. */
	_thread_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (_thread_event == nullptr) return "Can't create thread shutdown event";

	if (!StartNewThread(&_dmusic_thread, "ottd:dmusic", &MidiThreadProc)) return "Can't create MIDI output thread";

	return nullptr;
}


MusicDriver_DMusic::~MusicDriver_DMusic()
{
	this->Stop();
}


void MusicDriver_DMusic::Stop()
{
	if (_dmusic_thread.joinable()) {
		_playback.shutdown = true;
		SetEvent(_thread_event);
		_dmusic_thread.join();
	}

	/* Unloaded any instruments we loaded. */
	if (!_dls_downloads.empty()) {
		IDirectMusicPortDownload *download_port = nullptr;
		_port->QueryInterface(IID_IDirectMusicPortDownload, (LPVOID *)&download_port);

		/* Instruments refer to waves. As the waves are at the beginning of the download list,
		 * do the unload from the back so that references are cleared properly. */
		for (std::vector<IDirectMusicDownload *>::reverse_iterator i = _dls_downloads.rbegin(); download_port != nullptr && i != _dls_downloads.rend(); i++) {
			download_port->Unload(*i);
			(*i)->Release();
		}
		_dls_downloads.clear();

		if (download_port != nullptr) download_port->Release();
	}

	if (_buffer != nullptr) {
		_buffer->Release();
		_buffer = nullptr;
	}

	if (_port != nullptr) {
		_port->Activate(FALSE);
		_port->Release();
		_port = nullptr;
	}

	if (_music != nullptr) {
		_music->Release();
		_music = nullptr;
	}

	CloseHandle(_thread_event);

	CoUninitialize();
}


void MusicDriver_DMusic::PlaySong(const MusicSongInfo &song)
{
	std::lock_guard<std::mutex> lock(_thread_mutex);

	if (!_playback.next_file.LoadSong(song)) return;

	_playback.next_segment.start = song.override_start;
	_playback.next_segment.end = song.override_end;
	_playback.next_segment.loop = song.loop;

	_playback.do_start = true;
	SetEvent(_thread_event);
}


void MusicDriver_DMusic::StopSong()
{
	_playback.do_stop = true;
	SetEvent(_thread_event);
}


bool MusicDriver_DMusic::IsSongPlaying()
{
	return _playback.playing || _playback.do_start;
}


void MusicDriver_DMusic::SetVolume(byte vol)
{
	_playback.new_volume = vol;
}
