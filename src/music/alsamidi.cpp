/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file alsamidi.cpp Support for ALSA Linux midi. */

#include "../stdafx.h"
#include "../openttd.h"
#include "alsamidi.h"
#include "../base_media_base.h"
#include "midifile.hpp"
#include <alsa/asoundlib.h>
#include "../debug.h"
#include "midi.h"
#include <chrono>
#include <thread>

#include "../safeguards.h"

/** Factory for ALSA midi player. */
static FMusicDriver_AlsaMidi iFMusicDriver_AlsaMidi;

std::optional<std::string_view> MusicDriver_AlsaMidi::Start(const StringList &parm)
{
	this->playing.store(false);
	Debug(driver, 2, "ALSA MIDI: Start");
	this->dev_port = (uint)GetDriverParamInt(parm, "port", UINT_MAX);
	Debug(driver, 2, "ALSA MIDI: using MIDI device at port {}", dev_port);

	// Open sequencer
	if (snd_seq_open(&this->seq, "default", SND_SEQ_OPEN_OUTPUT, 0) < 0) {
		return "Failed to open ALSA sequencer";
	}

	snd_seq_set_client_name(this->seq, "OpenTTD MIDI Out");

	// Create port
	this->seq_port = snd_seq_create_simple_port(this->seq, "MIDI Out",
		SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ,
		SND_SEQ_PORT_TYPE_MIDI_GENERIC|SND_SEQ_PORT_TYPE_APPLICATION);

	if (this->seq_port < 0) {
		return "Failed to create ALSA sequencer port";
	}

	// Create event queue
	Debug(driver, 2, "ALSA MIDI: Creating sequencer event queue");
	this->queue_id = snd_seq_alloc_named_queue(seq, "OpenTTD Sequencer Queue");
	if (this->queue_id < 0) {
		return "Failed to create ALSA sequencer event queue";
	}

	// Set a slightly larger event output buffer than normal
	snd_seq_set_client_pool_output(this->seq, 1000);
	snd_seq_set_client_pool_output_room(this->seq, 1000);

	// Turn on nonblocking more
	snd_seq_nonblock(this->seq, 1);

	snd_seq_addr_t sender, dest;

	sender.client = snd_seq_client_id(this->seq);
	sender.port = this->seq_port;

	dest.client = this->dev_port;
	dest.port = this->seq_port;

	snd_seq_port_subscribe_t* subs;
	snd_seq_port_subscribe_alloca(&subs);
	snd_seq_port_subscribe_set_sender(subs, &sender);
	snd_seq_port_subscribe_set_dest(subs, &dest);

	if (snd_seq_subscribe_port(this->seq, subs) < 0) {
		return "Failed to connect to port";
	}

	Debug(driver, 2, "ALSA MIDI: opened sequencer port {}", this->seq_port);

	return std::nullopt;
}

void MusicDriver_AlsaMidi::Stop()
{
	Debug(driver, 2, "ALSA MIDI: stopping");
	this->StopSong();

	if (this->queue_id) {
		Debug(driver, 2, "ALSA MIDI: freeing sequencer event queue");
		snd_seq_free_queue(this->seq, this->queue_id);
	}

	if (this->seq) {
		Debug(driver, 2, "ALSA MIDI: closing sequencer handle");
		snd_seq_close(this->seq);
	}
}

void MusicDriver_AlsaMidi::PlaySong(const MusicSongInfo &song)
{
	Debug(driver, 2, "ALSA MIDI: PlaySong");
	std::string filename = MidiFile::GetSMFFile(song);
	smf::MidiFile midifile;

	Debug(driver, 2, "ALSA MIDI: reading SMFFile");
	if (!filename.empty()) {
		if (!midifile.read(filename)) {
			Debug(driver, 2, "ALSA MIDI: error reading SMFFile");
		}
	}

	// Sort (should be sorted already, and WriteSMF only creates single-track MIDIs anyway)
	midifile.sortTracks();
	// Convert MIDI ticks to absolute seconds
	midifile.doTimeAnalysis();

	//Merge > 1 tracks into single track for easier queueing.
	midifile.joinTracks();

	if (this->playing.load() == true) {
		this->StopSong();
	}

	Debug(driver, 2, "ALSA MIDI: starting playback of {}", song.songname);
	Debug(driver, 2, "ALSA MIDI: SMF filename {}", filename);

	// ALSA does not allow setting PPQ on started queues, so do this first
	// Tempo may be adjusted later, on a started queue.
	int ppq = midifile.getTPQ();
	this->SetPPQ(ppq);

	snd_seq_start_queue(this->seq, this->queue_id, nullptr);
	snd_seq_drain_output(this->seq);

	this->playing.store(true);

	StartNewThread(&this->_queue_thread, "ottd:alsamidi", &StartQueue, this, std::move(midifile));
}

