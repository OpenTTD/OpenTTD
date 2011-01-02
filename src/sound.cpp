/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file sound.cpp Handling of playing sounds. */

#include "stdafx.h"
#include "landscape.h"
#include "mixer.h"
#include "newgrf_sound.h"
#include "fios.h"
#include "window_gui.h"
#include "vehicle_base.h"

/* The type of set we're replacing */
#define SET_TYPE "sounds"
#include "base_media_func.h"

static SoundEntry _original_sounds[ORIGINAL_SAMPLE_COUNT];
MusicFileSettings _msf;

static void OpenBankFile(const char *filename)
{
	memset(_original_sounds, 0, sizeof(_original_sounds));

	/* If there is no sound file (nosound set), don't load anything */
	if (filename == NULL) return;

	FioOpenFile(SOUND_SLOT, filename);
	size_t pos = FioGetPos();
	uint count = FioReadDword();

	/* The new format has the highest bit always set */
	bool new_format = HasBit(count, 31);
	ClrBit(count, 31);
	count /= 8;

	/* Simple check for the correct number of original sounds. */
	if (count != ORIGINAL_SAMPLE_COUNT) {
		/* Corrupt sample data? Just leave the allocated memory as those tell
		 * there is no sound to play (size = 0 due to calloc). Not allocating
		 * the memory disables valid NewGRFs that replace sounds. */
		DEBUG(misc, 6, "Incorrect number of sounds in '%s', ignoring.", filename);
		return;
	}

	FioSeekTo(pos, SEEK_SET);

	for (uint i = 0; i != ORIGINAL_SAMPLE_COUNT; i++) {
		_original_sounds[i].file_slot = SOUND_SLOT;
		_original_sounds[i].file_offset = GB(FioReadDword(), 0, 31) + pos;
		_original_sounds[i].file_size = FioReadDword();
	}

	for (uint i = 0; i != ORIGINAL_SAMPLE_COUNT; i++) {
		SoundEntry *sound = &_original_sounds[i];
		char name[255];

		FioSeekTo(sound->file_offset, SEEK_SET);

		/* Check for special case, see else case */
		FioReadBlock(name, FioReadByte()); // Read the name of the sound
		if (new_format || strcmp(name, "Corrupt sound") != 0) {
			FioSeekTo(12, SEEK_CUR); // Skip past RIFF header

			/* Read riff tags */
			for (;;) {
				uint32 tag = FioReadDword();
				uint32 size = FioReadDword();

				if (tag == ' tmf') {
					FioReadWord(); // wFormatTag
					sound->channels = FioReadWord();        // wChannels
					sound->rate     = FioReadDword();       // samples per second
					if (!new_format) sound->rate = 11025;   // seems like all old samples should be played at this rate.
					FioReadDword();                         // avg bytes per second
					FioReadWord();                          // alignment
					sound->bits_per_sample = FioReadByte(); // bits per sample
					FioSeekTo(size - (2 + 2 + 4 + 4 + 2 + 1), SEEK_CUR);
				} else if (tag == 'atad') {
					sound->file_size = size;
					sound->file_slot = SOUND_SLOT;
					sound->file_offset = FioGetPos();
					break;
				} else {
					sound->file_size = 0;
					break;
				}
			}
		} else {
			/*
			 * Special case for the jackhammer sound
			 * (name in sample.cat is "Corrupt sound")
			 * It's no RIFF file, but raw PCM data
			 */
			sound->channels = 1;
			sound->rate = 11025;
			sound->bits_per_sample = 8;
			sound->file_slot = SOUND_SLOT;
			sound->file_offset = FioGetPos();
		}
	}
}

static bool SetBankSource(MixerChannel *mc, const SoundEntry *sound)
{
	assert(sound != NULL);

	if (sound->file_size == 0) return false;

	int8 *mem = MallocT<int8>(sound->file_size + 2);
	/* Add two extra bytes so rate conversion can read these
	 * without reading out of its input buffer. */
	mem[sound->file_size    ] = 0;
	mem[sound->file_size + 1] = 0;

	FioSeekToFile(sound->file_slot, sound->file_offset);
	FioReadBlock(mem, sound->file_size);

	/* 16-bit PCM WAV files should be signed by default */
	if (sound->bits_per_sample == 8) {
		for (uint i = 0; i != sound->file_size; i++) {
			mem[i] += -128; // Convert unsigned sound data to signed
		}
	}

#if TTD_ENDIAN == TTD_BIG_ENDIAN
	if (sound->bits_per_sample == 16) {
		uint num_samples = sound->file_size / 2;
		int16 *samples = (int16 *)mem;
		for (uint i = 0; i < num_samples; i++) {
			samples[i] = BSWAP16(samples[i]);
		}
	}
#endif

	assert(sound->bits_per_sample == 8 || sound->bits_per_sample == 16);
	assert(sound->channels == 1);
	assert(sound->file_size != 0 && sound->rate != 0);

	MxSetChannelRawSrc(mc, mem, sound->file_size, sound->rate, sound->bits_per_sample == 16);

	return true;
}

