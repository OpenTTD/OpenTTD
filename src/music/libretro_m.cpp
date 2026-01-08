/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file libretro_m.cpp Libretro music driver implementation. */

#ifdef WITH_LIBRETRO

#include "../stdafx.h"
#include "libretro_m.h"
#include "../base_media_music.h"
#include "../video/libretro/libretro_core.h"
#include "../video/libretro/libretro.h"
#include "../debug.h"

#include "../safeguards.h"

std::optional<std::string_view> MusicDriver_Libretro::Start(const StringList &param)
{
    playing = false;
    volume = 127;
    return std::nullopt;
}

void MusicDriver_Libretro::Stop()
{
    StopSong();
}

void MusicDriver_Libretro::PlaySong(const MusicSongInfo &song)
{
    playing = true;
}

void MusicDriver_Libretro::StopSong()
{
    playing = false;
}

bool MusicDriver_Libretro::IsSongPlaying()
{
    return playing;
}

void MusicDriver_Libretro::SetVolume(uint8_t vol)
{
    volume = vol;
}

static FMusicDriver_Libretro iFMusicDriver_Libretro;

#endif /* WITH_LIBRETRO */