/**
 * Starts the ALSA sequencer queue, iterates through the MIDI events in the file,
 * converts them to ALSA sequencer events, and pushes them onto the queue.
 *
 * This function is blocking and expects to be run in a thread. It will block
 * until either it is signaled to stop (in which case it will purge the ALSA queue,
 * send a GM RESET, and terminate), or it has enqueued all events in the MIDI file,
 * and waited for the queue to finish processing them all.
 *
 *
 * @param drv pointer to `this` instance of the class
 * @param midifile the previously-loaded, sorted, and time-corrected STD MIDI file.
 *
 * @see Stopping()
 * @see StopSong()
 * @see WaitForFinish()
 */
static void StartQueue(MusicDriver_AlsaMidi *drv, const smf::MidiFile midifile)
{
	Debug(driver, 2, "ALSA MIDI: queue thread started");

	unsigned int last_tick;
	// Push all events for all tracks to the sequencer queue
	for (int track = 0; track < midifile.getNumTracks(); track++) {

		std::vector<uint8_t> sysex_buffer;

		for (int event = 0; event < midifile[track].size(); event++) {
			auto& ev = midifile[track][event];

			last_tick = static_cast<unsigned int>(ev.tick);

			if (drv->Stopping())  {
				Debug(driver, 2, "ALSA MIDI: Looks like we are stopping, bailing out of queue thread");
				drv->SendResetEvent();
				drv->StopQueue();
				sysex_buffer.clear();
				return;
			}
			if (ev.isTempo()) {
				// Handle tempo change here,  as we have to change it for the whole queue
				Debug(driver, 2, "ALSA MIDI: Got tempo change event in queue thread");
				int tempo_uspq = ev.getTempoMicroseconds();
				drv->UpdateTempo(tempo_uspq);
				continue;
			}
			// Handle SYSEX events
			// SYSEX events may
			// 1. Be a complete SYSEX event (begin with F0 and end with F7)
			// 2. Be a "middle" SYSEX event (a previous message began with F0)
			// 3. Be an "end" SYSEX event (a previous message began with F0, and this one ends with F7)
			// This basically means you need an accumulator. Split SYSEX messages are *rare* but exist.
			if (ev.getCommandByte() == MIDIST_SYSEX) {
				Debug(driver, 2, "ALSA MIDI: got SYSEX message");
				sysex_buffer.clear();

				// If this is is a complete (not partial) SYSEX message, send it
				// Otherwise, accumulate it as a partial and continue to the next
				if (ev.back() == MIDIST_ENDSYSEX) {
					Debug(driver, 2, "ALSA MIDI: complete SYSEX, sending");
					sysex_buffer.insert(sysex_buffer.end(), ev.begin() + 1, ev.end() -1);
					drv->SendSysexEvent(sysex_buffer);
					sysex_buffer.clear();
				} else {
					sysex_buffer.insert(sysex_buffer.end(), ev.begin() + 1, ev.end());
				}
				continue;
			}
			if (!sysex_buffer.empty() && ev.back() == MIDIST_ENDSYSEX) {
				Debug(driver, 2, "ALSA MIDI: partial SYSEX completed, sending");
				sysex_buffer.insert(sysex_buffer.end(), ev.begin(), ev.end() -1);
				drv->SendSysexEvent(sysex_buffer);
				sysex_buffer.clear();
				continue;
			}
			if (!sysex_buffer.empty()) {
				Debug(driver, 2, "ALSA MIDI: partial SYSEX continuing");
				sysex_buffer.insert(sysex_buffer.end(), ev.begin(), ev.end());
				continue;
			}

			// At this point, it's just a regular event - handle it.
			drv->SendEvent(ev);
		}
	}

	Debug(driver, 2, "ALSA MIDI: queue thread finished, waiting for events");
	drv->WaitForFinish(last_tick);
	drv->StopQueue();
}

/**
 * Stops the ALSA sequencer queue, and sets `playing` to false.
 *
 * Note that this does not clear or drop any pending events in the queue
 * before stopping it.
 *
 * @see SendResetEvent()
 */
void MusicDriver_AlsaMidi::StopQueue()
{
	Debug(driver, 2, "ALSA MIDI: stopping current queue!");
	snd_seq_stop_queue(this->seq, this->queue_id, nullptr);
	snd_seq_drain_output(this->seq);
	this->playing.store(false);
}

/**
 * Sends a SYSEX GM reset message, after dropping all pending events  in the
 * queue.
 *
 * Does not stop the queue.
 *
 * @see StopQueue()
 */
