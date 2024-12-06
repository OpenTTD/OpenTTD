/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file soundloader_func.h Functions related to sound loaders. */

#ifndef SOUNDLOADER_FUNC_H
#define SOUNDLOADER_FUNC_H

bool LoadSound(SoundEntry &sound, SoundID sound_id);
bool LoadSoundData(SoundEntry &sound, bool new_format, SoundID sound_id, const std::string &name);

#endif /* SOUNDLOADER_FUNC_H */
