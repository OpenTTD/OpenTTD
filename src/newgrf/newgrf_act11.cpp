/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_act11.cpp NewGRF Action 0x11 handler. */

#include "../stdafx.h"
#include "../debug.h"
#include "../newgrf_house.h"
#include "../newgrf_sound.h"
#include "../spritecache.h"
#include "newgrf_bytereader.h"
#include "newgrf_internal.h"

#include "../safeguards.h"

/**
 * Process a sound import from another GRF file.
 * @param sound Destination for sound.
 */
static void ImportGRFSound(SoundEntry *sound)
{
	const GRFFile *file;
	uint32_t grfid = _cur_gps.file->ReadDword();
	SoundID sound_id = _cur_gps.file->ReadWord();

	file = GetFileByGRFID(grfid);
	if (file == nullptr || file->sound_offset == 0) {
		GrfMsg(1, "ImportGRFSound: Source file not available");
		return;
	}

	if (sound_id >= file->num_sounds) {
		GrfMsg(1, "ImportGRFSound: Sound effect {} is invalid", sound_id);
		return;
	}

	GrfMsg(2, "ImportGRFSound: Copying sound {} ({}) from file {:x}", sound_id, file->sound_offset + sound_id, grfid);

	*sound = *GetSound(file->sound_offset + sound_id);

	/* Reset volume and priority, which TTDPatch doesn't copy */
	sound->volume = SOUND_EFFECT_MAX_VOLUME;
	sound->priority = 0;
}

/**
 * Load a sound from a file.
 * @param offs File offset to read sound from.
 * @param sound Destination for sound.
 */
static void LoadGRFSound(size_t offs, SoundEntry *sound)
{
	/* Set default volume and priority */
	sound->volume = SOUND_EFFECT_MAX_VOLUME;
	sound->priority = 0;

	if (offs != SIZE_MAX) {
		/* Sound is present in the NewGRF. */
		sound->file = _cur_gps.file;
		sound->file_offset = offs;
		sound->source = SoundSource::NewGRF;
		sound->grf_container_ver = _cur_gps.file->GetContainerVersion();
	}
}

/* Action 0x11 */
static void GRFSound(ByteReader &buf)
{
	/* <11> <num>
	 *
	 * W num      Number of sound files that follow */

	uint16_t num = buf.ReadWord();
	if (num == 0) return;

	SoundEntry *sound;
	if (_cur_gps.grffile->sound_offset == 0) {
		_cur_gps.grffile->sound_offset = GetNumSounds();
		_cur_gps.grffile->num_sounds = num;
		sound = AllocateSound(num);
	} else {
		sound = GetSound(_cur_gps.grffile->sound_offset);
	}

	SpriteFile &file = *_cur_gps.file;
	uint8_t grf_container_version = file.GetContainerVersion();
	for (int i = 0; i < num; i++) {
		_cur_gps.nfo_line++;

		/* Check whether the index is in range. This might happen if multiple action 11 are present.
		 * While this is invalid, we do not check for this. But we should prevent it from causing bigger trouble */
		bool invalid = i >= _cur_gps.grffile->num_sounds;

		size_t offs = file.GetPos();

		uint32_t len = grf_container_version >= 2 ? file.ReadDword() : file.ReadWord();
		uint8_t type = file.ReadByte();

		if (grf_container_version >= 2 && type == 0xFD) {
			/* Reference to sprite section. */
			if (invalid) {
				GrfMsg(1, "GRFSound: Sound index out of range (multiple Action 11?)");
				file.SkipBytes(len);
			} else if (len != 4) {
				GrfMsg(1, "GRFSound: Invalid sprite section import");
				file.SkipBytes(len);
			} else {
				uint32_t id = file.ReadDword();
				if (_cur_gps.stage == GLS_INIT) LoadGRFSound(GetGRFSpriteOffset(id), sound + i);
			}
			continue;
		}

		if (type != 0xFF) {
			GrfMsg(1, "GRFSound: Unexpected RealSprite found, skipping");
			file.SkipBytes(7);
			SkipSpriteData(*_cur_gps.file, type, len - 8);
			continue;
		}

		if (invalid) {
			GrfMsg(1, "GRFSound: Sound index out of range (multiple Action 11?)");
			file.SkipBytes(len);
		}

		uint8_t action = file.ReadByte();
		switch (action) {
			case 0xFF:
				/* Allocate sound only in init stage. */
				if (_cur_gps.stage == GLS_INIT) {
					if (grf_container_version >= 2) {
						GrfMsg(1, "GRFSound: Inline sounds are not supported for container version >= 2");
					} else {
						LoadGRFSound(offs, sound + i);
					}
				}
				file.SkipBytes(len - 1); // already read <action>
				break;

			case 0xFE:
				if (_cur_gps.stage == GLS_ACTIVATION) {
					/* XXX 'Action 0xFE' isn't really specified. It is only mentioned for
					 * importing sounds, so this is probably all wrong... */
					if (file.ReadByte() != 0) GrfMsg(1, "GRFSound: Import type mismatch");
					ImportGRFSound(sound + i);
				} else {
					file.SkipBytes(len - 1); // already read <action>
				}
				break;

			default:
				GrfMsg(1, "GRFSound: Unexpected Action {:x} found, skipping", action);
				file.SkipBytes(len - 1); // already read <action>
				break;
		}
	}
}

/* Action 0x11 (SKIP) */
static void SkipAct11(ByteReader &buf)
{
	/* <11> <num>
	 *
	 * W num      Number of sound files that follow */

	_cur_gps.skip_sprites = buf.ReadWord();

	GrfMsg(3, "SkipAct11: Skipping {} sprites", _cur_gps.skip_sprites);
}

template <> void GrfActionHandler<0x11>::FileScan(ByteReader &buf) { SkipAct11(buf); }
template <> void GrfActionHandler<0x11>::SafetyScan(ByteReader &buf) { GRFUnsafe(buf); }
template <> void GrfActionHandler<0x11>::LabelScan(ByteReader &buf) { SkipAct11(buf); }
template <> void GrfActionHandler<0x11>::Init(ByteReader &buf) { SkipAct11(buf); }
template <> void GrfActionHandler<0x11>::Reserve(ByteReader &buf) { SkipAct11(buf); }
template <> void GrfActionHandler<0x11>::Activation(ByteReader &buf) { GRFSound(buf); }