void MusicDriver_AlsaMidi::SendResetEvent()
{
	// Drop anything still in the queue, this is a disruptive reset.
	snd_seq_drop_output(this->seq);

	std::vector<unsigned char> sysex_rst_msg = {0x7E, 0x7F, 0x09, 0x01};
	this->SendSysexEvent(sysex_rst_msg);
}

/**
 * Generic helper for sending SYSEX messages.
 *
 * Note that this sends all SYSEX messages as "direct"/unscheduled events
 * (skips tick queue).
 *
 * @param data The SYSEX message data (excluding 0xF0/0xF7 begin/end markers).
 *
 * @see SendEvent()
 */
void MusicDriver_AlsaMidi::SendSysexEvent(const std::vector<uint8_t> data)
{
		snd_seq_event_t seqev;
		snd_seq_ev_clear(&seqev);
		snd_seq_ev_set_source(&seqev, this->seq_port);
		snd_seq_ev_set_subs(&seqev);
		snd_seq_ev_set_direct(&seqev);

		std::vector<uint8_t> complete_message;
		complete_message.reserve(data.size() + 2);
		complete_message.push_back(0xF0);  // Start of SysEx
		complete_message.insert(complete_message.end(), data.begin(), data.end());
		complete_message.push_back(0xF7);  // End of SysEx
		snd_seq_ev_set_sysex(&seqev, complete_message.size(), complete_message.data());

		snd_seq_ev_set_source(&seqev, this->seq_port);
		snd_seq_ev_set_subs(&seqev);

		// TODO this assumes all SYSEX msgs are immediate, and not queued with a tick
		// this is correct for SYSEX GM RESET (all this is currently used for) but
		// might not be globally correct.
		snd_seq_ev_set_direct(&seqev);

		this->PushEvent(seqev);
}

/**
 * Generic helper for sending non-SYSEX messages.
 *
 * Converts MIDI events from the file into ALSA-specific sequencer queue events,
 * and schedules them on the tick-based sequencer queue.
 *
 * @param ev The raw MIDI event from the file.
 *
 * @see SendSysexEvent()
 */
void MusicDriver_AlsaMidi::SendEvent(const smf::MidiEvent& ev)
{
		snd_seq_event_t seqev;
		snd_seq_ev_clear(&seqev);

		unsigned int ticks = static_cast<unsigned int>(ev.tick);

		if (ev.isNoteOn()) {
			snd_seq_ev_set_noteon(&seqev, ev.getChannel(), ev[1], ev[2]);
		} else if (ev.isNoteOff()) {
			snd_seq_ev_set_noteoff(&seqev, ev.getChannel(), ev[1], ev[2]);
		} else if (ev.isController()) {
			snd_seq_ev_set_controller(&seqev, ev.getChannel(), ev[1], ev[2]);
		} else if (ev.isPatchChange()) {
			snd_seq_ev_set_pgmchange(&seqev, ev.getChannel(), ev[1]);
		} else if (ev.isPitchbend()) {
			snd_seq_ev_set_pitchbend(&seqev, ev.getChannel(), ((ev[2] << 7) | ev[1]) - 8192);
		} else if (ev.isPressure()) {
			snd_seq_ev_set_chanpress(&seqev, ev.getChannel(), ev[1]);
		} else if (ev.isAftertouch()) {
			snd_seq_ev_set_keypress(&seqev, ev.getChannel(), ev[1], ev[2]);
		} else if (ev.getCommandNibble() == 0xF0 && ev.getCommandByte() == MIDIST_SYSRESET) {
			Debug(driver, 2, "ALSA MIDI: reset event");
			snd_seq_ev_set_fixed(&seqev);
			seqev.type = SND_SEQ_EVENT_RESET;
		} else if (ev.isMeta()) {
			Debug(driver, 2, "ALSA MIDI: ignoring meta message");
			return;
		} else {
			Debug(driver, 2, "ALSA MIDI: unknown message: {}", ev.getCommandNibble());
			return;
		}

		// Schedule event
		snd_seq_ev_schedule_tick(&seqev, this->queue_id, 0, ticks);
		snd_seq_ev_set_source(&seqev, this->seq_port);
		snd_seq_ev_set_subs(&seqev);

		this->PushEvent(seqev);
}

/**
 * Waits until either:
 *
 * 1. The ALSA sequencer finishes processing up to the last event
 * that was enqueued, as measured by comparing the tick value of the
 * last event against the current tick value of the ALSA queue state.
 *
 * 2. `Stopping()` returns true, signaling early exit (don't wait for playback to finish).
 *
 * @param last_event_tick Tick value of the last event that was added to the queue.
 *
 * @see Stopping()
 */
