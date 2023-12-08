/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file sound.cpp Handling of playing sounds. */

#include "stdafx.h"
#include "core/endian_func.hpp"
#include "landscape.h"
#include "mixer.h"
#include "newgrf_sound.h"
#include "random_access_file_type.h"
#include "window_gui.h"
#include "vehicle_base.h"

#ifdef WITH_LIBMAD
#include <mad.h>
#endif /* WITH_LIBMAD */

/* The type of set we're replacing */
#define SET_TYPE "sounds"
#include "base_media_func.h"

#include "safeguards.h"

static std::array<SoundEntry, ORIGINAL_SAMPLE_COUNT> _original_sounds;

/**
 * Convert u8 samples to i8.
 * @param in buffer of samples to convert.
 */
void NormaliseInt8(std::vector<byte> &in)
{
	/* Convert 8-bit samples from unsigned to signed. */
	uint8_t *inb = reinterpret_cast<uint8_t *>(&*std::begin(in));
	uint8_t *ine = reinterpret_cast<uint8_t *>(&*std::end(in)) - 1;
	for (; inb != ine; ++inb) {
		*inb = *inb - 128;
	}
}

/**
 * Convert i16 samples from little endian.
 * @param in buffer of samples to convert.
 */
void NormaliseInt16(std::vector<byte> &in)
{
	/* Buffer sized must be aligned to 2 bytes. */
	assert((in.size() & 1) == 0);

	/* Convert samples from little endian. On a LE system this will do nothing. */
	int16_t *inb = reinterpret_cast<int16_t *>(&*std::begin(in));
	int16_t *ine = reinterpret_cast<int16_t *>(&*std::end(in));
	for (; inb != ine; ++inb) {
		*inb = FROM_LE16(*inb);
	}
}

/**
 * Raw PCM sound loader, used as a fallback if the WAV sound loader fails.
 * @param[in,out] sound Sound to load. Playback parameters will be filled in.
 * @param new_format Whether this sound comes from a new format file.
 * @param[in,out] data buffer to load sound data into.
 */
static bool LoadSoundRaw(SoundEntry &sound, bool new_format, std::vector<byte> &data)
{
	/* No raw sounds are permitted with a new format file. */
	if (new_format) return false;

	/*
	 * Special case for the jackhammer sound
	 * (name in Windows sample.cat is "Corrupt sound")
	 * It's no RIFF file, but raw PCM data
	 */
	sound.channels = 1;
	sound.rate = 11025;
	sound.bits_per_sample = 8;

	/* Allocate an extra sample to ensure the runtime resampler doesn't go out of bounds.*/
	data.resize(sound.file_size + 1);
	sound.file->ReadBlock(data.data(), sound.file_size);

	NormaliseInt8(data);

	return true;
}

/**
 * Wav file (RIFF/WAVE) sound louder.
 * @param[in,out] sound Sound to load. Playback parameters will be filled in.
 * @param new_format Whether this sound comes from a new format file.
 * @param[in,out] data buffer to load sound data into.
 */
