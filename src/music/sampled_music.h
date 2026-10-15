/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file sampled_music.h Playback for sampled music tracks. */

#ifndef MUSIC_SAMPLED_MUSIC_H
#define MUSIC_SAMPLED_MUSIC_H

#include "../base_media_music.h"

bool MusicTrackUsesSampledPlayback(MusicTrackType filetype);

bool PlaySampledMusic(const MusicSongInfo &song, uint8_t volume);
void StopSampledMusic();
bool IsSampledMusicPlaying();
void SetSampledMusicVolume(uint8_t volume);

#endif /* MUSIC_SAMPLED_MUSIC_H */