void MusicDriver_AlsaMidi::WaitForFinish(const unsigned int last_event_tick)
{
	Debug(driver, 2, "ALSA MIDI: waiting for events finish");

	// First wait for queue to drain
	int res = 0;
	do {
		res = snd_seq_drain_output(this->seq);
		if (res != 0) {
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
	} while (res != 0);

	// Now get queue status and wait until we've passed the last scheduled tick
	snd_seq_queue_status_t *status;
	snd_seq_queue_status_alloca(&status);

	do {
		snd_seq_get_queue_status(this->seq, this->queue_id, status);
		const snd_seq_tick_time_t current_tick = snd_seq_queue_status_get_tick_time(status);

		if (this->Stopping()) {
			Debug(driver, 2, "ALSA MIDI: got stop signal, not waiting for events to finish");
			this->SendResetEvent();
			break;
		}

		if (current_tick >= last_event_tick) {
			// This is necessarily imprecise, just because the queue has processed the last
			// tick event doesn't mean whatever output device in use has played it yet,
			// but in practice this is good enough to not cut off the last few notes.
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	} while(true);

	Debug(driver, 2, "ALSA MIDI: events finished");
}

/**
 * Pushes an ALSA sequencer event onto the ALSA sequencer queue.
 *
 * If the push fails because the output buffer is full, drains the buffer
 * and retries.
 *
 * @param seqev ALSA sequencer event to push.
 */
void MusicDriver_AlsaMidi::PushEvent(snd_seq_event_t seqev)
{
		int res = 0;
		do {
			res = snd_seq_event_output(this->seq, &seqev);
			if (res == -EAGAIN) {
				Debug(driver, 2, "ALSA MIDI: output queue full, draining and retrying");
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			} else if (res < 0) {
				Debug(driver, 2, "ALSA MIDI: unknow error sending event to output queue! {}", res);
			}
			snd_seq_drain_output(this->seq);
		} while(res == -EAGAIN);
}

/**
 * Signals the queue thread to terminate and joins it.
 */
void MusicDriver_AlsaMidi::StopSong()
{
	this->stopping.store(true);

	Debug(driver, 2, "ALSA MIDI: StopSong waiting for queue thread");
	if (this->_queue_thread.joinable()) {
		this->_queue_thread.join();
	}

	Debug(driver, 2, "ALSA MIDI: stopping current queue");

	this->stopping.store(false);
	Debug(driver, 2, "ALSA MIDI: stopped song");
}

bool MusicDriver_AlsaMidi::Stopping()
{
	return this->stopping.load();
}

bool MusicDriver_AlsaMidi::IsSongPlaying()
{
	return this->playing.load();
}

void MusicDriver_AlsaMidi::SetVolume([[maybe_unused]]uint8_t vol)
{
	// TODO no-op, output device has its own volume control
}

/**
 * Updates the tempo of the current (started) ALSA sequencer queue.
 *
 * @param tempo_uspq Tempo value in units per quarter note.
 */
void MusicDriver_AlsaMidi::UpdateTempo(const int tempo_uspq)
{
	if (snd_seq_change_queue_tempo(this->seq, this->queue_id, tempo_uspq, nullptr) < 0) {
		throw std::runtime_error("Failed to update queue tempo");
	}
	snd_seq_drain_output(this->seq);
}

/**
 * Updates the Pulses Per Quarternote (PPQ) of the current ALSA sequencer queue.
 *
 * Note that the PPQ of an ALSA sequencer queue cannot be changed after it is started.
 *
 * @param ppq Pulse per quarter note value.
 *
 * @see StopQueue()
 */
void MusicDriver_AlsaMidi::SetPPQ(const int ppq)
{
	Debug(driver, 2, "ALSA MIDI: setting PPQ to {}", ppq);
	snd_seq_queue_tempo_t* tempo;
	snd_seq_queue_tempo_alloca(&tempo);
	snd_seq_queue_tempo_set_ppq(tempo, ppq);
	snd_seq_queue_tempo_set_tempo(tempo, 1000000); // 60 BPM

	snd_seq_queue_status_t *status;
	snd_seq_queue_status_alloca(&status);
	snd_seq_get_queue_status(seq, queue_id, status);

	if (snd_seq_queue_status_get_status(status) == 0) {
		if (snd_seq_set_queue_tempo(this->seq, this->queue_id, tempo) < 0) {
			throw std::runtime_error("Failed to set queue PPQ");
		}
		snd_seq_drain_output(this->seq);
	} else {
			Debug(driver, 2, "ALSA MIDI: tried to set PPQ on non-stopped queue!");
	}
}
