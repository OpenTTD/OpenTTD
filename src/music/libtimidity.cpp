/* $Id$ */

#include "../stdafx.h"
#include "../openttd.h"
#include "../sound.h"
#include "../string.h"
#include "../variables.h"
#include "../debug.h"
#include "libtimidity.h"
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <errno.h>
#include <timidity.h>
#if defined(PSP)
#include <pspaudiolib.h>
#endif /* PSP */

enum MidiState {
	MIDI_STOPPED = 0,
	MIDI_PLAYING = 1,
};

static struct {
	MidIStream *stream;
	MidSongOptions options;
	MidSong *song;

	MidiState status;
	uint32 song_length;
	uint32 song_position;
} _midi;

#if defined(PSP)
static void AudioOutCallback(void *buf, unsigned int _reqn, void *userdata)
{
	memset(buf, 0, _reqn * PSP_NUM_AUDIO_CHANNELS);
	if (_midi.status == MIDI_PLAYING) {
		mid_song_read_wave(_midi.song, buf, _reqn * PSP_NUM_AUDIO_CHANNELS);
	}
}
#endif /* PSP */

static const char *LibtimidityMidiStart(const char *const *param)
{
	_midi.status = MIDI_STOPPED;

	if (mid_init(param == NULL ? NULL : (char *)param[0]) < 0) {
		/* If init fails, it can be because no configuration was found.
		 *  If it was not forced via param, try to load it without a
		 *  configuration. Who knows that works. */
		if (param != NULL || mid_init_no_config() < 0) {
			DEBUG(driver, 0, "error initializing timidity");
			return NULL;
		}
	}
	DEBUG(driver, 1, "successfully initialised timidity");

	_midi.options.rate = 44100;
	_midi.options.format = MID_AUDIO_S16LSB;
	_midi.options.channels = 2;
#if defined(PSP)
	_midi.options.buffer_size = PSP_NUM_AUDIO_SAMPLES;
#else
	_midi.options.buffer_size = _midi.options.rate;
#endif

#if defined(PSP)
	pspAudioInit();
	pspAudioSetChannelCallback(_midi.options.channels, &AudioOutCallback, NULL);
	pspAudioSetVolume(_midi.options.channels, PSP_VOLUME_MAX, PSP_VOLUME_MAX);
#endif /* PSP */

	return NULL;
}

static void LibtimidityMidiStop()
{
	if (_midi.status == MIDI_PLAYING) {
		_midi.status = MIDI_STOPPED;
		mid_song_free(_midi.song);
	}
	mid_exit();
}

static void LibtimidityMidiPlaySong(const char *filename)
{
	_midi.stream = mid_istream_open_file(filename);
	if (_midi.stream == NULL) {
		DEBUG(driver, 0, "Could not open music file");
		return;
	}

	_midi.song = mid_song_load(_midi.stream, &_midi.options);
	mid_istream_close(_midi.stream);
	_midi.song_length = mid_song_get_total_time(_midi.song);

	if (_midi.song == NULL) {
		DEBUG(driver, 1, "Invalid MIDI file");
		return;
	}

	mid_song_start(_midi.song);
	_midi.status = MIDI_PLAYING;
}

static void LibtimidityMidiStopSong()
{
	_midi.status = MIDI_STOPPED;
	mid_song_free(_midi.song);
}

static bool LibtimidityMidiIsPlaying()
{
	if (_midi.status == MIDI_PLAYING) {
		_midi.song_position = mid_song_get_time(_midi.song);
		if (_midi.song_position >= _midi.song_length) {
			_midi.status = MIDI_STOPPED;
			_midi.song_position = 0;
		}
	}

	return (_midi.status == MIDI_PLAYING);
}

static void LibtimidityMidiSetVolume(byte vol)
{
	if (_midi.song != NULL)
		mid_song_set_volume(_midi.song, vol);
}

const HalMusicDriver _libtimidity_music_driver = {
	LibtimidityMidiStart,
	LibtimidityMidiStop,
	LibtimidityMidiPlaySong,
	LibtimidityMidiStopSong,
	LibtimidityMidiIsPlaying,
	LibtimidityMidiSetVolume,
};