static bool LoadSoundWav(SoundEntry &sound, bool new_format, std::vector<byte> &data)
{
	RandomAccessFile &file = *sound.file;

	/* Check RIFF/WAVE header. */
	if (file.ReadDword() != BSWAP32('RIFF')) return false;
	file.ReadDword(); // Skip data size
	if (file.ReadDword() != BSWAP32('WAVE')) return false;

	/* Read riff tags */
	for (;;) {
		uint32_t tag = file.ReadDword();
		uint32_t size = file.ReadDword();

		if (tag == BSWAP32('fmt ')) {
			uint16_t format = file.ReadWord();        // wFormatTag
			if (format != 1) return false; // File must be Uncompressed PCM
			sound.channels = file.ReadWord();         // wChannels
			sound.rate     = file.ReadDword();        // samples per second
			if (!new_format) sound.rate = 11025;      // seems like all old samples should be played at this rate.
			file.ReadDword();                         // avg bytes per second
			file.ReadWord();                          // alignment
			sound.bits_per_sample = file.ReadWord();  // bits per sample
			if (sound.bits_per_sample != 8 && sound.bits_per_sample != 16) return false; // File must be 8 or 16 BPS.
		} else if (tag == BSWAP32('data')) {
			uint align = sound.channels * sound.bits_per_sample / 8;
			if ((size & (align - 1)) != 0) return false; // Ensure length is aligned correctly for channels and BPS.

			sound.file_size = size;
			if (size == 0) return true; // No need to continue.

			/* Allocate an extra sample to ensure the runtime resampler doesn't go out of bounds.*/
			data.resize(sound.file_size + sound.channels * sound.bits_per_sample / 8);
			file.ReadBlock(data.data(), sound.file_size);

			if (sound.bits_per_sample == 8) NormaliseInt8(data);
			if (sound.bits_per_sample == 16) NormaliseInt16(data);

			return true;
		} else {
			sound.file_size = 0;
			break;
		}
	}

	return false;
}

#ifdef WITH_LIBMAD

/* Struct to hold context during MP3 decoding. */
struct MadContext {
	SoundEntry &sound;
	std::vector<byte> &data;
	bool read;
	std::vector<byte> in_data{};
};

static mad_flow MadInputFunc(void *context, struct mad_stream *stream)
{
	MadContext *ctx = static_cast<MadContext *>(context);
	if (ctx->read) return MAD_FLOW_STOP;

	ctx->in_data.resize(ctx->sound.file_size);
	ctx->sound.file->ReadBlock(ctx->in_data.data(), ctx->in_data.size());
	ctx->read = true;

	mad_stream_buffer(stream, ctx->in_data.data(), ctx->in_data.size());

	return MAD_FLOW_CONTINUE;
}

static inline int16_t MadConvertToInt16(mad_fixed_t sample)
{
	if (sample >= MAD_F_ONE) return INT16_MAX;
	if (sample <= -MAD_F_ONE) return INT16_MIN;
	return sample >> (MAD_F_FRACBITS - 15);
}

static mad_flow MadOutputFunc(void *context, struct mad_header const *, struct mad_pcm *pcm)
{
	MadContext *ctx = static_cast<MadContext *>(context);

	ctx->sound.rate = pcm->samplerate;
	ctx->sound.bits_per_sample = 16;
	ctx->sound.channels = 1;

	const mad_fixed_t *lin = pcm->samples[0];
	// const mad_fixed_t *rin = pcm->samples[1];
	int nsamples = pcm->length;

	ctx->data.reserve(ctx->data.size() + nsamples * 2 + 2);
	while (nsamples--) {
		int16_t sample = MadConvertToInt16(*lin++);
		ctx->data.push_back(GB((uint16_t)sample, 0, 8));
		ctx->data.push_back(GB((uint16_t)sample, 8, 8));
	}

	return MAD_FLOW_CONTINUE;
}

static bool LoadSoundMp3(SoundEntry &sound, bool, std::vector<byte> &data)
{
	MadContext ctx{sound, data, false};

	struct mad_decoder decoder;
	mad_decoder_init(&decoder, &ctx, MadInputFunc, nullptr, nullptr, MadOutputFunc, nullptr, nullptr);
	mad_decoder_run(&decoder, MAD_DECODER_MODE_SYNC);
	mad_decoder_finish(&decoder);

	if (sound.rate == 0) return false;

	/* Add padding sample for resampler. */
	uint padding = sound.channels * sound.bits_per_sample / 8;
	while (padding--) data.push_back(0);

	return true;
}

#endif /* WITH_LIBMAD */

using SoundLoader = bool (*)(SoundEntry &sound, bool new_format, std::vector<byte> &data);

static std::initializer_list<SoundLoader> _sound_loaders = {
	LoadSoundWav,
#ifdef WITH_LIBMAD
	LoadSoundMp3,
#endif /* WITH_LIBMAD */
	LoadSoundRaw,
};

