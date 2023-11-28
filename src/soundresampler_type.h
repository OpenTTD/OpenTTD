/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file soundresampler_type.h Types related to sound resamplers. */

#ifndef SOUNDRESAMPLER_TYPE_H
#define SOUNDRESAMPLER_TYPE_H

#include "provider_manager.h"
#include "sound_type.h"

/** Base interface for a SoundResampler implementation. */
class SoundResampler : public PriorityBaseProvider<SoundResampler> {
public:
	/** @copydoc PriorityBaseProvider::PriorityBaseProvider */
	SoundResampler(std::string_view name, std::string_view description, int priority) : PriorityBaseProvider<SoundResampler>(name, description, priority)
	{
		ProviderManager<SoundResampler>::Register(*this);
	}

	/** Unregister this sound resampler. */
	virtual ~SoundResampler()
	{
		ProviderManager<SoundResampler>::Unregister(*this);
	}

	/**
	 * Resample a loaded sound.
	 * @param sound The sound to resample.
	 * @param play_rate The new sample rate.
	 * @return \c true iff the sound was resampled.
	 */
	virtual bool Resample(SoundEntry &sound, uint32_t play_rate) const = 0;
};

#endif /* SOUNDRESAMPLER_TYPE_H */
