/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file libretro_openttd.h OpenTTD integration interface for libretro core. */

#ifndef VIDEO_LIBRETRO_OPENTTD_H
#define VIDEO_LIBRETRO_OPENTTD_H

/**
 * Namespace for OpenTTD integration functions called from the libretro core.
 * These functions handle initialization, shutdown, game loading, and per-frame updates.
 */
namespace LibretroOpenTTD {

/**
 * Initialize OpenTTD for libretro operation.
 * Sets up paths, loads configuration, initializes graphics/sound/music,
 * and prepares the game for running.
 * @return true if initialization succeeded, false otherwise.
 */
bool Initialize();

/**
 * Shutdown OpenTTD cleanly.
 * Saves configuration, cleans up resources, and stops all drivers.
 */
void Shutdown();

/**
 * Load a game from the specified path.
 * @param path Path to savegame or scenario file, or nullptr for menu.
 * @return true if the game started loading, false on error.
 */
bool LoadGame(const char *path);

/**
 * Unload the current game and return to menu.
 */
void UnloadGame();

/**
 * Run a single frame of the game.
 * Processes input, runs game logic, and renders.
 */
void RunFrame();

/**
 * Process input from libretro and update OpenTTD input state.
 */
void ProcessLibretroInput();

/**
 * Process and mix audio for the current frame.
 */
void ProcessAudio();

/**
 * Check if OpenTTD has been initialized.
 * @return true if initialized.
 */
bool IsInitialized();

/**
 * Check if a game is currently loaded.
 * @return true if a game is loaded.
 */
bool IsGameLoaded();

/**
 * Get the OpenTTD version string.
 * @return Version string.
 */
const char *GetVersion();

} /* namespace LibretroOpenTTD */

#endif /* VIDEO_LIBRETRO_OPENTTD_H */
