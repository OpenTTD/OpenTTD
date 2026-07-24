/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file sampled_music.cpp Playback for sampled music tracks. */

#include "../stdafx.h"

#include "sampled_music.h"

#include "../debug.h"
#include "../fileio_func.h"
#include "../mixer.h"

#include "../safeguards.h"

bool MusicTrackUsesSampledPlayback(MusicTrackType filetype)
{
	return filetype == MTT_OPUS;
}

#ifdef WITH_OPUSFILE

#include "../misc/autorelease.hpp"

#include <mutex>
#include <opusfile.h>

static constexpr uint32_t OPUS_SAMPLE_RATE = 48000; ///< OpusFile always decodes at 48 kHz.
static constexpr size_t OPUS_DECODE_BUFFER_FRAMES = 5760; ///< 120 ms at 48 kHz.

struct OpusFileSource {
	std::optional<FileHandle> file;
	long start = 0;
	size_t size = 0;

	FILE *GetFile()
	{
		return static_cast<FILE *>(*this->file);
	}
};

static int OpusRead(void *stream, unsigned char *ptr, int nbytes)
{
	OpusFileSource *source = static_cast<OpusFileSource *>(stream);
	FILE *file = source->GetFile();

	long current_pos = ftell(file);
	if (current_pos < source->start) return 0;

	size_t relative_pos = static_cast<size_t>(current_pos - source->start);
	if (relative_pos >= source->size) return 0;

	size_t remaining = source->size - relative_pos;
	size_t bytes_to_read = std::min<size_t>(static_cast<size_t>(nbytes), remaining);
	return static_cast<int>(fread(ptr, 1, bytes_to_read, file));
}

static int OpusSeek(void *stream, opus_int64 offset, int whence)
{
	OpusFileSource *source = static_cast<OpusFileSource *>(stream);
	FILE *file = source->GetFile();

	opus_int64 target = 0;
	switch (whence) {
		case SEEK_SET:
			target = offset;
			break;

		case SEEK_CUR: {
			long current_pos = ftell(file);
			if (current_pos < 0) return -1;
			target = current_pos - source->start + offset;
			break;
		}

		case SEEK_END:
			target = static_cast<opus_int64>(source->size) + offset;
			break;

		default:
			return -1;
	}

	if (target < 0 || static_cast<uint64_t>(target) > source->size) return -1;
	return fseek(file, source->start + static_cast<long>(target), SEEK_SET);
}

static opus_int64 OpusTell(void *stream)
{
	OpusFileSource *source = static_cast<OpusFileSource *>(stream);
	long current_pos = ftell(source->GetFile());
	if (current_pos < source->start) return -1;
	return current_pos - source->start;
}

static int OpusClose(void *)
{
	return 0;
}

static const OpusFileCallbacks _opus_callbacks = {
	OpusRead,
	OpusSeek,
	OpusTell,
	OpusClose,
};

struct SampledMusicPlayback {
	std::mutex mutex;

	std::unique_ptr<OpusFileSource> source;
	AutoRelease<OggOpusFile, op_free> opus;

	std::vector<int16_t> decode_buffer{};
	size_t decoded_frames = 0;
	size_t decoded_pos = 0;
	uint32_t frac_pos = 0;
	uint32_t frac_speed = 0;

	uint8_t volume = 127;
	bool loop = false;
	bool playing = false;
	bool stream_active = false;
};

static SampledMusicPlayback _sampled_music;

static bool RefillDecodeBuffer()
{
	size_t remaining_frames = _sampled_music.decoded_pos < _sampled_music.decoded_frames ? _sampled_music.decoded_frames - _sampled_music.decoded_pos : 0;
	if (remaining_frames != 0 && _sampled_music.decoded_pos != 0) {
		std::copy_n(_sampled_music.decode_buffer.data() + _sampled_music.decoded_pos * 2, remaining_frames * 2, _sampled_music.decode_buffer.data());
	}
	_sampled_music.decoded_frames = remaining_frames;
	_sampled_music.decoded_pos = 0;

	while (_sampled_music.decoded_frames < 2 && _sampled_music.playing) {
		int16_t *target = _sampled_music.decode_buffer.data() + _sampled_music.decoded_frames * 2;
		int values_available = static_cast<int>(_sampled_music.decode_buffer.size() - _sampled_music.decoded_frames * 2);
		int read = op_read_stereo(_sampled_music.opus.get(), target, values_available);

		if (read == 0) {
			if (_sampled_music.loop && op_pcm_seek(_sampled_music.opus.get(), 0) == 0) continue;
			_sampled_music.playing = false;
			break;
		}

		if (read < 0) {
			_sampled_music.playing = false;
			break;
		}

		_sampled_music.decoded_frames += read;
	}

	return _sampled_music.decoded_frames - _sampled_music.decoded_pos >= 2;
}

static int16_t InterpolateSample(int16_t current, int16_t next, uint32_t frac)
{
	int sample = (static_cast<int>(current) * static_cast<int>(0x10000 - frac) + static_cast<int>(next) * static_cast<int>(frac)) >> 16;
	return static_cast<int16_t>(sample);
}

