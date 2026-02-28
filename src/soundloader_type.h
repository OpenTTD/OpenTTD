/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file soundloader_type.h Types related to sound loaders. */

#ifndef SOUNDLOADER_TYPE_H
#define SOUNDLOADER_TYPE_H

#include "provider_manager.h"
#include "sound_type.h"

/** Base interface for a SoundLoader implementation. */
class SoundLoader : public PriorityBaseProvider<SoundLoader> {
public:
	SoundLoader(std::string_view name, std::string_view description, int priority) : PriorityBaseProvider<SoundLoader>(name, description, priority)
	{
		ProviderManager<SoundLoader>::Register(*this);
	}

	/** Unregister this sound loader. */
	~SoundLoader() override
	{
		ProviderManager<SoundLoader>::Unregister(*this);
	}

	/**
	 * Load a sound from the file and offset in the given sound entry.
	 * It is up to the implementations to update the sound's channels, bits_per_sample and rate.
	 * @param sound The entry to load.
	 * @param new_format Whether this is an old format soundset (with some buggy data), or the new format.
	 * @param[out] data The vector to write the decoded sound data into.
	 * @return \c true iff the entry was loaded correctly.
	 */
	virtual bool Load(SoundEntry &sound, bool new_format, std::vector<std::byte> &data) const = 0;
};

#endif /* SOUNDLOADER_TYPE_H */