bool LoadSound(SoundEntry &sound, bool new_format, SoundID sound_id, const std::string &name)
{
	/* Check for valid sound size. */
	if (sound.file_size == 0 || sound.file_size > ((size_t)-1) - 2) return false;

	size_t pos = sound.file->GetPos();
	sound.data = std::make_shared<std::vector<byte>>();
	for (auto &sl : _sound_loaders) {
		sound.file->SeekTo(pos, SEEK_SET);
		if (sl(sound, new_format, *sound.data)) break;
	}

	if (sound.data->empty()) {
		/* Some sounds are unused so it does not matter if they are empty. */
		static const std::initializer_list<SoundID> UNUSED_SOUNDS = {
			SND_0D_UNUSED + 2, SND_11_UNUSED + 2, SND_22_UNUSED + 2, SND_23_UNUSED + 2, SND_32_UNUSED,
		};

		if (std::find(std::begin(UNUSED_SOUNDS), std::end(UNUSED_SOUNDS), sound_id) == std::end(UNUSED_SOUNDS)) {
			Debug(grf, 0, "LoadSound [{}]: Failed to load sound '{}' for slot {}", sound.file->GetSimplifiedFilename(), name, sound_id);
		}
		return false;
	}

	assert(sound.bits_per_sample == 8 || sound.bits_per_sample == 16);
	assert(sound.channels == 1);
	assert(!sound.data->empty() && sound.rate != 0);

	return true;
}

static bool LoadBasesetSound(SoundEntry &sound, bool new_format, SoundID sound_id)
{
	sound.file->SeekTo(sound.file_offset, SEEK_SET);

	/* Read name of sound for diagnostics. */
	size_t name_len = sound.file->ReadByte();
	std::string name(name_len, '\0');
	sound.file->ReadBlock(name.data(), name_len);

	return LoadSound(sound, new_format, sound_id, name);
}

static bool LoadSoundSource(SoundEntry &sound, SoundID sound_id)
{
	switch (sound.source) {
		case SoundSource::BasesetOldFormat: return LoadBasesetSound(sound, false, sound_id);
		case SoundSource::BasesetNewFormat: return LoadBasesetSound(sound, true, sound_id);
		case SoundSource::NewGRF: return LoadNewGRFSound(&sound, sound_id);
		default: NOT_REACHED();
	}
}

static void OpenBankFile(const std::string &filename)
{
	/**
	 * The sound file for the original sounds, i.e. those not defined/overridden by a NewGRF.
	 * Needs to be kept alive during the game as _original_sounds[n].file refers to this.
	 */
	static std::unique_ptr<RandomAccessFile> original_sound_file;

	_original_sounds.fill({});

	/* If there is no sound file (nosound set), don't load anything */
	if (filename.empty()) return;

	original_sound_file = std::make_unique<RandomAccessFile>(filename, BASESET_DIR);
	size_t pos = original_sound_file->GetPos();
	uint count = original_sound_file->ReadDword();

	/* The new format has the highest bit always set */
	auto source = HasBit(count, 31) ? SoundSource::BasesetNewFormat : SoundSource::BasesetOldFormat;
	ClrBit(count, 31);
	count /= 8;

	/* Simple check for the correct number of original sounds. */
	if (count != ORIGINAL_SAMPLE_COUNT) {
		/* Corrupt sample data? Just leave the allocated memory as those tell
		 * there is no sound to play (size = 0 due to calloc). Not allocating
		 * the memory disables valid NewGRFs that replace sounds. */
		Debug(misc, 6, "Incorrect number of sounds in '{}', ignoring.", filename);
		return;
	}

	original_sound_file->SeekTo(pos, SEEK_SET);

	/* Read sound file positions. */
	for (auto &sound : _original_sounds) {
		sound.file = original_sound_file.get();
		sound.file_offset = GB(original_sound_file->ReadDword(), 0, 31) + pos;
		sound.file_size = original_sound_file->ReadDword();
		sound.source = source;
	}
}

