/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file sound.cpp Handling of playing sounds. */

#include "stdafx.h"
#include "landscape.h"
#include "sound_type.h"
#include "soundloader_func.h"
#include "mixer.h"
#include "newgrf_sound.h"
#include "random_access_file_type.h"
#include "window_func.h"
#include "window_gui.h"
#include "vehicle_base.h"
#include "base_media_func.h"
#include "base_media_sounds.h"

#include "safeguards.h"

static std::array<SoundEntry, ORIGINAL_SAMPLE_COUNT> _original_sounds;

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
		if (!LoadSound(*sound, sound_id)) {
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
	OpenBankFile(BaseSounds::GetUsedSet()->files[0].filename);
}


/* Low level sound player */
static void StartSound(SoundID sound_id, float pan, uint volume)
{
	if (volume == 0) return;

	SoundEntry *sound = GetSound(sound_id);
	if (sound == nullptr) return;

	if (sound->rate == 0) {
		/* If the sound's sample rate is not set then the sound needs to be loaded, but if the sound's file pointer
		 * is empty then an attempt was already made to load the sound but it failed. We don't want to try again. */
		if (sound->file == nullptr) return;
	}

	MixerChannel *mc = MxAllocateChannel();
	if (mc == nullptr) return;

	if (!SetBankSource(mc, sound, sound_id)) return;

	/* Apply the sound effect's own volume. */
	volume = sound->volume * volume;

	MxSetChannelVolume(mc, volume, pan);
	MxActivateChannel(mc);
}


static const uint8_t _vol_factor_by_zoom[] = {255, 255, 255, 190, 134, 87};
static_assert(lengthof(_vol_factor_by_zoom) == to_underlying(ZoomLevel::End));

static const uint8_t _sound_base_vol[] = {
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

static const uint8_t _sound_idx[] = {
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

	auto set = BaseSounds::GetSet(index);
	BaseSounds::ini_set = set->name;
	BaseSounds::SetSet(set);

	MxCloseAllChannels();
	InitializeSound();

	/* Replace baseset sounds in the pool with the updated original sounds. This is safe to do as
	 * any sound still playing holds its own shared_ptr to the sample data. */
	for (uint i = 0; i < ORIGINAL_SAMPLE_COUNT; i++) {
		SoundEntry *sound = GetSound(i);
		/* GRF Container 0 means the sound comes from the baseset, and isn't overridden by NewGRF. */
		if (sound == nullptr || sound->grf_container_ver != 0) continue;

		*sound = _original_sounds[_sound_idx[i]];
		sound->volume = _sound_base_vol[i];
		sound->priority = 0;
	}

	InvalidateWindowData(WC_GAME_OPTIONS, WN_GAME_OPTIONS_GAME_OPTIONS, 0, true);
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
		if (w->viewport == nullptr) continue;

		const Viewport &vp = *w->viewport;
		if (left < vp.virtual_left + vp.virtual_width && right > vp.virtual_left &&
				top < vp.virtual_top + vp.virtual_height && bottom > vp.virtual_top) {
			int screen_x = (left + right) / 2 - vp.virtual_left;
			int width = (vp.virtual_width == 0 ? 1 : vp.virtual_width);
			float panning = (float)screen_x / width;

			StartSound(
				sound,
				panning,
				_vol_factor_by_zoom[to_underlying(vp.zoom)]
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

/**
 * Play a beep sound for a click event if enabled in settings.
 */
void SndClickBeep()
{
	if (_settings_client.sound.click_beep) SndPlayFx(SND_15_BEEP);
}

/**
 * Play a beep sound for a confirm event if enabled in settings.
 */
void SndConfirmBeep()
{
	if (_settings_client.sound.confirm) SndPlayFx(SND_15_BEEP);
}

/** Names corresponding to the sound set's files */
static const std::string_view _sound_file_names[] = { "samples" };

template <>
/* static */ std::span<const std::string_view> BaseSet<SoundsSet>::GetFilenames()
{
	return _sound_file_names;
}

template <>
/* static */ std::string_view BaseMedia<SoundsSet>::GetExtension()
{
	return ".obs"; // OpenTTD Base Sounds
}

template <>
/* static */ bool BaseMedia<SoundsSet>::DetermineBestSet()
{
	if (BaseMedia<SoundsSet>::used_set != nullptr) return true;

	const SoundsSet *best = nullptr;
	for (const auto &c : BaseMedia<SoundsSet>::available_sets) {
		/* Skip unusable sets */
		if (c->GetNumMissing() != 0) continue;

		if (best == nullptr ||
				(best->fallback && !c->fallback) ||
				best->valid_files < c->valid_files ||
				(best->valid_files == c->valid_files &&
					(best->shortname == c->shortname && best->version < c->version))) {
			best = c.get();
		}
	}

	BaseMedia<SoundsSet>::used_set = best;
	return BaseMedia<SoundsSet>::used_set != nullptr;
}

template class BaseMedia<SoundsSet>;
