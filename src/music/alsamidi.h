/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file alsamidi.h Base of Alsa MIDI support. */

#ifndef MUSIC_ALSAMIDI_H
#define MUSIC_ALSAMIDI_H

#include "music_driver.hpp"
#include <alsa/asoundlib.h>
#include "midifile/MidiFile.h"
#include "../../thread.h"
#include <atomic>

/** The midi player for ALSA on Linux. */
class MusicDriver_AlsaMidi : public MusicDriver {
public:
	std::optional<std::string_view> Start(const StringList &param) override;

	void Stop() override;

	void PlaySong(const MusicSongInfo &song) override;

	void StopSong() override;

	bool IsSongPlaying() override;

	void SetVolume(uint8_t vol) override;
	std::string_view GetName() const override { return "alsamidi"; }

	void SendEvent(const smf::MidiEvent& event);
	void SendSysexEvent(const std::vector<uint8_t> data);
	void SendResetEvent();
	void UpdateTempo(const int tempo_uspq);
	bool Stopping();
	void WaitForFinish(const unsigned int last_tick);
	void StopQueue();

private:
	snd_seq_t* seq;
	int queue_id;
	int seq_port;
	int dev_port;
	std::thread _queue_thread;
	std::atomic<bool> playing{false};
	std::atomic<bool> stopping{false};

	void SetPPQ(const int ppq);
	void PushEvent(snd_seq_event_t seqev);
};

/** Factory for the Linux ALSA midi player. */
class FMusicDriver_AlsaMidi : public DriverFactoryBase {
public:
	FMusicDriver_AlsaMidi() : DriverFactoryBase(Driver::DT_MUSIC, 10, "alsamidi", "ALSA Linux MIDI Driver") {}
	Driver *CreateInstance() const override { return new MusicDriver_AlsaMidi(); }
};

static void StartQueue(MusicDriver_AlsaMidi *drv, const smf::MidiFile midifile);

#endif /* MUSIC_ALSAMIDI_H */