static bool SetBankSource(MixerChannel *mc, SoundEntry *sound, SoundID sound_id)
{
	assert(sound != nullptr);

	if (sound->file != nullptr) {
		if (!LoadSoundSource(*sound, sound_id)) {
			/* Mark as invalid. */
			sound->file = nullptr;
			return false;
		}
		sound->file = nullptr;
	}

	/* Check for valid sound. */
	if (sound->data->empty()) return false;

	MxSetChannelRawSrc(mc, sound->data, sound->rate, sound->bits_per_sample == 16);

	return true;
}

void InitializeSound()
{
	Debug(misc, 1, "Loading sound effects...");
	OpenBankFile(BaseSounds::GetUsedSet()->files->filename);
}


/* Low level sound player */
static void StartSound(SoundID sound_id, float pan, uint volume)
{
	if (volume == 0) return;

	SoundEntry *sound = GetSound(sound_id);
	if (sound == nullptr) return;

	/* Empty sound? */
	if (sound->rate == 0 && sound->file == nullptr) return;

	MixerChannel *mc = MxAllocateChannel();
	if (mc == nullptr) return;

	if (!SetBankSource(mc, sound, sound_id)) return;

	/* Apply the sound effect's own volume. */
	volume = sound->volume * volume;

	MxSetChannelVolume(mc, volume, pan);
	MxActivateChannel(mc);
}


static const byte _vol_factor_by_zoom[] = {255, 255, 255, 190, 134, 87};
static_assert(lengthof(_vol_factor_by_zoom) == ZOOM_LVL_END);

static const byte _sound_base_vol[] = {
	128,  90, 128, 128, 128, 128, 128, 128,
	128,  90,  90, 128, 128, 128, 128, 128,
	128, 128, 128,  80, 128, 128, 128, 128,
	128, 128, 128, 128, 128, 128, 128, 128,
	128, 128,  90,  90,  90, 128,  90, 128,
	128,  90, 128, 128, 128,  90, 128, 128,
	128, 128, 128, 128,  90, 128, 128, 128,
	128,  90, 128, 128, 128, 128, 128, 128,
	128, 128,  90,  90,  90, 128, 128, 128,
	 90,
};

static const byte _sound_idx[] = {
	 2,  3,  4,  5,  6,  7,  8,  9,
	10, 11, 12, 13, 14, 15, 16, 17,
	18, 19, 20, 21, 22, 23, 24, 25,
	26, 27, 28, 29, 30, 31, 32, 33,
	34, 35, 36, 37, 38, 39, 40,  0,
	 1, 41, 42, 43, 44, 45, 46, 47,
	48, 49, 50, 51, 52, 53, 54, 55,
	56, 57, 58, 59, 60, 61, 62, 63,
	64, 65, 66, 67, 68, 69, 70, 71,
	72,
};

void SndCopyToPool()
{
	SoundEntry *sound = AllocateSound(ORIGINAL_SAMPLE_COUNT);
	for (uint i = 0; i < ORIGINAL_SAMPLE_COUNT; i++) {
		sound[i] = _original_sounds[_sound_idx[i]];
		sound[i].volume = _sound_base_vol[i];
		sound[i].priority = 0;
	}
}

/**
 * Change the configured sound set and reset sounds.
 * @param index Index of sound set to switch to.
 */
void ChangeSoundSet(int index)
{
	if (BaseSounds::GetIndexOfUsedSet() == index) return;

	auto* set = BaseSounds::GetSet(index);
	BaseSounds::ini_set = set->name;
	BaseSounds::SetSet(set);

	MxCloseAllChannels();
	InitializeSound();

	/* Replace baseset sounds in the pool with the updated original sounds. This is safe to do as
	 * any sound still playing holds its own shared_ptr to the sample data. */
	for (uint i = 0; i < ORIGINAL_SAMPLE_COUNT; i++) {
		SoundEntry *sound = GetSound(i);
		if (sound != nullptr && sound->source != SoundSource::NewGRF) {
			*sound = _original_sounds[_sound_idx[i]];
			sound->volume = _sound_base_vol[i];
			sound->priority = 0;
		}
	}
}