static int16_t ScaleSample(int16_t sample, uint8_t volume)
{
	if (volume == 127) return sample;
	return static_cast<int16_t>(static_cast<int>(sample) * volume / 127);
}

static void RenderSampledMusic(int16_t *buffer, size_t samples)
{
	std::unique_lock<std::mutex> lock{_sampled_music.mutex, std::try_to_lock};
	if (!lock.owns_lock() || !_sampled_music.playing || _sampled_music.opus == nullptr) return;

	for (size_t out = 0; out < samples; out++) {
		if (_sampled_music.decoded_pos + 1 >= _sampled_music.decoded_frames && !RefillDecodeBuffer()) return;

		const int16_t *current = _sampled_music.decode_buffer.data() + _sampled_music.decoded_pos * 2;
		const int16_t *next = current + 2;

		buffer[out * 2] = ScaleSample(InterpolateSample(current[0], next[0], _sampled_music.frac_pos), _sampled_music.volume);
		buffer[out * 2 + 1] = ScaleSample(InterpolateSample(current[1], next[1], _sampled_music.frac_pos), _sampled_music.volume);

		_sampled_music.frac_pos += _sampled_music.frac_speed;
		_sampled_music.decoded_pos += _sampled_music.frac_pos >> 16;
		_sampled_music.frac_pos &= 0xffff;
	}
}

void StopSampledMusic()
{
	bool stream_active = false;
	{
		std::lock_guard<std::mutex> lock{_sampled_music.mutex};
		stream_active = _sampled_music.stream_active;
	}

	if (!stream_active) return;

	MxSetMusicSource(nullptr);

	std::lock_guard<std::mutex> lock{_sampled_music.mutex};
	_sampled_music.opus.reset();
	_sampled_music.source.reset();
	_sampled_music.decode_buffer.clear();
	_sampled_music.decoded_frames = 0;
	_sampled_music.decoded_pos = 0;
	_sampled_music.frac_pos = 0;
	_sampled_music.frac_speed = 0;
	_sampled_music.playing = false;
	_sampled_music.stream_active = false;
}

bool PlaySampledMusic(const MusicSongInfo &song, uint8_t volume)
{
	if (song.filetype != MTT_OPUS) return false;

	StopSampledMusic();

	uint32_t mixer_rate = MxSetMusicSource(nullptr);

	size_t file_size = 0;
	auto file = FioFOpenFile(song.filename, "rb", Subdirectory::Baseset, &file_size);
	if (!file.has_value()) {
		Debug(driver, 0, "Could not open sampled music file '{}'", song.filename);
		return false;
	}

	auto source = std::make_unique<OpusFileSource>();
	source->file = std::move(file);
	source->size = file_size;
	source->start = ftell(source->GetFile());
	if (source->start < 0) {
		Debug(driver, 0, "Could not read sampled music file '{}'", song.filename);
		return false;
	}

	int error = 0;
	AutoRelease<OggOpusFile, op_free> opus{op_open_callbacks(source.get(), &_opus_callbacks, nullptr, 0, &error)};
	if (error != 0 || opus == nullptr) {
		Debug(driver, 0, "Could not open Ogg Opus music file '{}'", song.filename);
		return false;
	}

	{
		std::lock_guard<std::mutex> lock{_sampled_music.mutex};
		_sampled_music.source = std::move(source);
		_sampled_music.opus = std::move(opus);
		_sampled_music.decode_buffer.assign(OPUS_DECODE_BUFFER_FRAMES * 2, 0);
		_sampled_music.decoded_frames = 0;
		_sampled_music.decoded_pos = 0;
		_sampled_music.frac_pos = 0;
		_sampled_music.frac_speed = static_cast<uint32_t>((static_cast<uint64_t>(OPUS_SAMPLE_RATE) << 16) / mixer_rate);
		_sampled_music.volume = volume;
		_sampled_music.loop = song.loop;
		_sampled_music.playing = true;
		_sampled_music.stream_active = true;
	}

	MxSetMusicSource(RenderSampledMusic);
	return true;
}

bool IsSampledMusicPlaying()
{
	std::lock_guard<std::mutex> lock{_sampled_music.mutex};
	return _sampled_music.playing;
}

void SetSampledMusicVolume(uint8_t volume)
{
	std::lock_guard<std::mutex> lock{_sampled_music.mutex};
	_sampled_music.volume = volume;
}

#else /* WITH_OPUSFILE */

bool PlaySampledMusic(const MusicSongInfo &song, uint8_t)
{
	if (song.filetype == MTT_OPUS) Debug(driver, 0, "Ogg Opus music support is not available in this build");
	return false;
}

void StopSampledMusic()
{
}

bool IsSampledMusicPlaying()
{
	return false;
}

void SetSampledMusicVolume(uint8_t)
{
}

#endif /* WITH_OPUSFILE */
