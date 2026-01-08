/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file libretro_s.cpp Libretro sound driver implementation. */

#ifdef WITH_LIBRETRO

#include "../stdafx.h"
#include "libretro_s.h"
#include "../video/libretro/libretro_core.h"
#include "../video/libretro/libretro.h"
#include "../mixer.h"
#include "../debug.h"

#include "../safeguards.h"

/* Audio constants */
static const uint32_t LIBRETRO_SAMPLE_RATE = 44100;

std::optional<std::string_view> SoundDriver_Libretro::Start(const StringList &param)
{
    if (!MxInitialize(LIBRETRO_SAMPLE_RATE)) {
        return "Failed to initialize audio mixer";
    }
    return std::nullopt;
}

void SoundDriver_Libretro::Stop()
{
    MxCloseAllChannels();
}

static FSoundDriver_Libretro iFSoundDriver_Libretro;

#endif /* WITH_LIBRETRO */