/**
 * Decide 'where' (between left and right speaker) to play the sound effect.
 * Note: Callers must determine if sound effects are enabled. This plays a sound regardless of the setting.
 * @param sound Sound effect to play
 * @param left   Left edge of virtual coordinates where the sound is produced
 * @param right  Right edge of virtual coordinates where the sound is produced
 * @param top    Top edge of virtual coordinates where the sound is produced
 * @param bottom Bottom edge of virtual coordinates where the sound is produced
 */
static void SndPlayScreenCoordFx(SoundID sound, int left, int right, int top, int bottom)
{
	/* Iterate from back, so that main viewport is checked first */
	for (const Window *w : Window::IterateFromBack()) {
		const Viewport *vp = w->viewport;

		if (vp != nullptr &&
				left < vp->virtual_left + vp->virtual_width && right > vp->virtual_left &&
				top < vp->virtual_top + vp->virtual_height && bottom > vp->virtual_top) {
			int screen_x = (left + right) / 2 - vp->virtual_left;
			int width = (vp->virtual_width == 0 ? 1 : vp->virtual_width);
			float panning = (float)screen_x / width;

			StartSound(
				sound,
				panning,
				_vol_factor_by_zoom[vp->zoom]
			);
			return;
		}
	}
}

void SndPlayTileFx(SoundID sound, TileIndex tile)
{
	/* emits sound from center of the tile */
	int x = std::min(Map::MaxX() - 1, TileX(tile)) * TILE_SIZE + TILE_SIZE / 2;
	int y = std::min(Map::MaxY() - 1, TileY(tile)) * TILE_SIZE - TILE_SIZE / 2;
	int z = (y < 0 ? 0 : GetSlopePixelZ(x, y));
	Point pt = RemapCoords(x, y, z);
	y += 2 * TILE_SIZE;
	Point pt2 = RemapCoords(x, y, GetSlopePixelZ(x, y));
	SndPlayScreenCoordFx(sound, pt.x, pt2.x, pt.y, pt2.y);
}

void SndPlayVehicleFx(SoundID sound, const Vehicle *v)
{
	SndPlayScreenCoordFx(sound,
		v->coord.left, v->coord.right,
		v->coord.top, v->coord.bottom
	);
}

void SndPlayFx(SoundID sound)
{
	StartSound(sound, 0.5, UINT8_MAX);
}

INSTANTIATE_BASE_MEDIA_METHODS(BaseMedia<SoundsSet>, SoundsSet)

/** Names corresponding to the sound set's files */
static const char * const _sound_file_names[] = { "samples" };


template <class T, size_t Tnum_files, bool Tsearch_in_tars>
/* static */ const char * const *BaseSet<T, Tnum_files, Tsearch_in_tars>::file_names = _sound_file_names;

template <class Tbase_set>
/* static */ const char *BaseMedia<Tbase_set>::GetExtension()
{
	return ".obs"; // OpenTTD Base Sounds
}

template <class Tbase_set>
/* static */ bool BaseMedia<Tbase_set>::DetermineBestSet()
{
	if (BaseMedia<Tbase_set>::used_set != nullptr) return true;

	const Tbase_set *best = nullptr;
	for (const Tbase_set *c = BaseMedia<Tbase_set>::available_sets; c != nullptr; c = c->next) {
		/* Skip unusable sets */
		if (c->GetNumMissing() != 0) continue;

		if (best == nullptr ||
				(best->fallback && !c->fallback) ||
				best->valid_files < c->valid_files ||
				(best->valid_files == c->valid_files &&
					(best->shortname == c->shortname && best->version < c->version))) {
			best = c;
		}
	}

	BaseMedia<Tbase_set>::used_set = best;
	return BaseMedia<Tbase_set>::used_set != nullptr;
}