void InitializeSound()
{
	DEBUG(misc, 1, "Loading sound effects...");
	OpenBankFile(BaseSounds::GetUsedSet()->files->filename);
}

/* Low level sound player */
static void StartSound(SoundID sound_id, float pan, uint volume)
{
	if (volume == 0) return;

	const SoundEntry *sound = GetSound(sound_id);
	if (sound == NULL) return;

	/* Empty sound? */
	if (sound->rate == 0) return;

	MixerChannel *mc = MxAllocateChannel();
	if (mc == NULL) return;

	if (!SetBankSource(mc, sound)) return;

	/* Apply the sound effect's own volume. */
	volume = sound->volume * volume;

	MxSetChannelVolume(mc, volume, pan);
	MxActivateChannel(mc);
}


static const byte _vol_factor_by_zoom[] = {255, 190, 134, 87};
assert_compile(lengthof(_vol_factor_by_zoom) == ZOOM_LVL_COUNT);

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
	for (uint i = 0; i < ORIGINAL_SAMPLE_COUNT; i++) {
		SoundEntry *sound = AllocateSound();
		*sound = _original_sounds[_sound_idx[i]];
		sound->volume = _sound_base_vol[i];
		sound->priority = 0;
	}
}

/**
 * Decide 'where' (between left and right speaker) to play the sound effect.
 * @param sound Sound effect to play
 * @param left   Left edge of virtual coordinates where the sound is produced
 * @param right  Right edge of virtual coordinates where the sound is produced
 * @param top    Top edge of virtual coordinates where the sound is produced
 * @param bottom Bottom edge of virtual coordinates where the sound is produced
 */
static void SndPlayScreenCoordFx(SoundID sound, int left, int right, int top, int bottom)
{
	if (_msf.effect_vol == 0) return;

	const Window *w;
	FOR_ALL_WINDOWS_FROM_BACK(w) {
		const ViewPort *vp = w->viewport;

		if (vp != NULL &&
				left < vp->virtual_left + vp->virtual_width && right > vp->virtual_left &&
				top < vp->virtual_top + vp->virtual_height && bottom > vp->virtual_top) {
			int screen_x = (left + right) / 2 - vp->virtual_left;
			int width = (vp->virtual_width == 0 ? 1 : vp->virtual_width);
			float panning = (float)screen_x / width;

			StartSound(
				sound,
				panning,
				(_msf.effect_vol * _vol_factor_by_zoom[vp->zoom - ZOOM_LVL_BEGIN]) / 256
			);
			return;
		}
	}
}

void SndPlayTileFx(SoundID sound, TileIndex tile)
{
	/* emits sound from center of the tile */
	int x = min(MapMaxX() - 1, TileX(tile)) * TILE_SIZE + TILE_SIZE / 2;
	int y = min(MapMaxY() - 1, TileY(tile)) * TILE_SIZE - TILE_SIZE / 2;
	uint z = (y < 0 ? 0 : GetSlopeZ(x, y));
	Point pt = RemapCoords(x, y, z);
	y += 2 * TILE_SIZE;
	Point pt2 = RemapCoords(x, y, GetSlopeZ(x, y));
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
	StartSound(sound, 0.5, _msf.effect_vol);
}

INSTANTIATE_BASE_MEDIA_METHODS(BaseMedia<SoundsSet>, SoundsSet)

/** Names corresponding to the sound set's files */
static const char * const _sound_file_names[] = { "samples" };


template <class T, size_t Tnum_files, Subdirectory Tsubdir>
/* static */ const char * const *BaseSet<T, Tnum_files, Tsubdir>::file_names = _sound_file_names;

template <class Tbase_set>
/* static */ const char *BaseMedia<Tbase_set>::GetExtension()
{
	return ".obs"; // OpenTTD Base Sounds
}

template <class Tbase_set>
/* static */ bool BaseMedia<Tbase_set>::DetermineBestSet()
{
	if (BaseMedia<Tbase_set>::used_set != NULL) return true;

	const Tbase_set *best = NULL;
	for (const Tbase_set *c = BaseMedia<Tbase_set>::available_sets; c != NULL; c = c->next) {
		/* Skip unuseable sets */
		if (c->GetNumMissing() != 0) continue;

		if (best == NULL ||
				(best->fallback && !c->fallback) ||
				best->valid_files < c->valid_files ||
				(best->valid_files == c->valid_files &&
					(best->shortname == c->shortname && best->version < c->version))) {
			best = c;
		}
	}

	BaseMedia<Tbase_set>::used_set = best;
	return BaseMedia<Tbase_set>::used_set != NULL;
}

