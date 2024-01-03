/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file cocoa_m.cpp
 * @brief MIDI music player for MacOS X using CoreAudio.
 */


#ifdef WITH_COCOA

#include "../stdafx.h"
#include "../os/macosx/macos.h"
#include "cocoa_m.h"
#include "midifile.hpp"
#include "../debug.h"
#include "../base_media_base.h"

#include <CoreServices/CoreServices.h>
#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioToolbox.h>

#include "../safeguards.h"

#if !defined(HAVE_OSX_1011_SDK)
#define kMusicSequenceFile_AnyType 0
#endif

static FMusicDriver_Cocoa iFMusicDriver_Cocoa;


static MusicPlayer    _player = nullptr;
static MusicSequence  _sequence = nullptr;
static MusicTimeStamp _seq_length = 0;
static bool           _playing = false;
static byte           _volume = 127;


/** Set the volume of the current sequence. */
static void DoSetVolume()
{
	if (_sequence == nullptr) return;

	AUGraph graph;
	MusicSequenceGetAUGraph(_sequence, &graph);

	AudioUnit output_unit = nullptr;

	/* Get output audio unit */
	UInt32 node_count = 0;
	AUGraphGetNodeCount(graph, &node_count);
	for (UInt32 i = 0; i < node_count; i++) {
		AUNode node;
		AUGraphGetIndNode(graph, i, &node);

		AudioUnit unit;
		AudioComponentDescription desc;
		AUGraphNodeInfo(graph, node, &desc, &unit);

		if (desc.componentType == kAudioUnitType_Output) {
			output_unit = unit;
			break;
		}
	}
	if (output_unit == nullptr) {
		Debug(driver, 1, "cocoa_m: Failed to get output node to set volume");
		return;
	}

	Float32 vol = _volume / 127.0f;  // 0 - +127 -> 0.0 - 1.0
	AudioUnitSetParameter(output_unit, kHALOutputParam_Volume, kAudioUnitScope_Global, 0, vol, 0);
}


/**
 * Initialized the MIDI player, including QuickTime initialization.
 */
const char *MusicDriver_Cocoa::Start(const StringList &)
{
	if (NewMusicPlayer(&_player) != noErr) return "failed to create music player";

	return nullptr;
}


/**
 * Checks whether the player is active.
 */
bool MusicDriver_Cocoa::IsSongPlaying()
{
	if (!_playing) return false;

	MusicTimeStamp time = 0;
	MusicPlayerGetTime(_player, &time);
	return time < _seq_length;
}


/**
 * Stops the MIDI player.
 */
void MusicDriver_Cocoa::Stop()
{
	if (_player != nullptr) DisposeMusicPlayer(_player);
	if (_sequence != nullptr) DisposeMusicSequence(_sequence);
}


/**
 * Starts playing a new song.
 *
 * @param song Description of music to load and play
 */
void MusicDriver_Cocoa::PlaySong(const MusicSongInfo &song)
{
	std::string filename = MidiFile::GetSMFFile(song);

	Debug(driver, 2, "cocoa_m: trying to play '{}'", filename);

	this->StopSong();
	if (_sequence != nullptr) {
		DisposeMusicSequence(_sequence);
		_sequence = nullptr;
	}

	if (filename.empty()) return;

	if (NewMusicSequence(&_sequence) != noErr) {
		Debug(driver, 0, "cocoa_m: Failed to create music sequence");
		return;
	}

	std::string os_file = OTTD2FS(filename);
	CFAutoRelease<CFURLRef> url(CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault, (const UInt8*)os_file.c_str(), os_file.length(), false));

	if (MusicSequenceFileLoad(_sequence, url.get(), kMusicSequenceFile_AnyType, 0) != noErr) {
		Debug(driver, 0, "cocoa_m: Failed to load MIDI file");
		return;
	}

	/* Construct audio graph */
	AUGraph graph = nullptr;

	MusicSequenceGetAUGraph(_sequence, &graph);
	AUGraphOpen(graph);
	if (AUGraphInitialize(graph) != noErr) {
		Debug(driver, 0, "cocoa_m: Failed to initialize AU graph");
		return;
	}

	/* Figure out sequence length */
	UInt32 num_tracks;
	MusicSequenceGetTrackCount(_sequence, &num_tracks);
	_seq_length = 0;
	for (UInt32 i = 0; i < num_tracks; i++) {
		MusicTrack     track = nullptr;
		MusicTimeStamp track_length = 0;
		UInt32         prop_size = sizeof(MusicTimeStamp);
		MusicSequenceGetIndTrack(_sequence, i, &track);
		MusicTrackGetProperty(track, kSequenceTrackProperty_TrackLength, &track_length, &prop_size);
		if (track_length > _seq_length) _seq_length = track_length;
	}
	/* Add 8 beats for reverb/long note release */
	_seq_length += 8;

	DoSetVolume();
	MusicPlayerSetSequence(_player, _sequence);
	MusicPlayerPreroll(_player);
	if (MusicPlayerStart(_player) != noErr) return;
	_playing = true;

	Debug(driver, 3, "cocoa_m: playing '{}'", filename);
}


/**
 * Stops playing the current song, if the player is active.
 */
void MusicDriver_Cocoa::StopSong()
{
	MusicPlayerStop(_player);
	MusicPlayerSetSequence(_player, nullptr);
	_playing = false;
}


/**
 * Changes the playing volume of the MIDI player.
 *
 * @param vol The desired volume, range of the value is @c 0-127
 */
void MusicDriver_Cocoa::SetVolume(byte vol)
{
	_volume = vol;
	DoSetVolume();
}

#endif /* WITH_COCOA */
