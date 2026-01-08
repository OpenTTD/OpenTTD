/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file libretro_openttd.cpp OpenTTD integration for libretro core - stub implementation. */

#ifdef WITH_LIBRETRO

#include "libretro_openttd.h"
#include "libretro_core.h"
#include "libretro.h"

#include <atomic>
#include <string>

/* State tracking */
static std::atomic<bool> ottd_initialized{false};
static std::atomic<bool> ottd_game_loaded{false};

namespace LibretroOpenTTD {

bool Initialize()
{
    LibretroCore::Log(RETRO_LOG_INFO, "[libretro_ottd] Initialize: Starting OpenTTD initialization\n");

    if (ottd_initialized) {
        LibretroCore::Log(RETRO_LOG_WARN, "[libretro_ottd] Initialize: Already initialized\n");
        return true;
    }

    /* For now, just mark as initialized - full OpenTTD init will be added later */
    ottd_initialized = true;

    LibretroCore::Log(RETRO_LOG_INFO, "[libretro_ottd] Initialize: OpenTTD initialization complete (stub)\n");
    return true;
}

void Shutdown()
{
    LibretroCore::Log(RETRO_LOG_INFO, "[libretro_ottd] Shutdown: Shutting down OpenTTD\n");

    if (!ottd_initialized) {
        LibretroCore::Log(RETRO_LOG_WARN, "[libretro_ottd] Shutdown: Not initialized\n");
        return;
    }

    ottd_initialized = false;
    ottd_game_loaded = false;

    LibretroCore::Log(RETRO_LOG_INFO, "[libretro_ottd] Shutdown: OpenTTD shutdown complete\n");
}

bool LoadGame(const char *path)
{
    LibretroCore::Log(RETRO_LOG_INFO, "[libretro_ottd] LoadGame: %s\n", path ? path : "(contentless)");

    if (!ottd_initialized) {
        LibretroCore::Log(RETRO_LOG_ERROR, "[libretro_ottd] LoadGame: OpenTTD not initialized\n");
        return false;
    }

    ottd_game_loaded = true;
    return true;
}

void UnloadGame()
{
    LibretroCore::Log(RETRO_LOG_INFO, "[libretro_ottd] UnloadGame: Unloading current game\n");
    ottd_game_loaded = false;
}

void RunFrame()
{
    if (!ottd_initialized) {
        return;
    }

    /* Stub - will call OpenTTD game loop later */
}

void ProcessLibretroInput()
{
    /* Stub - will process input later */
}

void ProcessAudio()
{
    /* Stub - will mix audio later */
}

bool IsInitialized()
{
    return ottd_initialized;
}

bool IsGameLoaded()
{
    return ottd_game_loaded;
}

const char *GetVersion()
{
    return "14.0-libretro";
}

} /* namespace LibretroOpenTTD */

#endif /* WITH_LIBRETRO */
