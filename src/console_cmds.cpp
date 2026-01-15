/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file console_cmds.cpp Implementation of the console hooks. */

#include "stdafx.h"
#include "core/string_consumer.hpp"
#include "console_internal.h"
#include "debug.h"
#include "engine_func.h"
#include "landscape.h"
#include "saveload/saveload.h"
#include "network/core/network_game_info.h"
#include "network/network.h"
#include "network/network_func.h"
#include "network/network_base.h"
#include "network/network_admin.h"
#include "network/network_client.h"
#include "command_func.h"
#include "settings_func.h"
#include "fios.h"
#include "fileio_func.h"
#include "fontcache.h"
#include "screenshot.h"
#include "genworld.h"
#include "strings_func.h"
#include "viewport_func.h"
#include "window_func.h"
#include "timer/timer.h"
#include "company_func.h"
#include "gamelog.h"
#include "ai/ai.hpp"
#include "ai/ai_config.hpp"
#include "newgrf.h"
#include "newgrf_profiling.h"
#include "console_func.h"
#include "engine_base.h"
#include "road.h"
#include "rail.h"
#include "game/game.hpp"
#include "3rdparty/fmt/chrono.h"
#include "company_cmd.h"
#include "misc_cmd.h"

#if defined(WITH_ZLIB)
#include "network/network_content.h"
#endif /* WITH_ZLIB */

#include "table/strings.h"

#include "safeguards.h"

/* scriptfile handling */
static uint _script_current_depth; ///< Depth of scripts running (used to abort execution when #ConReturn is encountered).

/* Scheduled execution handling. */
static std::string _scheduled_monthly_script; ///< Script scheduled to execute by the 'schedule' console command (empty if no script is scheduled).

/** Timer that runs every month of game time for the 'schedule' console command. */
static const IntervalTimer<TimerGameCalendar> _scheduled_monthly_timer = {{TimerGameCalendar::MONTH, TimerGameCalendar::Priority::NONE}, [](auto) {
	if (_scheduled_monthly_script.empty()) {
		return;
	}

	/* Clear the schedule before rather than after the script to allow the script to itself call
	 * schedule without it getting immediately cleared. */
	const std::string filename = _scheduled_monthly_script;
	_scheduled_monthly_script.clear();

	IConsolePrint(CC_DEFAULT, "Executing scheduled script file '{}'...", filename);
	IConsoleCmdExec(fmt::format("exec {}", filename));
}};

/**
 * Parse an integer using #ParseInteger and convert it to the requested type.
 * @param arg The string to be converted.
 * @tparam T The type to return.
 * @return The number in the given type, or std::nullopt when it could not be parsed.
 */
template <typename T>
static std::optional<T> ParseType(std::string_view arg)
{
	auto i = ParseInteger(arg);
	if (i.has_value()) return static_cast<T>(*i);
	return std::nullopt;
}

/** File list storage for the console, for caching the last 'ls' command. */
class ConsoleFileList : public FileList {
public:
	ConsoleFileList(AbstractFileType abstract_filetype, bool show_dirs) : FileList(), abstract_filetype(abstract_filetype), show_dirs(show_dirs)
	{
	}

	/** Declare the file storage cache as being invalid, also clears all stored files. */
	void InvalidateFileList()
	{
		this->clear();
		this->file_list_valid = false;
	}

	/**
	 * (Re-)validate the file storage cache. Only makes a change if the storage was invalid, or if \a force_reload.
	 * @param force_reload Always reload the file storage cache.
	 */
	void ValidateFileList(bool force_reload = false)
	{
		if (force_reload || !this->file_list_valid) {
			this->BuildFileList(this->abstract_filetype, SLO_LOAD, this->show_dirs);
			this->file_list_valid = true;
		}
	}

	AbstractFileType abstract_filetype; ///< The abstract file type to list.
	bool show_dirs; ///< Whether to show directories in the file list.
	bool file_list_valid = false; ///< If set, the file list is valid.
};

static ConsoleFileList _console_file_list_savegame{FT_SAVEGAME, true}; ///< File storage cache for savegames.
static ConsoleFileList _console_file_list_scenario{FT_SCENARIO, false}; ///< File storage cache for scenarios.
static ConsoleFileList _console_file_list_heightmap{FT_HEIGHTMAP, false}; ///< File storage cache for heightmaps.

/****************
 * command hooks
 ****************/

/**
 * Check network availability and inform in console about failure of detection.
 * @return Network availability.
 */
static inline bool NetworkAvailable(bool echo)
{
	if (!_network_available) {
		if (echo) IConsolePrint(CC_ERROR, "You cannot use this command because there is no network available.");
		return false;
	}
	return true;
}

/**
 * Check whether we are a server.
 * @return Are we a server? True when yes, false otherwise.
 */
static ConsoleHookResult ConHookServerOnly(bool echo)
{
	if (!NetworkAvailable(echo)) return CHR_DISALLOW;

	if (!_network_server) {
		if (echo) IConsolePrint(CC_ERROR, "This command is only available to a network server.");
		return CHR_DISALLOW;
	}
	return CHR_ALLOW;
}

/**
 * Check whether we are a client in a network game.
 * @return Are we a client in a network game? True when yes, false otherwise.
 */
static ConsoleHookResult ConHookClientOnly(bool echo)
{
	if (!NetworkAvailable(echo)) return CHR_DISALLOW;

	if (_network_server) {
		if (echo) IConsolePrint(CC_ERROR, "This command is not available to a network server.");
		return CHR_DISALLOW;
	}
	return CHR_ALLOW;
}

/**
 * Check whether we are in a multiplayer game.
 * @return True when we are client or server in a network game.
 */
static ConsoleHookResult ConHookNeedNetwork(bool echo)
{
	if (!NetworkAvailable(echo)) return CHR_DISALLOW;

	if (!_networking || (!_network_server && !MyClient::IsConnected())) {
		if (echo) IConsolePrint(CC_ERROR, "Not connected. This command is only available in multiplayer.");
		return CHR_DISALLOW;
	}
	return CHR_ALLOW;
}

/**
 * Check whether we are in a multiplayer game and are playing, i.e. we are not the dedicated server.
 * @return Are we a client or non-dedicated server in a network game? True when yes, false otherwise.
 */
static ConsoleHookResult ConHookNeedNonDedicatedNetwork(bool echo)
{
	if (!NetworkAvailable(echo)) return CHR_DISALLOW;

	if (_network_dedicated) {
		if (echo) IConsolePrint(CC_ERROR, "This command is not available to a dedicated network server.");
		return CHR_DISALLOW;
	}
	return CHR_ALLOW;
}

/**
 * Check whether we are in singleplayer mode.
 * @return True when no network is active.
 */
static ConsoleHookResult ConHookNoNetwork(bool echo)
{
	if (_networking) {
		if (echo) IConsolePrint(CC_ERROR, "This command is forbidden in multiplayer.");
		return CHR_DISALLOW;
	}
	return CHR_ALLOW;
}

/**
 * Check if are either in singleplayer or a server.
 * @return True iff we are either in singleplayer or a server.
 */
static ConsoleHookResult ConHookServerOrNoNetwork(bool echo)
{
	if (_networking && !_network_server) {
		if (echo) IConsolePrint(CC_ERROR, "This command is only available to a network server.");
		return CHR_DISALLOW;
	}
	return CHR_ALLOW;
}

static ConsoleHookResult ConHookNewGRFDeveloperTool(bool echo)
{
	if (_settings_client.gui.newgrf_developer_tools) {
		if (_game_mode == GM_MENU) {
			if (echo) IConsolePrint(CC_ERROR, "This command is only available in-game and in the editor.");
			return CHR_DISALLOW;
		}
		return ConHookNoNetwork(echo);
	}
	return CHR_HIDE;
}

/**
 * Reset status of all engines.
 * @return Will always succeed.
 */
static bool ConResetEngines(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "Reset status data of all engines. This might solve some issues with 'lost' engines. Usage: 'resetengines'.");
		return true;
	}

	StartupEngines();
	return true;
}

/**
 * Reset status of the engine pool.
 * @return Will always return true.
 * @note Resetting the pool only succeeds when there are no vehicles ingame.
 */
static bool ConResetEnginePool(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "Reset NewGRF allocations of engine slots. This will remove invalid engine definitions, and might make default engines available again.");
		return true;
	}

	if (_game_mode == GM_MENU) {
		IConsolePrint(CC_ERROR, "This command is only available in-game and in the editor.");
		return true;
	}

	if (!EngineOverrideManager::ResetToCurrentNewGRFConfig()) {
		IConsolePrint(CC_ERROR, "This can only be done when there are no vehicles in the game.");
		return true;
	}

	return true;
}

#ifdef _DEBUG
/**
 * Reset a tile to bare land in debug mode.
 * param tile number.
 * @return True when the tile is reset or the help on usage was printed (0 or two parameters).
 */
static bool ConResetTile(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "Reset a tile to bare land. Usage: 'resettile <tile>'.");
		IConsolePrint(CC_HELP, "Tile can be either decimal (34161) or hexadecimal (0x4a5B).");
		return true;
	}

	if (argv.size() == 2) {
		auto result = ParseInteger(argv[1], 0);
		if (result.has_value() && IsValidTile(*result)) {
			DoClearSquare(TileIndex{*result});
			return true;
		}
	}

	return false;
}
#endif /* _DEBUG */

/**
 * Zoom map to given level.
 * param level As defined by ZoomLevel and as limited by zoom_min/zoom_max from GUISettings.
 * @return True when either console help was shown or a proper amount of parameters given.
 */
static bool ConZoomToLevel(std::span<std::string_view> argv)
{
	switch (argv.size()) {
		case 0:
			IConsolePrint(CC_HELP, "Set the current zoom level of the main viewport.");
			IConsolePrint(CC_HELP, "Usage: 'zoomto <level>'.");

			if (ZoomLevel::Min < _settings_client.gui.zoom_min) {
				IConsolePrint(CC_HELP, "The lowest zoom-in level allowed by current client settings is {}.", std::max(ZoomLevel::Min, _settings_client.gui.zoom_min));
			} else {
				IConsolePrint(CC_HELP, "The lowest supported zoom-in level is {}.", std::max(ZoomLevel::Min, _settings_client.gui.zoom_min));
			}

			if (_settings_client.gui.zoom_max < ZoomLevel::Max) {
				IConsolePrint(CC_HELP, "The highest zoom-out level allowed by current client settings is {}.", std::min(_settings_client.gui.zoom_max, ZoomLevel::Max));
			} else {
				IConsolePrint(CC_HELP, "The highest supported zoom-out level is {}.", std::min(_settings_client.gui.zoom_max, ZoomLevel::Max));
			}
			return true;

		case 2: {
			auto level = ParseInteger<std::underlying_type_t<ZoomLevel>>(argv[1]);
			if (level.has_value()) {
				auto zoom_lvl = static_cast<ZoomLevel>(*level);
				if (!IsInsideMM(zoom_lvl, ZoomLevel::Begin, ZoomLevel::End)) {
					IConsolePrint(CC_ERROR, "Invalid zoom level. Valid range is {} to {}.", ZoomLevel::Min, ZoomLevel::Max);
				} else if (!IsInsideMM(zoom_lvl, _settings_client.gui.zoom_min, _settings_client.gui.zoom_max + 1)) {
					IConsolePrint(CC_ERROR, "Current client settings limit zoom levels to range {} to {}.", _settings_client.gui.zoom_min, _settings_client.gui.zoom_max);
				} else {
					Window *w = GetMainWindow();
					Viewport &vp = *w->viewport;
					while (vp.zoom > zoom_lvl) DoZoomInOutWindow(ZOOM_IN, w);
					while (vp.zoom < zoom_lvl) DoZoomInOutWindow(ZOOM_OUT, w);
				}
				return true;
			}
			break;
		}
	}

	return false;
}

/**
 * Scroll to a tile on the map.
 * param x tile number or tile x coordinate.
 * param y optional y coordinate.
 * @note When only one argument is given it is interpreted as the tile number.
 *       When two arguments are given, they are interpreted as the tile's x
 *       and y coordinates.
 * @return True when either console help was shown or a proper amount of parameters given.
 */
static bool ConScrollToTile(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "Center the screen on a given tile.");
		IConsolePrint(CC_HELP, "Usage: 'scrollto [instant] <tile>' or 'scrollto [instant] <x> <y>'.");
		IConsolePrint(CC_HELP, "Numbers can be either decimal (34161) or hexadecimal (0x4a5B).");
		IConsolePrint(CC_HELP, "'instant' will immediately move and redraw viewport without smooth scrolling.");
		return true;
	}
	if (argv.size() < 2) return false;

	uint32_t arg_index = 1;
	bool instant = false;
	if (argv[arg_index] == "instant") {
		++arg_index;
		instant = true;
	}

	switch (argv.size() - arg_index) {
		case 1: {
			auto result = ParseInteger(argv[arg_index], 0);
			if (result.has_value()) {
				if (*result >= Map::Size()) {
					IConsolePrint(CC_ERROR, "Tile does not exist.");
					return true;
				}
				ScrollMainWindowToTile(TileIndex{*result}, instant);
				return true;
			}
			break;
		}

		case 2: {
			auto x = ParseInteger(argv[arg_index], 0);
			auto y = ParseInteger(argv[arg_index + 1], 0);
			if (x.has_value() && y.has_value()) {
				if (*x >= Map::SizeX() || *y >= Map::SizeY()) {
					IConsolePrint(CC_ERROR, "Tile does not exist.");
					return true;
				}
				ScrollMainWindowToTile(TileXY(*x, *y), instant);
				return true;
			}
			break;
		}
	}

	return false;
}

/**
 * Save the map to a file.
 * param filename the filename to save the map to.
 * @return True when help was displayed or the file attempted to be saved.
 */
static bool ConSave(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "Save the current game. Usage: 'save <filename>'.");
		return true;
	}

	if (argv.size() == 2) {
		std::string filename = fmt::format("{}.sav", argv[1]);
		IConsolePrint(CC_DEFAULT, "Saving map...");

		if (SaveOrLoad(filename, SLO_SAVE, DFT_GAME_FILE, SAVE_DIR) != SL_OK) {
			IConsolePrint(CC_ERROR, "Saving map failed.");
		} else {
			IConsolePrint(CC_INFO, "Map successfully saved to '{}'.", filename);
		}
		return true;
	}

	return false;
}

/**
 * Explicitly save the configuration.
 * @return True.
 */
static bool ConSaveConfig(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "Saves the configuration for new games to the configuration file, typically 'openttd.cfg'.");
		IConsolePrint(CC_HELP, "It does not save the configuration of the current game to the configuration file.");
		return true;
	}

	SaveToConfig();
	IConsolePrint(CC_DEFAULT, "Saved config.");
	return true;
}

static bool ConLoad(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "Load a game by name or index. Usage: 'load <file | number>'.");
		return true;
	}

	if (argv.size() != 2) return false;

	std::string_view file = argv[1];
	_console_file_list_savegame.ValidateFileList();
	const FiosItem *item = _console_file_list_savegame.FindItem(file);
	if (item != nullptr) {
		if (item->type.abstract == FT_SAVEGAME) {
			_switch_mode = SM_LOAD_GAME;
			_file_to_saveload.Set(*item);
		} else {
			IConsolePrint(CC_ERROR, "'{}' is not a savegame.", file);
		}
	} else {
		IConsolePrint(CC_ERROR, "'{}' cannot be found.", file);
	}

	return true;
}

static bool ConLoadScenario(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "Load a scenario by name or index. Usage: 'load_scenario <file | number>'.");
		return true;
	}

	if (argv.size() != 2) return false;

	std::string_view file = argv[1];
	_console_file_list_scenario.ValidateFileList();
	const FiosItem *item = _console_file_list_scenario.FindItem(file);
	if (item != nullptr) {
		if (item->type.abstract == FT_SCENARIO) {
			_switch_mode = SM_LOAD_GAME;
			_file_to_saveload.Set(*item);
		} else {
			IConsolePrint(CC_ERROR, "'{}' is not a scenario.", file);
		}
	} else {
		IConsolePrint(CC_ERROR, "'{}' cannot be found.", file);
	}

	return true;
}

static bool ConLoadHeightmap(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "Load a heightmap by name or index. Usage: 'load_heightmap <file | number>'.");
		return true;
	}

	if (argv.size() != 2) return false;

	std::string_view file = argv[1];
	_console_file_list_heightmap.ValidateFileList();
	const FiosItem *item = _console_file_list_heightmap.FindItem(file);
	if (item != nullptr) {
		if (item->type.abstract == FT_HEIGHTMAP) {
			_switch_mode = SM_START_HEIGHTMAP;
			_file_to_saveload.Set(*item);
		} else {
			IConsolePrint(CC_ERROR, "'{}' is not a heightmap.", file);
		}
	} else {
		IConsolePrint(CC_ERROR, "'{}' cannot be found.", file);
	}

	return true;
}

static bool ConRemove(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "Remove a savegame by name or index. Usage: 'rm <file | number>'.");
		return true;
	}

	if (argv.size() != 2) return false;

	std::string_view file = argv[1];
	_console_file_list_savegame.ValidateFileList();
	const FiosItem *item = _console_file_list_savegame.FindItem(file);
	if (item != nullptr) {
		if (item->type.abstract == FT_SAVEGAME) {
			if (!FioRemove(item->name)) {
				IConsolePrint(CC_ERROR, "Failed to delete '{}'.", item->name);
			}
		} else {
			IConsolePrint(CC_ERROR, "'{}' is not a savegame.", file);
		}
	} else {
		IConsolePrint(CC_ERROR, "'{}' could not be found.", file);
	}

	_console_file_list_savegame.InvalidateFileList();
	return true;
}


/* List all the files in the current dir via console */
static bool ConListFiles(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "List all loadable savegames and directories in the current dir via console. Usage: 'ls | dir'.");
		return true;
	}

	_console_file_list_savegame.ValidateFileList(true);
	for (uint i = 0; i < _console_file_list_savegame.size(); i++) {
		IConsolePrint(CC_DEFAULT, "{}) {}", i, _console_file_list_savegame[i].title.GetDecodedString());
	}

	return true;
}

/* List all the scenarios */
static bool ConListScenarios(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "List all loadable scenarios. Usage: 'list_scenarios'.");
		return true;
	}

	_console_file_list_scenario.ValidateFileList(true);
	for (uint i = 0; i < _console_file_list_scenario.size(); i++) {
		IConsolePrint(CC_DEFAULT, "{}) {}", i, _console_file_list_scenario[i].title.GetDecodedString());
	}

	return true;
}

/* List all the heightmaps */
static bool ConListHeightmaps(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "List all loadable heightmaps. Usage: 'list_heightmaps'.");
		return true;
	}

	_console_file_list_heightmap.ValidateFileList(true);
	for (uint i = 0; i < _console_file_list_heightmap.size(); i++) {
		IConsolePrint(CC_DEFAULT, "{}) {}", i, _console_file_list_heightmap[i].title.GetDecodedString());
	}

	return true;
}

/* Change the dir via console */
static bool ConChangeDirectory(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "Change the dir via console. Usage: 'cd <directory | number>'.");
		return true;
	}

	if (argv.size() != 2) return false;

	std::string_view file = argv[1];
	_console_file_list_savegame.ValidateFileList(true);
	const FiosItem *item = _console_file_list_savegame.FindItem(file);
	if (item != nullptr) {
		switch (item->type.detailed) {
			case DFT_FIOS_DIR:
			case DFT_FIOS_DRIVE:
			case DFT_FIOS_PARENT:
				FiosBrowseTo(item);
				break;
			default: IConsolePrint(CC_ERROR, "{}: Not a directory.", file);
		}
	} else {
		IConsolePrint(CC_ERROR, "{}: No such file or directory.", file);
	}

	_console_file_list_savegame.InvalidateFileList();
	return true;
}

static bool ConPrintWorkingDirectory(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "Print out the current working directory. Usage: 'pwd'.");
		return true;
	}

	/* XXX - Workaround for broken file handling */
	_console_file_list_savegame.ValidateFileList(true);
	_console_file_list_savegame.InvalidateFileList();

	IConsolePrint(CC_DEFAULT, FiosGetCurrentPath());
	return true;
}

static bool ConClearBuffer(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "Clear the console buffer. Usage: 'clear'.");
		return true;
	}

	IConsoleClearBuffer();
	SetWindowDirty(WC_CONSOLE, 0);
	return true;
}


/**********************************
 * Network Core Console Commands
 **********************************/

static bool ConKickOrBan(std::string_view arg, bool ban, std::string_view reason)
{
	uint n;

	if (arg.find_first_of(".:") == std::string::npos) { // banning with ID
		auto client_id = ParseType<ClientID>(arg);
		if (!client_id.has_value()) {
			IConsolePrint(CC_ERROR, "The given client-id is not a valid number.");
			return true;
		}

		/* Don't kill the server, or the client doing the rcon. The latter can't be kicked because
		 * kicking frees closes and subsequently free the connection related instances, which we
		 * would be reading from and writing to after returning. So we would read or write data
		 * from freed memory up till the segfault triggers. */
		if (*client_id == CLIENT_ID_SERVER || *client_id == _redirect_console_to_client) {
			IConsolePrint(CC_ERROR, "You can not {} yourself!", ban ? "ban" : "kick");
			return true;
		}

		NetworkClientInfo *ci = NetworkClientInfo::GetByClientID(*client_id);
		if (ci == nullptr) {
			IConsolePrint(CC_ERROR, "Invalid client-id.");
			return true;
		}

		if (!ban) {
			/* Kick only this client, not all clients with that IP */
			NetworkServerKickClient(*client_id, reason);
			return true;
		}

		/* When banning, kick+ban all clients with that IP */
		n = NetworkServerKickOrBanIP(*client_id, ban, reason);
	} else {
		n = NetworkServerKickOrBanIP(arg, ban, reason);
	}

	if (n == 0) {
		IConsolePrint(CC_DEFAULT, ban ? "Client not online, address added to banlist." : "Client not found.");
	} else {
		IConsolePrint(CC_DEFAULT, "{}ed {} client(s).", ban ? "Bann" : "Kick", n);
	}

	return true;
}

static bool ConKick(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "Kick a client from a network game. Usage: 'kick <ip | client-id> [<kick-reason>]'.");
		IConsolePrint(CC_HELP, "For client-id's, see the command 'clients'.");
		return true;
	}

	if (argv.size() != 2 && argv.size() != 3) return false;

	/* No reason supplied for kicking */
	if (argv.size() == 2) return ConKickOrBan(argv[1], false, {});

	/* Reason for kicking supplied */
	size_t kick_message_length = argv[2].size();
	if (kick_message_length >= 255) {
		IConsolePrint(CC_ERROR, "Maximum kick message length is 254 characters. You entered {} characters.", kick_message_length);
		return false;
	} else {
		return ConKickOrBan(argv[1], false, argv[2]);
	}
}

static bool ConBan(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "Ban a client from a network game. Usage: 'ban <ip | client-id> [<ban-reason>]'.");
		IConsolePrint(CC_HELP, "For client-id's, see the command 'clients'.");
		IConsolePrint(CC_HELP, "If the client is no longer online, you can still ban their IP.");
		return true;
	}

	if (argv.size() != 2 && argv.size() != 3) return false;

	/* No reason supplied for kicking */
	if (argv.size() == 2) return ConKickOrBan(argv[1], true, {});

	/* Reason for kicking supplied */
	size_t kick_message_length = argv[2].size();
	if (kick_message_length >= 255) {
		IConsolePrint(CC_ERROR, "Maximum kick message length is 254 characters. You entered {} characters.", kick_message_length);
		return false;
	} else {
		return ConKickOrBan(argv[1], true, argv[2]);
	}
}

static bool ConUnBan(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "Unban a client from a network game. Usage: 'unban <ip | banlist-index>'.");
		IConsolePrint(CC_HELP, "For a list of banned IP's, see the command 'banlist'.");
		return true;
	}

	if (argv.size() != 2) return false;

	/* Try by IP. */
	uint index;
	for (index = 0; index < _network_ban_list.size(); index++) {
		if (_network_ban_list[index] == argv[1]) break;
	}

	/* Try by index. */
	if (index >= _network_ban_list.size()) {
		index = ParseInteger(argv[1]).value_or(0) - 1U; // let it wrap
	}

	if (index < _network_ban_list.size()) {
		IConsolePrint(CC_DEFAULT, "Unbanned {}.", _network_ban_list[index]);
		_network_ban_list.erase(_network_ban_list.begin() + index);
	} else {
		IConsolePrint(CC_DEFAULT, "Invalid list index or IP not in ban-list.");
		IConsolePrint(CC_DEFAULT, "For a list of banned IP's, see the command 'banlist'.");
	}

	return true;
}

static bool ConBanList(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "List the IP's of banned clients: Usage 'banlist'.");
		return true;
	}

	IConsolePrint(CC_DEFAULT, "Banlist:");

	uint i = 1;
	for (const auto &entry : _network_ban_list) {
		IConsolePrint(CC_DEFAULT, "  {}) {}", i, entry);
		i++;
	}

	return true;
}

static bool ConPauseGame(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "Pause a network game. Usage: 'pause'.");
		return true;
	}

	if (_game_mode == GM_MENU) {
		IConsolePrint(CC_ERROR, "This command is only available in-game and in the editor.");
		return true;
	}

	if (!_pause_mode.Test(PauseMode::Normal)) {
		Command<Commands::Pause>::Post(PauseMode::Normal, true);
		if (!_networking) IConsolePrint(CC_DEFAULT, "Game paused.");
	} else {
		IConsolePrint(CC_DEFAULT, "Game is already paused.");
	}

	return true;
}

static bool ConUnpauseGame(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "Unpause a network game. Usage: 'unpause'.");
		return true;
	}

	if (_game_mode == GM_MENU) {
		IConsolePrint(CC_ERROR, "This command is only available in-game and in the editor.");
		return true;
	}

	if (_pause_mode.Test(PauseMode::Normal)) {
		Command<Commands::Pause>::Post(PauseMode::Normal, false);
		if (!_networking) IConsolePrint(CC_DEFAULT, "Game unpaused.");
	} else if (_pause_mode.Test(PauseMode::Error)) {
		IConsolePrint(CC_DEFAULT, "Game is in error state and cannot be unpaused via console.");
	} else if (_pause_mode.Any()) {
		IConsolePrint(CC_DEFAULT, "Game cannot be unpaused manually; disable pause_on_join/min_active_clients.");
	} else {
		IConsolePrint(CC_DEFAULT, "Game is already unpaused.");
	}

	return true;
}

static bool ConRcon(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "Remote control the server from another client. Usage: 'rcon <password> <command>'.");
		IConsolePrint(CC_HELP, "Remember to enclose the command in quotes, otherwise only the first parameter is sent.");
		IConsolePrint(CC_HELP, "When your client's public key is in the 'authorized keys' for 'rcon', the password is not checked and may be '*'.");
		return true;
	}

	if (argv.size() < 3) return false;

	if (_network_server) {
		IConsoleCmdExec(argv[2]);
	} else {
		NetworkClientSendRcon(argv[1], argv[2]);
	}
	return true;
}

static bool ConStatus(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "List the status of all clients connected to the server. Usage 'status'.");
		return true;
	}

	NetworkServerShowStatusToConsole();
	return true;
}

static bool ConServerInfo(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "List current and maximum client/company limits. Usage 'server_info'.");
		IConsolePrint(CC_HELP, "You can change these values by modifying settings 'network.max_clients' and 'network.max_companies'.");
		return true;
	}

	IConsolePrint(CC_DEFAULT, "Invite code:                {}", _network_server_invite_code);
	IConsolePrint(CC_DEFAULT, "Current/maximum clients:    {:3d}/{:3d}", _network_game_info.clients_on, _settings_client.network.max_clients);
	IConsolePrint(CC_DEFAULT, "Current/maximum companies:  {:3d}/{:3d}", Company::GetNumItems(), _settings_client.network.max_companies);
	IConsolePrint(CC_DEFAULT, "Current spectators:         {:3d}", NetworkSpectatorCount());

	return true;
}

static bool ConClientNickChange(std::span<std::string_view> argv)
{
	if (argv.size() != 3) {
		IConsolePrint(CC_HELP, "Change the nickname of a connected client. Usage: 'client_name <client-id> <new-name>'.");
		IConsolePrint(CC_HELP, "For client-id's, see the command 'clients'.");
		return true;
	}

	auto client_id = ParseType<ClientID>(argv[1]);
	if (!client_id.has_value()) {
		IConsolePrint(CC_ERROR, "The given client-id is not a valid number.");
		return true;
	}

	if (*client_id == CLIENT_ID_SERVER) {
		IConsolePrint(CC_ERROR, "Please use the command 'name' to change your own name!");
		return true;
	}

	if (NetworkClientInfo::GetByClientID(*client_id) == nullptr) {
		IConsolePrint(CC_ERROR, "Invalid client-id.");
		return true;
	}

	std::string client_name{StrTrimView(argv[2], StringConsumer::WHITESPACE_NO_NEWLINE)};
	if (!NetworkIsValidClientName(client_name)) {
		IConsolePrint(CC_ERROR, "Cannot give a client an empty name.");
		return true;
	}

	if (!NetworkServerChangeClientName(*client_id, client_name)) {
		IConsolePrint(CC_ERROR, "Cannot give a client a duplicate name.");
	}

	return true;
}

static std::optional<CompanyID> ParseCompanyID(std::string_view arg)
{
	auto company_id = ParseType<CompanyID>(arg);
	if (company_id.has_value() && *company_id <= MAX_COMPANIES) return static_cast<CompanyID>(*company_id - 1);
	return company_id;
}

static bool ConJoinCompany(std::span<std::string_view> argv)
{
	if (argv.size() < 2) {
		IConsolePrint(CC_HELP, "Request joining another company. Usage: 'join <company-id>'.");
		IConsolePrint(CC_HELP, "For valid company-id see company list, use 255 for spectator.");
		return true;
	}

	auto company_id = ParseCompanyID(argv[1]);
	if (!company_id.has_value()) {
		IConsolePrint(CC_ERROR, "The given company-id is not a valid number.");
		return true;
	}

	const NetworkClientInfo *info = NetworkClientInfo::GetByClientID(_network_own_client_id);
	if (info == nullptr) {
		IConsolePrint(CC_ERROR, "You have not joined the game yet!");
		return true;
	}

	/* Check we have a valid company id! */
	if (!Company::IsValidID(*company_id) && *company_id != COMPANY_SPECTATOR) {
		IConsolePrint(CC_ERROR, "Company does not exist. Company-id must be between 1 and {}.", MAX_COMPANIES);
		return true;
	}

	if (info->client_playas == *company_id) {
		IConsolePrint(CC_ERROR, "You are already there!");
		return true;
	}

	if (*company_id != COMPANY_SPECTATOR && !Company::IsHumanID(*company_id)) {
		IConsolePrint(CC_ERROR, "Cannot join AI company.");
		return true;
	}

	if (!info->CanJoinCompany(*company_id)) {
		IConsolePrint(CC_ERROR, "You are not allowed to join this company.");
		return true;
	}

	/* non-dedicated server may just do the move! */
	if (_network_server) {
		NetworkServerDoMove(CLIENT_ID_SERVER, *company_id);
	} else {
		NetworkClientRequestMove(*company_id);
	}

	return true;
}

static bool ConMoveClient(std::span<std::string_view> argv)
{
	if (argv.size() < 3) {
		IConsolePrint(CC_HELP, "Move a client to another company. Usage: 'move <client-id> <company-id>'.");
		IConsolePrint(CC_HELP, "For valid client-id see 'clients', for valid company-id see 'companies', use 255 for moving to spectators.");
		return true;
	}

	auto client_id = ParseType<ClientID>(argv[1]);
	if (!client_id.has_value()) {
		IConsolePrint(CC_ERROR, "The given client-id is not a valid number.");
		return true;
	}
	const NetworkClientInfo *ci = NetworkClientInfo::GetByClientID(*client_id);

	auto company_id = ParseCompanyID(argv[2]);
	if (!company_id.has_value()) {
		IConsolePrint(CC_ERROR, "The given company-id is not a valid number.");
		return true;
	}

	/* check the client exists */
	if (ci == nullptr) {
		IConsolePrint(CC_ERROR, "Invalid client-id, check the command 'clients' for valid client-id's.");
		return true;
	}

	if (!Company::IsValidID(*company_id) && *company_id != COMPANY_SPECTATOR) {
		IConsolePrint(CC_ERROR, "Company does not exist. Company-id must be between 1 and {}.", MAX_COMPANIES);
		return true;
	}

	if (*company_id != COMPANY_SPECTATOR && !Company::IsHumanID(*company_id)) {
		IConsolePrint(CC_ERROR, "You cannot move clients to AI companies.");
		return true;
	}

	if (ci->client_id == CLIENT_ID_SERVER && _network_dedicated) {
		IConsolePrint(CC_ERROR, "You cannot move the server!");
		return true;
	}

	if (ci->client_playas == *company_id) {
		IConsolePrint(CC_ERROR, "You cannot move someone to where they already are!");
		return true;
	}

	/* we are the server, so force the update */
	NetworkServerDoMove(ci->client_id, *company_id);

	return true;
}

static bool ConResetCompany(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "Remove an idle company from the game. Usage: 'reset_company <company-id>'.");
		IConsolePrint(CC_HELP, "For company-id's, see the list of companies from the dropdown menu. Company 1 is 1, etc.");
		return true;
	}

	if (argv.size() != 2) return false;

	auto index = ParseCompanyID(argv[1]);
	if (!index.has_value()) {
		IConsolePrint(CC_ERROR, "The given company-id is not a valid number.");
		return true;
	}

	/* Check valid range */
	if (!Company::IsValidID(*index)) {
		IConsolePrint(CC_ERROR, "Company does not exist. company-id must be between 1 and {}.", MAX_COMPANIES);
		return true;
	}

	if (!Company::IsHumanID(*index)) {
		IConsolePrint(CC_ERROR, "Company is owned by an AI.");
		return true;
	}

	if (NetworkCompanyHasClients(*index)) {
		IConsolePrint(CC_ERROR, "Cannot remove company: a client is connected to that company.");
		return false;
	}
	const NetworkClientInfo *ci = NetworkClientInfo::GetByClientID(CLIENT_ID_SERVER);
	assert(ci != nullptr);
	if (ci->client_playas == *index) {
		IConsolePrint(CC_ERROR, "Cannot remove company: the server is connected to that company.");
		return true;
	}

	/* It is safe to remove this company */
	Command<Commands::CompanyControl>::Post(CCA_DELETE, *index, CRR_MANUAL, INVALID_CLIENT_ID);
	IConsolePrint(CC_DEFAULT, "Company deleted.");

	return true;
}

static bool ConNetworkClients(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "Get a list of connected clients including their ID, name, company-id, and IP. Usage: 'clients'.");
		return true;
	}

	NetworkPrintClients();

	return true;
}

static bool ConNetworkReconnect(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "Reconnect to server to which you were connected last time. Usage: 'reconnect [<company-id>]'.");
		IConsolePrint(CC_HELP, "Company 255 is spectator (default, if not specified), 254 means creating new company.");
		IConsolePrint(CC_HELP, "All others are a certain company with Company 1 being #1.");
		return true;
	}

	CompanyID playas = COMPANY_SPECTATOR;
	if (argv.size() >= 2) {
		auto company_id = ParseCompanyID(argv[1]);
		if (!company_id.has_value()) {
			IConsolePrint(CC_ERROR, "The given company-id is not a valid number.");
			return true;
		}
		if (*company_id >= MAX_COMPANIES && *company_id != COMPANY_NEW_COMPANY && *company_id != COMPANY_SPECTATOR) return false;
		playas = *company_id;
	}

	if (_settings_client.network.last_joined.empty()) {
		IConsolePrint(CC_DEFAULT, "No server for reconnecting.");
		return true;
	}

	/* Don't resolve the address first, just print it directly as it comes from the config file. */
	IConsolePrint(CC_DEFAULT, "Reconnecting to {} ...", _settings_client.network.last_joined);

	return NetworkClientConnectGame(_settings_client.network.last_joined, playas);
}

static bool ConNetworkConnect(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "Connect to a remote OTTD server and join the game. Usage: 'connect <ip>'.");
		IConsolePrint(CC_HELP, "IP can contain port and company: 'IP[:Port][#Company]', eg: 'server.ottd.org:443#2'.");
		IConsolePrint(CC_HELP, "Company #255 is spectator all others are a certain company with Company 1 being #1.");
		return true;
	}

	if (argv.size() < 2) return false;

	return NetworkClientConnectGame(argv[1], COMPANY_NEW_COMPANY);
}

/*********************************
 *  script file console commands
 *********************************/

static bool ConExec(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "Execute a local script file. Usage: 'exec <script> [0]'.");
		IConsolePrint(CC_HELP, "By passing '0' after the script name, no warning about a missing script file will be shown.");
		return true;
	}

	if (argv.size() < 2) return false;

	auto script_file = FioFOpenFile(argv[1], "r", BASE_DIR);

	if (!script_file.has_value()) {
		if (argv.size() == 2 || argv[2] != "0") IConsolePrint(CC_ERROR, "Script file '{}' not found.", argv[1]);
		return true;
	}

	if (_script_current_depth == 11) {
		IConsolePrint(CC_ERROR, "Maximum 'exec' depth reached; script A is calling script B is calling script C ... more than 10 times.");
		return true;
	}

	_script_current_depth++;
	uint script_depth = _script_current_depth;

	char buffer[ICON_CMDLN_SIZE];
	while (fgets(buffer, sizeof(buffer), *script_file) != nullptr) {
		/* Remove newline characters from the executing script */
		std::string_view cmdline{buffer};
		auto last_non_newline = cmdline.find_last_not_of("\r\n");
		if (last_non_newline != std::string_view::npos) cmdline = cmdline.substr(0, last_non_newline + 1);

		IConsoleCmdExec(cmdline);
		/* Ensure that we are still on the same depth or that we returned via 'return'. */
		assert(_script_current_depth == script_depth || _script_current_depth == script_depth - 1);

		/* The 'return' command was executed. */
		if (_script_current_depth == script_depth - 1) break;
	}

	if (ferror(*script_file) != 0) {
		IConsolePrint(CC_ERROR, "Encountered error while trying to read from script file '{}'.", argv[1]);
	}

	if (_script_current_depth == script_depth) _script_current_depth--;
	return true;
}

static bool ConSchedule(std::span<std::string_view> argv)
{
	if (argv.size() < 3 || std::string_view(argv[1]) != "on-next-calendar-month") {
		IConsolePrint(CC_HELP, "Schedule a local script to execute later. Usage: 'schedule on-next-calendar-month <script>'.");
		return true;
	}

	/* Check if the file exists. It might still go away later, but helpful to show an error now. */
	if (!FioCheckFileExists(argv[2], BASE_DIR)) {
		IConsolePrint(CC_ERROR, "Script file '{}' not found.", argv[2]);
		return true;
	}

	/* We only support a single script scheduled, so we tell the user what's happening if there was already one. */
	std::string_view filename = std::string_view(argv[2]);
	if (!_scheduled_monthly_script.empty() && filename == _scheduled_monthly_script) {
		IConsolePrint(CC_INFO, "Script file '{}' was already scheduled to execute at the start of next calendar month.", filename);
	} else if (!_scheduled_monthly_script.empty() && filename != _scheduled_monthly_script) {
		IConsolePrint(CC_INFO, "Script file '{}' scheduled to execute at the start of next calendar month, replacing the previously scheduled script file '{}'.", filename, _scheduled_monthly_script);
	} else {
		IConsolePrint(CC_INFO, "Script file '{}' scheduled to execute at the start of next calendar month.", filename);
	}

	/* Store the filename to be used by _schedule_timer on the start of next calendar month. */
	_scheduled_monthly_script = filename;

	return true;
}

static bool ConReturn(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "Stop executing a running script. Usage: 'return'.");
		return true;
	}

	_script_current_depth--;
	return true;
}

/*****************************
 *  default console commands
 ******************************/
extern bool CloseConsoleLogIfActive();
extern std::span<const GRFFile> GetAllGRFFiles();
extern void ConPrintFramerate(); // framerate_gui.cpp
extern void ShowFramerateWindow();

static bool ConScript(std::span<std::string_view> argv)
{
	extern std::optional<FileHandle> _iconsole_output_file;

	if (argv.empty()) {
		IConsolePrint(CC_HELP, "Start or stop logging console output to a file. Usage: 'script <filename>'.");
		IConsolePrint(CC_HELP, "If filename is omitted, a running log is stopped if it is active.");
		return true;
	}

	if (!CloseConsoleLogIfActive()) {
		if (argv.size() < 2) return false;

		_iconsole_output_file = FileHandle::Open(argv[1], "ab");
		if (!_iconsole_output_file.has_value()) {
			IConsolePrint(CC_ERROR, "Could not open console log file '{}'.", argv[1]);
		} else {
			IConsolePrint(CC_INFO, "Console log output started to '{}'.", argv[1]);
		}
	}

	return true;
}

static bool ConEcho(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "Print back the first argument to the console. Usage: 'echo <arg>'.");
		return true;
	}

	if (argv.size() < 2) return false;
	IConsolePrint(CC_DEFAULT, "{}", argv[1]);
	return true;
}

static bool ConEchoC(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "Print back the first argument to the console in a given colour. Usage: 'echoc <colour> <arg2>'.");
		return true;
	}

	if (argv.size() < 3) return false;

	auto colour = ParseInteger(argv[1]);
	if (!colour.has_value() || !IsInsideMM(*colour, TC_BEGIN, TC_END)) {
		IConsolePrint(CC_ERROR, "The colour must be a number between {} and {}.", TC_BEGIN, TC_END - 1);
		return true;
	}

	IConsolePrint(static_cast<TextColour>(*colour), "{}", argv[2]);
	return true;
}

static bool ConNewGame(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "Start a new game. Usage: 'newgame [seed]'.");
		IConsolePrint(CC_HELP, "The server can force a new game using 'newgame'; any client joined will rejoin after the server is done generating the new game.");
		return true;
	}

	uint32_t seed = GENERATE_NEW_SEED;
	if (argv.size() >= 2) {
		auto param = ParseInteger(argv[1]);
		if (!param.has_value()) {
			IConsolePrint(CC_ERROR, "The given seed must be a valid number.");
			return true;
		}
		seed = *param;
	}

	StartNewGameWithoutGUI(seed);
	return true;
}

static bool ConRestart(std::span<std::string_view> argv)
{
	if (argv.empty() || argv.size() > 2) {
		IConsolePrint(CC_HELP, "Restart game. Usage: 'restart [current|newgame]'.");
		IConsolePrint(CC_HELP, "Restarts a game, using either the current or newgame (default) settings.");
		IConsolePrint(CC_HELP, " * if you started from a new game, and your current/newgame settings haven't changed, the game will be identical to when you started it.");
		IConsolePrint(CC_HELP, " * if you started from a savegame / scenario / heightmap, the game might be different, because the current/newgame settings might differ.");
		return true;
	}

	if (argv.size() == 1 || std::string_view(argv[1]) == "newgame") {
		StartNewGameWithoutGUI(_settings_game.game_creation.generation_seed);
	} else {
		_settings_game.game_creation.map_x = Map::LogX();
		_settings_game.game_creation.map_y = Map::LogY();
		_switch_mode = SM_RESTARTGAME;
	}

	return true;
}

static bool ConReload(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "Reload game. Usage: 'reload'.");
		IConsolePrint(CC_HELP, "Reloads a game if loaded via savegame / scenario / heightmap.");
		return true;
	}

	if (_file_to_saveload.ftype.abstract == FT_NONE || _file_to_saveload.ftype.abstract == FT_INVALID) {
		IConsolePrint(CC_ERROR, "No game loaded to reload.");
		return true;
	}

	/* Use a switch-mode to prevent copying over newgame settings to active settings. */
	_settings_game.game_creation.map_x = Map::LogX();
	_settings_game.game_creation.map_y = Map::LogY();
	_switch_mode = SM_RELOADGAME;
	return true;
}

/**
 * Print a text buffer line by line to the console. Lines are separated by '\n'.
 * @param full_string The multi-line string to print.
 */
static void PrintLineByLine(const std::string &full_string)
{
	std::istringstream in(full_string);
	std::string line;
	while (std::getline(in, line)) {
		IConsolePrint(CC_DEFAULT, line);
	}
}

template <typename F, typename ... Args>
bool PrintList(F list_function, Args... args)
{
	std::string output_str;
	auto inserter = std::back_inserter(output_str);
	list_function(inserter, args...);
	PrintLineByLine(output_str);

	return true;
}

static bool ConListAILibs(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "List installed AI libraries. Usage: 'list_ai_libs'.");
		return true;
	}

	return PrintList(AI::GetConsoleLibraryList);
}

static bool ConListAI(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "List installed AIs. Usage: 'list_ai'.");
		return true;
	}

	return PrintList(AI::GetConsoleList, false);
}

static bool ConListGameLibs(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "List installed Game Script libraries. Usage: 'list_game_libs'.");
		return true;
	}

	return PrintList(Game::GetConsoleLibraryList);
}

static bool ConListGame(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "List installed Game Scripts. Usage: 'list_game'.");
		return true;
	}

	return PrintList(Game::GetConsoleList, false);
}

static bool ConStartAI(std::span<std::string_view> argv)
{
	if (argv.empty() || argv.size() > 3) {
		IConsolePrint(CC_HELP, "Start a new AI. Usage: 'start_ai [<AI>] [<settings>]'.");
		IConsolePrint(CC_HELP, "Start a new AI. If <AI> is given, it starts that specific AI (if found).");
		IConsolePrint(CC_HELP, "If <settings> is given, it is parsed and the AI settings are set to that.");
		return true;
	}

	if (_game_mode != GM_NORMAL) {
		IConsolePrint(CC_ERROR, "AIs can only be managed in a game.");
		return true;
	}

	if (Company::GetNumItems() == CompanyPool::MAX_SIZE) {
		IConsolePrint(CC_ERROR, "Can't start a new AI (no more free slots).");
		return true;
	}
	if (_networking && !_network_server) {
		IConsolePrint(CC_ERROR, "Only the server can start a new AI.");
		return true;
	}
	if (_networking && !_settings_game.ai.ai_in_multiplayer) {
		IConsolePrint(CC_ERROR, "AIs are not allowed in multiplayer by configuration.");
		IConsolePrint(CC_ERROR, "Switch AI -> AI in multiplayer to True.");
		return true;
	}
	if (!AI::CanStartNew()) {
		IConsolePrint(CC_ERROR, "Can't start a new AI.");
		return true;
	}

	int n = 0;
	/* Find the next free slot */
	for (const Company *c : Company::Iterate()) {
		if (c->index != n) break;
		n++;
	}

	AIConfig *config = AIConfig::GetConfig((CompanyID)n);
	if (argv.size() >= 2) {
		config->Change(argv[1], -1, false);

		/* If the name is not found, and there is a dot in the name,
		 * try again with the assumption everything right of the dot is
		 * the version the user wants to load. */
		if (!config->HasScript()) {
			StringConsumer consumer{std::string_view{argv[1]}};
			auto name = consumer.ReadUntilChar('.', StringConsumer::SKIP_ONE_SEPARATOR);
			if (consumer.AnyBytesLeft()) {
				auto version = consumer.TryReadIntegerBase<uint32_t>(10);
				if (!version.has_value()) {
					IConsolePrint(CC_ERROR, "The version is not a valid number.");
					return true;
				}
				config->Change(name, *version, true);
			}
		}

		if (!config->HasScript()) {
			IConsolePrint(CC_ERROR, "Failed to load the specified AI.");
			return true;
		}
		if (argv.size() == 3) {
			config->StringToSettings(argv[2]);
		}
	}

	/* Start a new AI company */
	Command<Commands::CompanyControl>::Post(CCA_NEW_AI, CompanyID::Invalid(), CRR_NONE, INVALID_CLIENT_ID);

	return true;
}

static bool ConReloadAI(std::span<std::string_view> argv)
{
	if (argv.size() != 2) {
		IConsolePrint(CC_HELP, "Reload an AI. Usage: 'reload_ai <company-id>'.");
		IConsolePrint(CC_HELP, "Reload the AI with the given company id. For company-id's, see the list of companies from the dropdown menu. Company 1 is 1, etc.");
		return true;
	}

	if (_game_mode != GM_NORMAL) {
		IConsolePrint(CC_ERROR, "AIs can only be managed in a game.");
		return true;
	}

	if (_networking && !_network_server) {
		IConsolePrint(CC_ERROR, "Only the server can reload an AI.");
		return true;
	}

	auto company_id = ParseCompanyID(argv[1]);
	if (!company_id.has_value()) {
		IConsolePrint(CC_ERROR, "The given company-id is not a valid number.");
		return true;
	}

	if (!Company::IsValidID(*company_id)) {
		IConsolePrint(CC_ERROR, "Unknown company. Company range is between 1 and {}.", MAX_COMPANIES);
		return true;
	}

	/* In singleplayer mode the player can be in an AI company, after cheating or loading network save with an AI in first slot. */
	if (Company::IsHumanID(*company_id) || *company_id == _local_company) {
		IConsolePrint(CC_ERROR, "Company is not controlled by an AI.");
		return true;
	}

	/* First kill the company of the AI, then start a new one. This should start the current AI again */
	Command<Commands::CompanyControl>::Post(CCA_DELETE, *company_id, CRR_MANUAL, INVALID_CLIENT_ID);
	Command<Commands::CompanyControl>::Post(CCA_NEW_AI, *company_id, CRR_NONE, INVALID_CLIENT_ID);
	IConsolePrint(CC_DEFAULT, "AI reloaded.");

	return true;
}

static bool ConStopAI(std::span<std::string_view> argv)
{
	if (argv.size() != 2) {
		IConsolePrint(CC_HELP, "Stop an AI. Usage: 'stop_ai <company-id>'.");
		IConsolePrint(CC_HELP, "Stop the AI with the given company id. For company-id's, see the list of companies from the dropdown menu. Company 1 is 1, etc.");
		return true;
	}

	if (_game_mode != GM_NORMAL) {
		IConsolePrint(CC_ERROR, "AIs can only be managed in a game.");
		return true;
	}

	if (_networking && !_network_server) {
		IConsolePrint(CC_ERROR, "Only the server can stop an AI.");
		return true;
	}

	auto company_id = ParseCompanyID(argv[1]);
	if (!company_id.has_value()) {
		IConsolePrint(CC_ERROR, "The given company-id is not a valid number.");
		return true;
	}

	if (!Company::IsValidID(*company_id)) {
		IConsolePrint(CC_ERROR, "Unknown company. Company range is between 1 and {}.", MAX_COMPANIES);
		return true;
	}

	/* In singleplayer mode the player can be in an AI company, after cheating or loading network save with an AI in first slot. */
	if (Company::IsHumanID(*company_id) || *company_id == _local_company) {
		IConsolePrint(CC_ERROR, "Company is not controlled by an AI.");
		return true;
	}

	/* Now kill the company of the AI. */
	Command<Commands::CompanyControl>::Post(CCA_DELETE, *company_id, CRR_MANUAL, INVALID_CLIENT_ID);
	IConsolePrint(CC_DEFAULT, "AI stopped, company deleted.");

	return true;
}

static bool ConRescanAI(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "Rescan the AI dir for scripts. Usage: 'rescan_ai'.");
		return true;
	}

	if (_networking && !_network_server) {
		IConsolePrint(CC_ERROR, "Only the server can rescan the AI dir for scripts.");
		return true;
	}

	AI::Rescan();

	return true;
}

static bool ConRescanGame(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "Rescan the Game Script dir for scripts. Usage: 'rescan_game'.");
		return true;
	}

	if (_networking && !_network_server) {
		IConsolePrint(CC_ERROR, "Only the server can rescan the Game Script dir for scripts.");
		return true;
	}

	Game::Rescan();

	return true;
}

static bool ConRescanNewGRF(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "Rescan the data dir for NewGRFs. Usage: 'rescan_newgrf'.");
		return true;
	}

	if (!RequestNewGRFScan()) {
		IConsolePrint(CC_ERROR, "NewGRF scanning is already running. Please wait until completed to run again.");
	}

	return true;
}

static bool ConGetSeed(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "Returns the seed used to create this game. Usage: 'getseed'.");
		IConsolePrint(CC_HELP, "The seed can be used to reproduce the exact same map as the game started with.");
		return true;
	}

	IConsolePrint(CC_DEFAULT, "Generation Seed: {}", _settings_game.game_creation.generation_seed);
	return true;
}

static bool ConGetDate(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "Returns the current date (year-month-day) of the game. Usage: 'getdate'.");
		return true;
	}

	TimerGameCalendar::YearMonthDay ymd = TimerGameCalendar::ConvertDateToYMD(TimerGameCalendar::date);
	IConsolePrint(CC_DEFAULT, "Date: {:04d}-{:02d}-{:02d}", ymd.year, ymd.month + 1, ymd.day);
	return true;
}

static bool ConGetSysDate(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "Returns the current date (year-month-day) of your system. Usage: 'getsysdate'.");
		return true;
	}

	IConsolePrint(CC_DEFAULT, "System Date: {:%Y-%m-%d %H:%M:%S}", fmt::localtime(time(nullptr)));
	return true;
}

static bool ConAlias(std::span<std::string_view> argv)
{
	IConsoleAlias *alias;

	if (argv.empty()) {
		IConsolePrint(CC_HELP, "Add a new alias, or redefine the behaviour of an existing alias . Usage: 'alias <name> <command>'.");
		return true;
	}

	if (argv.size() < 3) return false;

	alias = IConsole::AliasGet(std::string(argv[1]));
	if (alias == nullptr) {
		IConsole::AliasRegister(std::string(argv[1]), argv[2]);
	} else {
		alias->cmdline = argv[2];
	}
	return true;
}

static bool ConScreenShot(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "Create a screenshot of the game. Usage: 'screenshot [viewport | normal | big | giant | heightmap | minimap] [no_con] [size <width> <height>] [<filename>]'.");
		IConsolePrint(CC_HELP, "  'viewport' (default) makes a screenshot of the current viewport (including menus, windows).");
		IConsolePrint(CC_HELP, "  'normal' makes a screenshot of the visible area.");
		IConsolePrint(CC_HELP, "  'big' makes a zoomed-in screenshot of the visible area.");
		IConsolePrint(CC_HELP, "  'giant' makes a screenshot of the whole map.");
		IConsolePrint(CC_HELP, "  'heightmap' makes a heightmap screenshot of the map that can be loaded in as heightmap.");
		IConsolePrint(CC_HELP, "  'minimap' makes a top-viewed minimap screenshot of the whole world which represents one tile by one pixel.");
		IConsolePrint(CC_HELP, "  'no_con' hides the console to create the screenshot (only useful in combination with 'viewport').");
		IConsolePrint(CC_HELP, "  'size' sets the width and height of the viewport to make a screenshot of (only useful in combination with 'normal' or 'big').");
		IConsolePrint(CC_HELP, "  A filename ending in # will prevent overwriting existing files and will number files counting upwards.");
		return true;
	}

	if (argv.size() > 7) return false;

	ScreenshotType type = SC_VIEWPORT;
	uint32_t width = 0;
	uint32_t height = 0;
	std::string name{};
	uint32_t arg_index = 1;

	if (argv.size() > arg_index) {
		if (argv[arg_index] == "viewport") {
			type = SC_VIEWPORT;
			arg_index += 1;
		} else if (argv[arg_index] == "normal") {
			type = SC_DEFAULTZOOM;
			arg_index += 1;
		} else if (argv[arg_index] == "big") {
			type = SC_ZOOMEDIN;
			arg_index += 1;
		} else if (argv[arg_index] == "giant") {
			type = SC_WORLD;
			arg_index += 1;
		} else if (argv[arg_index] == "heightmap") {
			type = SC_HEIGHTMAP;
			arg_index += 1;
		} else if (argv[arg_index] == "minimap") {
			type = SC_MINIMAP;
			arg_index += 1;
		}
	}

	if (argv.size() > arg_index && argv[arg_index] == "no_con") {
		if (type != SC_VIEWPORT) {
			IConsolePrint(CC_ERROR, "'no_con' can only be used in combination with 'viewport'.");
			return true;
		}
		IConsoleClose();
		arg_index += 1;
	}

	if (argv.size() > arg_index + 2 && argv[arg_index] == "size") {
		/* size <width> <height> */
		if (type != SC_DEFAULTZOOM && type != SC_ZOOMEDIN) {
			IConsolePrint(CC_ERROR, "'size' can only be used in combination with 'normal' or 'big'.");
			return true;
		}
		auto t = ParseInteger(argv[arg_index + 1]);
		if (!t.has_value()) {
			IConsolePrint(CC_ERROR, "Invalid width '{}'", argv[arg_index + 1]);
			return true;
		}
		width = *t;

		t = ParseInteger(argv[arg_index + 2]);
		if (!t.has_value()) {
			IConsolePrint(CC_ERROR, "Invalid height '{}'", argv[arg_index + 2]);
			return true;
		}
		height = *t;
		arg_index += 3;
	}

	if (argv.size() > arg_index) {
		/* Last parameter that was not one of the keywords must be the filename. */
		name = argv[arg_index];
		arg_index += 1;
	}

	if (argv.size() > arg_index) {
		/* We have parameters we did not process; means we misunderstood any of the above. */
		return false;
	}

	MakeScreenshot(type, std::move(name), width, height);
	return true;
}

static bool ConInfoCmd(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "Print out debugging information about a command. Usage: 'info_cmd <cmd>'.");
		return true;
	}

	if (argv.size() < 2) return false;

	const IConsoleCmd *cmd = IConsole::CmdGet(std::string(argv[1]));
	if (cmd == nullptr) {
		IConsolePrint(CC_ERROR, "The given command was not found.");
		return true;
	}

	IConsolePrint(CC_DEFAULT, "Command name: '{}'", cmd->name);

	if (cmd->hook != nullptr) IConsolePrint(CC_DEFAULT, "Command is hooked.");

	return true;
}

static bool ConDebugLevel(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "Get/set the default debugging level for the game. Usage: 'debug_level [<level>]'.");
		IConsolePrint(CC_HELP, "Level can be any combination of names, levels. Eg 'net=5 ms=4'. Remember to enclose it in \"'\"s.");
		return true;
	}

	if (argv.size() > 2) return false;

	if (argv.size() == 1) {
		IConsolePrint(CC_DEFAULT, "Current debug-level: '{}'", GetDebugString());
	} else {
		SetDebugString(argv[1], [](std::string_view err) { IConsolePrint(CC_ERROR, "{}", err); });
	}

	return true;
}

static bool ConExit(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "Exit the game. Usage: 'exit'.");
		return true;
	}

	if (_game_mode == GM_NORMAL && _settings_client.gui.autosave_on_exit) DoExitSave();

	_exit_game = true;
	return true;
}

static bool ConPart(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "Leave the currently joined/running game (only ingame). Usage: 'part'.");
		return true;
	}

	if (_game_mode != GM_NORMAL) return false;

	if (_network_dedicated) {
		IConsolePrint(CC_ERROR, "A dedicated server can not leave the game.");
		return false;
	}

	_switch_mode = SM_MENU;
	return true;
}

static bool ConHelp(std::span<std::string_view> argv)
{
	if (argv.size() == 2) {
		const IConsoleCmd *cmd;
		const IConsoleAlias *alias;

		cmd = IConsole::CmdGet(std::string(argv[1]));
		if (cmd != nullptr) {
			cmd->proc({});
			return true;
		}

		alias = IConsole::AliasGet(std::string(argv[1]));
		if (alias != nullptr) {
			cmd = IConsole::CmdGet(alias->cmdline);
			if (cmd != nullptr) {
				cmd->proc({});
				return true;
			}
			IConsolePrint(CC_ERROR, "Alias is of special type, please see its execution-line: '{}'.", alias->cmdline);
			return true;
		}

		IConsolePrint(CC_ERROR, "Command not found.");
		return true;
	}

	IConsolePrint(TC_LIGHT_BLUE, " ---- OpenTTD Console Help ---- ");
	IConsolePrint(CC_DEFAULT, " - commands: the command to list all commands is 'list_cmds'.");
	IConsolePrint(CC_DEFAULT, " call commands with '<command> <arg2> <arg3>...'");
	IConsolePrint(CC_DEFAULT, " - to assign strings, or use them as arguments, enclose it within quotes.");
	IConsolePrint(CC_DEFAULT, " like this: '<command> \"string argument with spaces\"'.");
	IConsolePrint(CC_DEFAULT, " - use 'help <command>' to get specific information.");
	IConsolePrint(CC_DEFAULT, " - scroll console output with shift + (up | down | pageup | pagedown).");
	IConsolePrint(CC_DEFAULT, " - scroll console input history with the up or down arrows.");
	IConsolePrint(CC_DEFAULT, "");
	return true;
}

static bool ConListCommands(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "List all registered commands. Usage: 'list_cmds [<pre-filter>]'.");
		return true;
	}

	for (auto &it : IConsole::Commands()) {
		const IConsoleCmd *cmd = &it.second;
		if (argv.size() <= 1|| cmd->name.find(argv[1]) != std::string::npos) {
			if (cmd->hook == nullptr || cmd->hook(false) != CHR_HIDE) IConsolePrint(CC_DEFAULT, cmd->name);
		}
	}

	return true;
}

static bool ConListAliases(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "List all registered aliases. Usage: 'list_aliases [<pre-filter>]'.");
		return true;
	}

	for (auto &it : IConsole::Aliases()) {
		const IConsoleAlias *alias = &it.second;
		if (argv.size() <= 1 || alias->name.find(argv[1]) != std::string::npos) {
			IConsolePrint(CC_DEFAULT, "{} => {}", alias->name, alias->cmdline);
		}
	}

	return true;
}

static bool ConCompanies(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "List the details of all companies in the game. Usage 'companies'.");
		return true;
	}

	for (const Company *c : Company::Iterate()) {
		/* Grab the company name */
		std::string company_name = GetString(STR_COMPANY_NAME, c->index);

		std::string colour = GetString(STR_COLOUR_DARK_BLUE + _company_colours[c->index]);
		IConsolePrint(CC_INFO, "#:{}({}) Company Name: '{}'  Year Founded: {}  Money: {}  Loan: {}  Value: {}  (T:{}, R:{}, P:{}, S:{}) {}",
			c->index + 1, colour, company_name,
			c->inaugurated_year, (int64_t)c->money, (int64_t)c->current_loan, (int64_t)CalculateCompanyValue(c),
			c->group_all[VEH_TRAIN].num_vehicle,
			c->group_all[VEH_ROAD].num_vehicle,
			c->group_all[VEH_AIRCRAFT].num_vehicle,
			c->group_all[VEH_SHIP].num_vehicle,
			c->is_ai ? "AI" : "");
	}

	return true;
}

static bool ConSay(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "Chat to your fellow players in a multiplayer game. Usage: 'say \"<msg>\"'.");
		return true;
	}

	if (argv.size() != 2) return false;

	if (!_network_server) {
		NetworkClientSendChat(NETWORK_ACTION_CHAT, DESTTYPE_BROADCAST, 0 /* param does not matter */, argv[1]);
	} else {
		bool from_admin = (_redirect_console_to_admin < AdminID::Invalid());
		NetworkServerSendChat(NETWORK_ACTION_CHAT, DESTTYPE_BROADCAST, 0, argv[1], CLIENT_ID_SERVER, from_admin);
	}

	return true;
}

static bool ConSayCompany(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "Chat to a certain company in a multiplayer game. Usage: 'say_company <company-no> \"<msg>\"'.");
		IConsolePrint(CC_HELP, "CompanyNo is the company that plays as company <companyno>, 1 through max_companies.");
		return true;
	}

	if (argv.size() != 3) return false;

	auto company_id = ParseCompanyID(argv[1]);
	if (!company_id.has_value()) {
		IConsolePrint(CC_ERROR, "The given company-id is not a valid number.");
		return true;
	}

	if (!Company::IsValidID(*company_id)) {
		IConsolePrint(CC_DEFAULT, "Unknown company. Company range is between 1 and {}.", MAX_COMPANIES);
		return true;
	}

	if (!_network_server) {
		NetworkClientSendChat(NETWORK_ACTION_CHAT_COMPANY, DESTTYPE_TEAM, company_id->base(), argv[2]);
	} else {
		bool from_admin = (_redirect_console_to_admin < AdminID::Invalid());
		NetworkServerSendChat(NETWORK_ACTION_CHAT_COMPANY, DESTTYPE_TEAM, company_id->base(), argv[2], CLIENT_ID_SERVER, from_admin);
	}

	return true;
}

static bool ConSayClient(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "Chat to a certain client in a multiplayer game. Usage: 'say_client <client-id> \"<msg>\"'.");
		IConsolePrint(CC_HELP, "For client-id's, see the command 'clients'.");
		return true;
	}

	if (argv.size() != 3) return false;

	auto client_id = ParseType<ClientID>(argv[1]);
	if (!client_id.has_value()) {
		IConsolePrint(CC_ERROR, "The given client-id is not a valid number.");
		return true;
	}

	if (!_network_server) {
		NetworkClientSendChat(NETWORK_ACTION_CHAT_CLIENT, DESTTYPE_CLIENT, *client_id, argv[2]);
	} else {
		bool from_admin = (_redirect_console_to_admin < AdminID::Invalid());
		NetworkServerSendChat(NETWORK_ACTION_CHAT_CLIENT, DESTTYPE_CLIENT, *client_id, argv[2], CLIENT_ID_SERVER, from_admin);
	}

	return true;
}

/** All the known authorized keys with their name. */
static const std::initializer_list<std::pair<std::string_view, NetworkAuthorizedKeys *>> _console_cmd_authorized_keys{
	{ "admin", &_settings_client.network.admin_authorized_keys },
	{ "rcon", &_settings_client.network.rcon_authorized_keys },
	{ "server", &_settings_client.network.server_authorized_keys },
};

enum ConNetworkAuthorizedKeyAction : uint8_t {
	CNAKA_LIST,
	CNAKA_ADD,
	CNAKA_REMOVE,
};

static void PerformNetworkAuthorizedKeyAction(std::string_view name, NetworkAuthorizedKeys *authorized_keys, ConNetworkAuthorizedKeyAction action, const std::string &authorized_key, CompanyID company = CompanyID::Invalid())
{
	switch (action) {
		case CNAKA_LIST:
			IConsolePrint(CC_WHITE, "The authorized keys for {} are:", name);
			for (auto &ak : *authorized_keys) IConsolePrint(CC_INFO, "  {}", ak);
			return;

		case CNAKA_ADD:
			if (authorized_keys->Contains(authorized_key)) {
				IConsolePrint(CC_WARNING, "Not added {} to {} as it already exists.", authorized_key, name);
				return;
			}

			if (company == CompanyID::Invalid()) {
				authorized_keys->Add(authorized_key);
			} else {
				AutoRestoreBackup backup(_current_company, company);
				Command<Commands::CompanyAllowListControl>::Post(CALCA_ADD, authorized_key);
			}
			IConsolePrint(CC_INFO, "Added {} to {}.", authorized_key, name);
			return;

		case CNAKA_REMOVE:
			if (!authorized_keys->Contains(authorized_key)) {
				IConsolePrint(CC_WARNING, "Not removed {} from {} as it does not exist.", authorized_key, name);
				return;
			}

			if (company == CompanyID::Invalid()) {
				authorized_keys->Remove(authorized_key);
			} else {
				AutoRestoreBackup backup(_current_company, company);
				Command<Commands::CompanyAllowListControl>::Post(CALCA_REMOVE, authorized_key);
			}
			IConsolePrint(CC_INFO, "Removed {} from {}.", authorized_key, name);
			return;
	}
}

static bool ConNetworkAuthorizedKey(std::span<std::string_view> argv)
{
	if (argv.size() <= 2) {
		IConsolePrint(CC_HELP, "List and update authorized keys. Usage: 'authorized_key list [type]|add [type] [key]|remove [type] [key]'.");
		IConsolePrint(CC_HELP, "  list: list all the authorized keys of the given type.");
		IConsolePrint(CC_HELP, "  add: add the given key to the authorized keys of the given type.");
		IConsolePrint(CC_HELP, "  remove: remove the given key from the authorized keys of the given type; use 'all' to remove all authorized keys.");
		IConsolePrint(CC_HELP, "Instead of a key, use 'client:<id>' to add/remove the key of that given client.");

		std::string buffer;
		for (auto [name, _] : _console_cmd_authorized_keys) format_append(buffer, ", {}", name);
		IConsolePrint(CC_HELP, "The supported types are: all{} and company:<id>.", buffer);
		return true;
	}

	ConNetworkAuthorizedKeyAction action;
	std::string_view action_string = argv[1];
	if (StrEqualsIgnoreCase(action_string, "list")) {
		action = CNAKA_LIST;
	} else if (StrEqualsIgnoreCase(action_string, "add")) {
		action = CNAKA_ADD;
	} else if (StrEqualsIgnoreCase(action_string, "remove") || StrEqualsIgnoreCase(action_string, "delete")) {
		action = CNAKA_REMOVE;
	} else {
		IConsolePrint(CC_WARNING, "No valid action was given.");
		return false;
	}

	std::string authorized_key;
	if (action != CNAKA_LIST) {
		if (argv.size() <= 3) {
			IConsolePrint(CC_ERROR, "You must enter the key.");
			return false;
		}

		authorized_key = argv[3];
		if (StrStartsWithIgnoreCase(authorized_key, "client:")) {
			auto value = ParseInteger<uint32_t>(authorized_key.substr(7));
			if (value.has_value()) authorized_key = NetworkGetPublicKeyOfClient(static_cast<ClientID>(*value));
			if (!value.has_value() || authorized_key.empty()) {
				IConsolePrint(CC_ERROR, "You must enter a valid client id; see 'clients'.");
				return false;
			}
		}

		if (authorized_key.size() != NETWORK_PUBLIC_KEY_LENGTH - 1) {
			IConsolePrint(CC_ERROR, "You must enter a valid authorized key.");
			return false;
		}
	}

	std::string_view type = argv[2];
	if (StrEqualsIgnoreCase(type, "all")) {
		for (auto [name, authorized_keys] : _console_cmd_authorized_keys) PerformNetworkAuthorizedKeyAction(name, authorized_keys, action, authorized_key);
		for (Company *c : Company::Iterate()) PerformNetworkAuthorizedKeyAction(fmt::format("company:{}", c->index + 1), &c->allow_list, action, authorized_key, c->index);
		return true;
	}

	if (StrStartsWithIgnoreCase(type, "company:")) {
		auto value = ParseInteger<uint32_t>(type.substr(8));
		Company *c = value.has_value() ? Company::GetIfValid(*value - 1) : nullptr;
		if (c == nullptr) {
			IConsolePrint(CC_ERROR, "You must enter a valid company id; see 'companies'.");
			return false;
		}

		PerformNetworkAuthorizedKeyAction(type, &c->allow_list, action, authorized_key, c->index);
		return true;
	}

	for (auto [name, authorized_keys] : _console_cmd_authorized_keys) {
		if (!StrEqualsIgnoreCase(type, name)) continue;

		PerformNetworkAuthorizedKeyAction(name, authorized_keys, action, authorized_key);
		return true;
	}

	IConsolePrint(CC_WARNING, "No valid type was given.");
	return false;
}


/* Content downloading only is available with ZLIB */
#if defined(WITH_ZLIB)

/** Resolve a string to a content type. */
static ContentType StringToContentType(std::string_view str)
{
	static const std::initializer_list<std::pair<std::string_view, ContentType>> content_types = {
		{"base",      CONTENT_TYPE_BASE_GRAPHICS},
		{"newgrf",    CONTENT_TYPE_NEWGRF},
		{"ai",        CONTENT_TYPE_AI},
		{"ailib",     CONTENT_TYPE_AI_LIBRARY},
		{"scenario",  CONTENT_TYPE_SCENARIO},
		{"heightmap", CONTENT_TYPE_HEIGHTMAP},
	};
	for (const auto &ct : content_types) {
		if (StrEqualsIgnoreCase(str, ct.first)) return ct.second;
	}
	return CONTENT_TYPE_END;
}

/** Asynchronous callback */
struct ConsoleContentCallback : public ContentCallback {
	void OnConnect(bool success) override
	{
		IConsolePrint(CC_DEFAULT, "Content server connection {}.", success ? "established" : "failed");
	}

	void OnDisconnect() override
	{
		IConsolePrint(CC_DEFAULT, "Content server connection closed.");
	}

	void OnDownloadComplete(ContentID cid) override
	{
		IConsolePrint(CC_DEFAULT, "Completed download of {}.", cid);
	}
};

/**
 * Outputs content state information to console
 * @param ci the content info
 */
static void OutputContentState(const ContentInfo &ci)
{
	static const std::string_view types[] = { "Base graphics", "NewGRF", "AI", "AI library", "Scenario", "Heightmap", "Base sound", "Base music", "Game script", "GS library" };
	static_assert(lengthof(types) == CONTENT_TYPE_END - CONTENT_TYPE_BEGIN);
	static const std::string_view states[] = { "Not selected", "Selected", "Dep Selected", "Installed", "Unknown" };
	static const TextColour state_to_colour[] = { CC_COMMAND, CC_INFO, CC_INFO, CC_WHITE, CC_ERROR };

	IConsolePrint(state_to_colour[to_underlying(ci.state)], "{}, {}, {}, {}, {:08X}, {}", ci.id, types[ci.type - 1], states[to_underlying(ci.state)], ci.name, ci.unique_id, FormatArrayAsHex(ci.md5sum));
}

static bool ConContent(std::span<std::string_view> argv)
{
	[[maybe_unused]] static ContentCallback *const cb = []() {
			auto res = new ConsoleContentCallback();
			_network_content_client.AddCallback(res);
			return res;
		}();

	if (argv.size() <= 1) {
		IConsolePrint(CC_HELP, "Query, select and download content. Usage: 'content update|upgrade|select [id]|unselect [all|id]|state [filter]|download'.");
		IConsolePrint(CC_HELP, "  update: get a new list of downloadable content; must be run first.");
		IConsolePrint(CC_HELP, "  upgrade: select all items that are upgrades.");
		IConsolePrint(CC_HELP, "  select: select a specific item given by its id. If no parameter is given, all selected content will be listed.");
		IConsolePrint(CC_HELP, "  unselect: unselect a specific item given by its id or 'all' to unselect all.");
		IConsolePrint(CC_HELP, "  state: show the download/select state of all downloadable content. Optionally give a filter string.");
		IConsolePrint(CC_HELP, "  download: download all content you've selected.");
		return true;
	}

	if (StrEqualsIgnoreCase(argv[1], "update")) {
		_network_content_client.RequestContentList((argv.size() > 2) ? StringToContentType(argv[2]) : CONTENT_TYPE_END);
		return true;
	}

	if (StrEqualsIgnoreCase(argv[1], "upgrade")) {
		_network_content_client.SelectUpgrade();
		return true;
	}

	if (StrEqualsIgnoreCase(argv[1], "select")) {
		if (argv.size() <= 2) {
			/* List selected content */
			IConsolePrint(CC_WHITE, "id, type, state, name");
			for (const ContentInfo &ci : _network_content_client.Info()) {
				if (ci.state != ContentInfo::State::Selected && ci.state != ContentInfo::State::Autoselected) continue;
				OutputContentState(ci);
			}
		} else if (StrEqualsIgnoreCase(argv[2], "all")) {
			/* The intention of this function was that you could download
			 * everything after a filter was applied; but this never really
			 * took off. Instead, a select few people used this functionality
			 * to download every available package on BaNaNaS. This is not in
			 * the spirit of this service. Additionally, these few people were
			 * good for 70% of the consumed bandwidth of BaNaNaS. */
			IConsolePrint(CC_ERROR, "'select all' is no longer supported since 1.11.");
		} else if (auto content_id = ParseType<ContentID>(argv[2]); content_id.has_value()) {
			_network_content_client.Select(*content_id);
		} else {
			IConsolePrint(CC_ERROR, "The given content-id is not a number or 'all'");
		}
		return true;
	}

	if (StrEqualsIgnoreCase(argv[1], "unselect")) {
		if (argv.size() <= 2) {
			IConsolePrint(CC_ERROR, "You must enter the id.");
			return false;
		}
		if (StrEqualsIgnoreCase(argv[2], "all")) {
			_network_content_client.UnselectAll();
		} else if (auto content_id = ParseType<ContentID>(argv[2]); content_id.has_value()) {
			_network_content_client.Unselect(*content_id);
		} else {
			IConsolePrint(CC_ERROR, "The given content-id is not a number or 'all'");
		}
		return true;
	}

	if (StrEqualsIgnoreCase(argv[1], "state")) {
		IConsolePrint(CC_WHITE, "id, type, state, name");
		for (const ContentInfo &ci : _network_content_client.Info()) {
			if (argv.size() > 2 && !StrContainsIgnoreCase(ci.name, argv[2])) continue;
			OutputContentState(ci);
		}
		return true;
	}

	if (StrEqualsIgnoreCase(argv[1], "download")) {
		uint files;
		uint bytes;
		_network_content_client.DownloadSelectedContent(files, bytes);
		IConsolePrint(CC_DEFAULT, "Downloading {} file(s) ({} bytes).", files, bytes);
		return true;
	}

	return false;
}
#endif /* defined(WITH_ZLIB) */

static bool ConFont(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "Manage the fonts configuration.");
		IConsolePrint(CC_HELP, "Usage 'font'.");
		IConsolePrint(CC_HELP, "  Print out the fonts configuration.");
		IConsolePrint(CC_HELP, "  The \"Currently active\" configuration is the one actually in effect (after interface scaling and replacing unavailable fonts).");
		IConsolePrint(CC_HELP, "  The \"Requested\" configuration is the one requested via console command or config file.");
		IConsolePrint(CC_HELP, "Usage 'font [medium|small|large|mono] [<font name>] [<size>]'.");
		IConsolePrint(CC_HELP, "  Change the configuration for a font.");
		IConsolePrint(CC_HELP, "  Omitting an argument will keep the current value.");
		IConsolePrint(CC_HELP, "  Set <font name> to \"\" for the default font. Note that <size> has no effect if the default font is in use, and fixed defaults are used instead.");
		IConsolePrint(CC_HELP, "  If the sprite font is enabled in Game Options, it is used instead of the default font.");
		IConsolePrint(CC_HELP, "  The <size> is automatically multiplied by the current interface scaling.");
		return true;
	}

	FontSize argfs;
	for (argfs = FS_BEGIN; argfs < FS_END; argfs++) {
		if (argv.size() > 1 && StrEqualsIgnoreCase(argv[1], FontSizeToName(argfs))) break;
	}

	/* First argument must be a FontSize. */
	if (argv.size() > 1 && argfs == FS_END) return false;

	if (argv.size() > 2) {
		FontCacheSubSetting *setting = GetFontCacheSubSetting(argfs);
		std::string font = setting->font;
		uint size = setting->size;
		uint8_t arg_index = 2;
		/* For <name> we want a string. */

		if (!ParseInteger(argv[arg_index]).has_value()) {
			font = argv[arg_index++];
		}

		if (argv.size() > arg_index) {
			/* For <size> we want a number. */
			auto v = ParseInteger(argv[arg_index]);
			if (v.has_value()) {
				size = *v;
				arg_index++;
			}
		}

		SetFont(argfs, font, size);
	}

	for (FontSize fs = FS_BEGIN; fs < FS_END; fs++) {
		FontCache *fc = FontCache::Get(fs);
		FontCacheSubSetting *setting = GetFontCacheSubSetting(fs);
		/* Make sure all non sprite fonts are loaded. */
		if (!setting->font.empty() && !fc->HasParent()) {
			FontCache::LoadFontCaches(fs);
			fc = FontCache::Get(fs);
		}
		IConsolePrint(CC_DEFAULT, "{} font:", FontSizeToName(fs));
		IConsolePrint(CC_DEFAULT, "Currently active: \"{}\", size {}", fc->GetFontName(), fc->GetFontSize());
		IConsolePrint(CC_DEFAULT, "Requested: \"{}\", size {}", setting->font, setting->size);
	}

	return true;
}

static bool ConSetting(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "Change setting for all clients. Usage: 'setting <name> [<value>]'.");
		IConsolePrint(CC_HELP, "Omitting <value> will print out the current value of the setting.");
		return true;
	}

	if (argv.size() == 1 || argv.size() > 3) return false;

	if (argv.size() == 2) {
		IConsoleGetSetting(argv[1]);
	} else {
		IConsoleSetSetting(argv[1], argv[2]);
	}

	return true;
}

static bool ConSettingNewgame(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "Change setting for the next game. Usage: 'setting_newgame <name> [<value>]'.");
		IConsolePrint(CC_HELP, "Omitting <value> will print out the current value of the setting.");
		return true;
	}

	if (argv.size() == 1 || argv.size() > 3) return false;

	if (argv.size() == 2) {
		IConsoleGetSetting(argv[1], true);
	} else {
		IConsoleSetSetting(argv[1], argv[2], true);
	}

	return true;
}

static bool ConListSettings(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "List settings. Usage: 'list_settings [<pre-filter>]'.");
		return true;
	}

	if (argv.size() > 2) return false;

	IConsoleListSettings((argv.size() == 2) ? argv[1] : std::string_view{});
	return true;
}

static bool ConGamelogPrint(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "Print logged fundamental changes to the game since the start. Usage: 'gamelog'.");
		return true;
	}

	_gamelog.PrintConsole();
	return true;
}

static bool ConNewGRFReload(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "Reloads all active NewGRFs from disk. Equivalent to reapplying NewGRFs via the settings, but without asking for confirmation. This might crash OpenTTD!");
		return true;
	}

	ReloadNewGRFData();
	return true;
}

static bool ConListDirs(std::span<std::string_view> argv)
{
	struct SubdirNameMap {
		std::string_view name; ///< UI name for the directory
		Subdirectory subdir; ///< Index of subdirectory type
		bool default_only; ///< Whether only the default (first existing) directory for this is interesting
	};
	static const SubdirNameMap subdir_name_map[] = {
		/* Game data directories */
		{ "baseset", BASESET_DIR, false },
		{ "newgrf", NEWGRF_DIR, false },
		{ "ai", AI_DIR, false },
		{ "ailib", AI_LIBRARY_DIR, false },
		{ "gs", GAME_DIR, false },
		{ "gslib", GAME_LIBRARY_DIR, false },
		{ "scenario", SCENARIO_DIR, false },
		{ "heightmap", HEIGHTMAP_DIR, false },
		/* Default save locations for user data */
		{ "save", SAVE_DIR, true },
		{ "autosave", AUTOSAVE_DIR, true },
		{ "screenshot", SCREENSHOT_DIR, true },
		{ "social_integration", SOCIAL_INTEGRATION_DIR, true },
	};

	if (argv.size() != 2) {
		IConsolePrint(CC_HELP, "List all search paths or default directories for various categories.");
		IConsolePrint(CC_HELP, "Usage: list_dirs <category>");
		std::string cats{subdir_name_map[0].name};
		bool first = true;
		for (const SubdirNameMap &sdn : subdir_name_map) {
			if (!first) {
				cats += ", ";
				cats += sdn.name;
			}
			first = false;
		}
		IConsolePrint(CC_HELP, "Valid categories: {}", cats);
		return true;
	}

	std::set<std::string> seen_dirs;
	for (const SubdirNameMap &sdn : subdir_name_map) {
		if (!StrEqualsIgnoreCase(argv[1], sdn.name))  continue;
		bool found = false;
		for (Searchpath sp : _valid_searchpaths) {
			/* Get the directory */
			std::string path = FioGetDirectory(sp, sdn.subdir);
			/* Check it hasn't already been listed */
			if (seen_dirs.find(path) != seen_dirs.end()) continue;
			seen_dirs.insert(path);
			/* Check if exists and mark found */
			bool exists = FileExists(path);
			found |= exists;
			/* Print */
			if (!sdn.default_only || exists) {
				IConsolePrint(exists ? CC_DEFAULT : CC_INFO, "{} {}", path, exists ? "[ok]" : "[not found]");
				if (sdn.default_only) break;
			}
		}
		if (!found) {
			IConsolePrint(CC_ERROR, "No directories exist for category {}", argv[1]);
		}
		return true;
	}

	IConsolePrint(CC_ERROR, "Invalid category name: {}", argv[1]);
	return false;
}

static bool ConNewGRFProfile(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "Collect performance data about NewGRF sprite requests and callbacks. Sub-commands can be abbreviated.");
		IConsolePrint(CC_HELP, "Usage: 'newgrf_profile [list]':");
		IConsolePrint(CC_HELP, "  List all NewGRFs that can be profiled, and their status.");
		IConsolePrint(CC_HELP, "Usage: 'newgrf_profile select <grf-num>...':");
		IConsolePrint(CC_HELP, "  Select one or more GRFs for profiling.");
		IConsolePrint(CC_HELP, "Usage: 'newgrf_profile unselect <grf-num>...':");
		IConsolePrint(CC_HELP, "  Unselect one or more GRFs from profiling. Use the keyword \"all\" instead of a GRF number to unselect all. Removing an active profiler aborts data collection.");
		IConsolePrint(CC_HELP, "Usage: 'newgrf_profile start [<num-ticks>]':");
		IConsolePrint(CC_HELP, "  Begin profiling all selected GRFs. If a number of ticks is provided, profiling stops after that many game ticks. There are 74 ticks in a calendar day.");
		IConsolePrint(CC_HELP, "Usage: 'newgrf_profile stop':");
		IConsolePrint(CC_HELP, "  End profiling and write the collected data to CSV files.");
		IConsolePrint(CC_HELP, "Usage: 'newgrf_profile abort':");
		IConsolePrint(CC_HELP, "  End profiling and discard all collected data.");
		return true;
	}

	std::span<const GRFFile> files = GetAllGRFFiles();

	/* "list" sub-command */
	if (argv.size() == 1 || StrStartsWithIgnoreCase(argv[1], "lis")) {
		IConsolePrint(CC_INFO, "Loaded GRF files:");
		int i = 1;
		for (const auto &grf : files) {
			auto profiler = std::ranges::find(_newgrf_profilers, &grf, &NewGRFProfiler::grffile);
			bool selected = profiler != _newgrf_profilers.end();
			bool active = selected && profiler->active;
			TextColour tc = active ? TC_LIGHT_BLUE : selected ? TC_GREEN : CC_INFO;
			std::string_view statustext = active ? " (active)" : selected ? " (selected)" : "";
			IConsolePrint(tc, "{}: [{:08X}] {}{}", i, std::byteswap(grf.grfid), grf.filename, statustext);
			i++;
		}
		return true;
	}

	/* "select" sub-command */
	if (StrStartsWithIgnoreCase(argv[1], "sel") && argv.size() >= 3) {
		for (size_t argnum = 2; argnum < argv.size(); ++argnum) {
			auto grfnum = ParseInteger(argv[argnum]);
			if (!grfnum.has_value() || *grfnum < 1 || static_cast<size_t>(*grfnum) > files.size()) {
				IConsolePrint(CC_WARNING, "GRF number {} out of range, not added.", *grfnum);
				continue;
			}
			const GRFFile *grf = &files[*grfnum - 1];
			if (std::any_of(_newgrf_profilers.begin(), _newgrf_profilers.end(), [&](NewGRFProfiler &pr) { return pr.grffile == grf; })) {
				IConsolePrint(CC_WARNING, "GRF number {} [{:08X}] is already selected for profiling.", *grfnum, std::byteswap(grf->grfid));
				continue;
			}
			_newgrf_profilers.emplace_back(grf);
		}
		return true;
	}

	/* "unselect" sub-command */
	if (StrStartsWithIgnoreCase(argv[1], "uns") && argv.size() >= 3) {
		for (size_t argnum = 2; argnum < argv.size(); ++argnum) {
			if (StrEqualsIgnoreCase(argv[argnum], "all")) {
				_newgrf_profilers.clear();
				break;
			}
			auto grfnum = ParseInteger(argv[argnum]);
			if (!grfnum.has_value() || *grfnum < 1 || static_cast<size_t>(*grfnum) > files.size()) {
				IConsolePrint(CC_WARNING, "GRF number {} out of range, not removing.", *grfnum);
				continue;
			}
			const GRFFile *grf = &files[*grfnum - 1];
			_newgrf_profilers.erase(std::ranges::find(_newgrf_profilers, grf, &NewGRFProfiler::grffile));
		}
		return true;
	}

	/* "start" sub-command */
	if (StrStartsWithIgnoreCase(argv[1], "sta")) {
		std::string grfids;
		size_t started = 0;
		for (NewGRFProfiler &pr : _newgrf_profilers) {
			if (!pr.active) {
				pr.Start();
				started++;

				if (!grfids.empty()) grfids += ", ";
				format_append(grfids, "[{:08X}]", std::byteswap(pr.grffile->grfid));
			}
		}
		if (started > 0) {
			IConsolePrint(CC_DEBUG, "Started profiling for GRFID{} {}.", (started > 1) ? "s" : "", grfids);

			if (argv.size() >= 3) {
				auto ticks = StringConsumer{argv[2]}.TryReadIntegerBase<uint64_t>(0);
				if (!ticks.has_value()) {
					IConsolePrint(CC_ERROR, "No valid amount of ticks was given, profiling will not stop automatically.");
				} else {
					NewGRFProfiler::StartTimer(*ticks);
					IConsolePrint(CC_DEBUG, "Profiling will automatically stop after {} ticks.", *ticks);
				}
			}
		} else if (_newgrf_profilers.empty()) {
			IConsolePrint(CC_ERROR, "No GRFs selected for profiling, did not start.");
		} else {
			IConsolePrint(CC_ERROR, "Did not start profiling for any GRFs, all selected GRFs are already profiling.");
		}
		return true;
	}

	/* "stop" sub-command */
	if (StrStartsWithIgnoreCase(argv[1], "sto")) {
		NewGRFProfiler::FinishAll();
		return true;
	}

	/* "abort" sub-command */
	if (StrStartsWithIgnoreCase(argv[1], "abo")) {
		for (NewGRFProfiler &pr : _newgrf_profilers) {
			pr.Abort();
		}
		NewGRFProfiler::AbortTimer();
		return true;
	}

	return false;
}

#ifdef _DEBUG
/******************
 *  debug commands
 ******************/

static void IConsoleDebugLibRegister()
{
	IConsole::CmdRegister("resettile",        ConResetTile);
	IConsole::AliasRegister("dbg_echo",       "echo %A; echo %B");
	IConsole::AliasRegister("dbg_echo2",      "echo %!");
}
#endif

static bool ConFramerate(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "Show frame rate and game speed information.");
		return true;
	}

	ConPrintFramerate();
	return true;
}

static bool ConFramerateWindow(std::span<std::string_view> argv)
{
	if (argv.empty()) {
		IConsolePrint(CC_HELP, "Open the frame rate window.");
		return true;
	}

	if (_network_dedicated) {
		IConsolePrint(CC_ERROR, "Can not open frame rate window on a dedicated server.");
		return false;
	}

	ShowFramerateWindow();
	return true;
}

/**
 * Format a label as a string.
 * If all elements are visible ASCII (excluding space) then the label will be formatted as a string of 4 characters,
 * otherwise it will be output as an 8-digit hexadecimal value.
 * @param label Label to format.
 * @return string representation of label.
 **/
static std::string FormatLabel(uint32_t label)
{
	if (std::isgraph(GB(label, 24, 8)) && std::isgraph(GB(label, 16, 8)) && std::isgraph(GB(label, 8, 8)) && std::isgraph(GB(label, 0, 8))) {
		return fmt::format("{:c}{:c}{:c}{:c}", GB(label, 24, 8), GB(label, 16, 8), GB(label, 8, 8), GB(label, 0, 8));
	}

	return fmt::format("{:08X}", label);
}

static void ConDumpRoadTypes()
{
	IConsolePrint(CC_DEFAULT, "  Flags:");
	IConsolePrint(CC_DEFAULT, "    c = catenary");
	IConsolePrint(CC_DEFAULT, "    l = no level crossings");
	IConsolePrint(CC_DEFAULT, "    X = no houses");
	IConsolePrint(CC_DEFAULT, "    h = hidden");
	IConsolePrint(CC_DEFAULT, "    T = buildable by towns");

	std::map<uint32_t, const GRFFile *> grfs;
	for (RoadType rt = ROADTYPE_BEGIN; rt < ROADTYPE_END; rt++) {
		const RoadTypeInfo *rti = GetRoadTypeInfo(rt);
		if (rti->label == 0) continue;
		uint32_t grfid = 0;
		const GRFFile *grf = rti->grffile[ROTSG_GROUND];
		if (grf != nullptr) {
			grfid = grf->grfid;
			grfs.emplace(grfid, grf);
		}
		IConsolePrint(CC_DEFAULT, "  {:02d} {} {}, Flags: {}{}{}{}{}, GRF: {:08X}, {}",
				(uint)rt,
				RoadTypeIsTram(rt) ? "Tram" : "Road",
				FormatLabel(rti->label),
				rti->flags.Test(RoadTypeFlag::Catenary)        ? 'c' : '-',
				rti->flags.Test(RoadTypeFlag::NoLevelCrossing) ? 'l' : '-',
				rti->flags.Test(RoadTypeFlag::NoHouses)        ? 'X' : '-',
				rti->flags.Test(RoadTypeFlag::Hidden)          ? 'h' : '-',
				rti->flags.Test(RoadTypeFlag::TownBuild)       ? 'T' : '-',
				std::byteswap(grfid),
				GetStringPtr(rti->strings.name)
		);
	}
	for (const auto &grf : grfs) {
		IConsolePrint(CC_DEFAULT, "  GRF: {:08X} = {}", std::byteswap(grf.first), grf.second->filename);
	}
}

static void ConDumpRailTypes()
{
	IConsolePrint(CC_DEFAULT, "  Flags:");
	IConsolePrint(CC_DEFAULT, "    c = catenary");
	IConsolePrint(CC_DEFAULT, "    l = no level crossings");
	IConsolePrint(CC_DEFAULT, "    h = hidden");
	IConsolePrint(CC_DEFAULT, "    s = no sprite combine");
	IConsolePrint(CC_DEFAULT, "    a = always allow 90 degree turns");
	IConsolePrint(CC_DEFAULT, "    d = always disallow 90 degree turns");

	std::map<uint32_t, const GRFFile *> grfs;
	for (RailType rt = RAILTYPE_BEGIN; rt < RAILTYPE_END; rt++) {
		const RailTypeInfo *rti = GetRailTypeInfo(rt);
		if (rti->label == 0) continue;
		uint32_t grfid = 0;
		const GRFFile *grf = rti->grffile[RTSG_GROUND];
		if (grf != nullptr) {
			grfid = grf->grfid;
			grfs.emplace(grfid, grf);
		}
		IConsolePrint(CC_DEFAULT, "  {:02d} {}, Flags: {}{}{}{}{}{}, GRF: {:08X}, {}",
				(uint)rt,
				FormatLabel(rti->label),
				rti->flags.Test(RailTypeFlag::Catenary)        ? 'c' : '-',
				rti->flags.Test(RailTypeFlag::NoLevelCrossing) ? 'l' : '-',
				rti->flags.Test(RailTypeFlag::Hidden)          ? 'h' : '-',
				rti->flags.Test(RailTypeFlag::NoSpriteCombine) ? 's' : '-',
				rti->flags.Test(RailTypeFlag::Allow90Deg)      ? 'a' : '-',
				rti->flags.Test(RailTypeFlag::Disallow90Deg)   ? 'd' : '-',
				std::byteswap(grfid),
				GetStringPtr(rti->strings.name)
		);
	}
	for (const auto &grf : grfs) {
		IConsolePrint(CC_DEFAULT, "  GRF: {:08X} = {}", std::byteswap(grf.first), grf.second->filename);
	}
}

static void ConDumpCargoTypes()
{
	IConsolePrint(CC_DEFAULT, "  Cargo classes:");
	IConsolePrint(CC_DEFAULT, "    p = passenger");
	IConsolePrint(CC_DEFAULT, "    m = mail");
	IConsolePrint(CC_DEFAULT, "    x = express");
	IConsolePrint(CC_DEFAULT, "    a = armoured");
	IConsolePrint(CC_DEFAULT, "    b = bulk");
	IConsolePrint(CC_DEFAULT, "    g = piece goods");
	IConsolePrint(CC_DEFAULT, "    l = liquid");
	IConsolePrint(CC_DEFAULT, "    r = refrigerated");
	IConsolePrint(CC_DEFAULT, "    h = hazardous");
	IConsolePrint(CC_DEFAULT, "    c = covered/sheltered");
	IConsolePrint(CC_DEFAULT, "    o = oversized");
	IConsolePrint(CC_DEFAULT, "    d = powderized");
	IConsolePrint(CC_DEFAULT, "    n = not pourable");
	IConsolePrint(CC_DEFAULT, "    e = potable");
	IConsolePrint(CC_DEFAULT, "    i = non-potable");
	IConsolePrint(CC_DEFAULT, "    S = special");

	std::map<uint32_t, const GRFFile *> grfs;
	for (const CargoSpec *spec : CargoSpec::Iterate()) {
		uint32_t grfid = 0;
		const GRFFile *grf = spec->grffile;
		if (grf != nullptr) {
			grfid = grf->grfid;
			grfs.emplace(grfid, grf);
		}
		IConsolePrint(CC_DEFAULT, "  {:02d} Bit: {:2d}, Label: {}, Callback mask: 0x{:02X}, Cargo class: {}{}{}{}{}{}{}{}{}{}{}{}{}{}{}{}, GRF: {:08X}, {}",
				spec->Index(),
				spec->bitnum,
				FormatLabel(spec->label.base()),
				spec->callback_mask.base(),
				spec->classes.Test(CargoClass::Passengers)   ? 'p' : '-',
				spec->classes.Test(CargoClass::Mail)         ? 'm' : '-',
				spec->classes.Test(CargoClass::Express)      ? 'x' : '-',
				spec->classes.Test(CargoClass::Armoured)     ? 'a' : '-',
				spec->classes.Test(CargoClass::Bulk)         ? 'b' : '-',
				spec->classes.Test(CargoClass::PieceGoods)   ? 'g' : '-',
				spec->classes.Test(CargoClass::Liquid)       ? 'l' : '-',
				spec->classes.Test(CargoClass::Refrigerated) ? 'r' : '-',
				spec->classes.Test(CargoClass::Hazardous)    ? 'h' : '-',
				spec->classes.Test(CargoClass::Covered)      ? 'c' : '-',
				spec->classes.Test(CargoClass::Oversized)    ? 'o' : '-',
				spec->classes.Test(CargoClass::Powderized)   ? 'd' : '-',
				spec->classes.Test(CargoClass::NotPourable)  ? 'n' : '-',
				spec->classes.Test(CargoClass::Potable)      ? 'e' : '-',
				spec->classes.Test(CargoClass::NonPotable)   ? 'i' : '-',
				spec->classes.Test(CargoClass::Special)      ? 'S' : '-',
				std::byteswap(grfid),
				GetStringPtr(spec->name)
		);
	}
	for (const auto &grf : grfs) {
		IConsolePrint(CC_DEFAULT, "  GRF: {:08X} = {}", std::byteswap(grf.first), grf.second->filename);
	}
}

static bool ConDumpInfo(std::span<std::string_view> argv)
{
	if (argv.size() != 2) {
		IConsolePrint(CC_HELP, "Dump debugging information.");
		IConsolePrint(CC_HELP, "Usage: 'dump_info roadtypes|railtypes|cargotypes'.");
		IConsolePrint(CC_HELP, "  Show information about road/tram types, rail types or cargo types.");
		return true;
	}

	if (StrEqualsIgnoreCase(argv[1], "roadtypes")) {
		ConDumpRoadTypes();
		return true;
	}

	if (StrEqualsIgnoreCase(argv[1], "railtypes")) {
		ConDumpRailTypes();
		return true;
	}

	if (StrEqualsIgnoreCase(argv[1], "cargotypes")) {
		ConDumpCargoTypes();
		return true;
	}

	return false;
}

/*******************************
 * console command registration
 *******************************/

void IConsoleStdLibRegister()
{
	IConsole::CmdRegister("debug_level",             ConDebugLevel);
	IConsole::CmdRegister("echo",                    ConEcho);
	IConsole::CmdRegister("echoc",                   ConEchoC);
	IConsole::CmdRegister("exec",                    ConExec);
	IConsole::CmdRegister("schedule",                ConSchedule);
	IConsole::CmdRegister("exit",                    ConExit);
	IConsole::CmdRegister("part",                    ConPart);
	IConsole::CmdRegister("help",                    ConHelp);
	IConsole::CmdRegister("info_cmd",                ConInfoCmd);
	IConsole::CmdRegister("list_cmds",               ConListCommands);
	IConsole::CmdRegister("list_aliases",            ConListAliases);
	IConsole::CmdRegister("newgame",                 ConNewGame);
	IConsole::CmdRegister("restart",                 ConRestart);
	IConsole::CmdRegister("reload",                  ConReload);
	IConsole::CmdRegister("getseed",                 ConGetSeed);
	IConsole::CmdRegister("getdate",                 ConGetDate);
	IConsole::CmdRegister("getsysdate",              ConGetSysDate);
	IConsole::CmdRegister("quit",                    ConExit);
	IConsole::CmdRegister("resetengines",            ConResetEngines,     ConHookNoNetwork);
	IConsole::CmdRegister("reset_enginepool",        ConResetEnginePool,  ConHookNoNetwork);
	IConsole::CmdRegister("return",                  ConReturn);
	IConsole::CmdRegister("screenshot",              ConScreenShot);
	IConsole::CmdRegister("script",                  ConScript);
	IConsole::CmdRegister("zoomto",                  ConZoomToLevel);
	IConsole::CmdRegister("scrollto",                ConScrollToTile);
	IConsole::CmdRegister("alias",                   ConAlias);
	IConsole::CmdRegister("load",                    ConLoad);
	IConsole::CmdRegister("load_save",               ConLoad);
	IConsole::CmdRegister("load_scenario",           ConLoadScenario);
	IConsole::CmdRegister("load_heightmap",          ConLoadHeightmap);
	IConsole::CmdRegister("rm",                      ConRemove);
	IConsole::CmdRegister("save",                    ConSave);
	IConsole::CmdRegister("saveconfig",              ConSaveConfig);
	IConsole::CmdRegister("ls",                      ConListFiles);
	IConsole::CmdRegister("list_saves",              ConListFiles);
	IConsole::CmdRegister("list_scenarios",          ConListScenarios);
	IConsole::CmdRegister("list_heightmaps",         ConListHeightmaps);
	IConsole::CmdRegister("cd",                      ConChangeDirectory);
	IConsole::CmdRegister("pwd",                     ConPrintWorkingDirectory);
	IConsole::CmdRegister("clear",                   ConClearBuffer);
	IConsole::CmdRegister("font",                    ConFont);
	IConsole::CmdRegister("setting",                 ConSetting);
	IConsole::CmdRegister("setting_newgame",         ConSettingNewgame);
	IConsole::CmdRegister("list_settings",           ConListSettings);
	IConsole::CmdRegister("gamelog",                 ConGamelogPrint);
	IConsole::CmdRegister("rescan_newgrf",           ConRescanNewGRF);
	IConsole::CmdRegister("list_dirs",               ConListDirs);

	IConsole::AliasRegister("dir",                   "ls");
	IConsole::AliasRegister("del",                   "rm %+");
	IConsole::AliasRegister("newmap",                "newgame");
	IConsole::AliasRegister("patch",                 "setting %+");
	IConsole::AliasRegister("set",                   "setting %+");
	IConsole::AliasRegister("set_newgame",           "setting_newgame %+");
	IConsole::AliasRegister("list_patches",          "list_settings %+");
	IConsole::AliasRegister("developer",             "setting developer %+");

	IConsole::CmdRegister("list_ai_libs",            ConListAILibs);
	IConsole::CmdRegister("list_ai",                 ConListAI);
	IConsole::CmdRegister("reload_ai",               ConReloadAI);
	IConsole::CmdRegister("rescan_ai",               ConRescanAI);
	IConsole::CmdRegister("start_ai",                ConStartAI);
	IConsole::CmdRegister("stop_ai",                 ConStopAI);

	IConsole::CmdRegister("list_game",               ConListGame);
	IConsole::CmdRegister("list_game_libs",          ConListGameLibs);
	IConsole::CmdRegister("rescan_game",             ConRescanGame);

	IConsole::CmdRegister("companies",               ConCompanies);
	IConsole::AliasRegister("players",               "companies");

	/* networking functions */

/* Content downloading is only available with ZLIB */
#if defined(WITH_ZLIB)
	IConsole::CmdRegister("content",                 ConContent);
#endif /* defined(WITH_ZLIB) */

	/*** Networking commands ***/
	IConsole::CmdRegister("say",                     ConSay,              ConHookNeedNetwork);
	IConsole::CmdRegister("say_company",             ConSayCompany,       ConHookNeedNetwork);
	IConsole::AliasRegister("say_player",            "say_company %+");
	IConsole::CmdRegister("say_client",              ConSayClient,        ConHookNeedNetwork);

	IConsole::CmdRegister("connect",                 ConNetworkConnect,   ConHookClientOnly);
	IConsole::CmdRegister("clients",                 ConNetworkClients,   ConHookNeedNetwork);
	IConsole::CmdRegister("status",                  ConStatus,           ConHookServerOnly);
	IConsole::CmdRegister("server_info",             ConServerInfo,       ConHookServerOnly);
	IConsole::AliasRegister("info",                  "server_info");
	IConsole::CmdRegister("reconnect",               ConNetworkReconnect, ConHookClientOnly);
	IConsole::CmdRegister("rcon",                    ConRcon,             ConHookNeedNetwork);

	IConsole::CmdRegister("join",                    ConJoinCompany,      ConHookNeedNonDedicatedNetwork);
	IConsole::AliasRegister("spectate",              "join 255");
	IConsole::CmdRegister("move",                    ConMoveClient,       ConHookServerOnly);
	IConsole::CmdRegister("reset_company",           ConResetCompany,     ConHookServerOnly);
	IConsole::AliasRegister("clean_company",         "reset_company %A");
	IConsole::CmdRegister("client_name",             ConClientNickChange, ConHookServerOnly);
	IConsole::CmdRegister("kick",                    ConKick,             ConHookServerOnly);
	IConsole::CmdRegister("ban",                     ConBan,              ConHookServerOnly);
	IConsole::CmdRegister("unban",                   ConUnBan,            ConHookServerOnly);
	IConsole::CmdRegister("banlist",                 ConBanList,          ConHookServerOnly);

	IConsole::CmdRegister("pause",                   ConPauseGame,        ConHookServerOrNoNetwork);
	IConsole::CmdRegister("unpause",                 ConUnpauseGame,      ConHookServerOrNoNetwork);

	IConsole::CmdRegister("authorized_key", ConNetworkAuthorizedKey, ConHookServerOnly);
	IConsole::AliasRegister("ak", "authorized_key %+");

	IConsole::AliasRegister("net_frame_freq",        "setting frame_freq %+");
	IConsole::AliasRegister("net_sync_freq",         "setting sync_freq %+");
	IConsole::AliasRegister("server_pw",             "setting server_password %+");
	IConsole::AliasRegister("server_password",       "setting server_password %+");
	IConsole::AliasRegister("rcon_pw",               "setting rcon_password %+");
	IConsole::AliasRegister("rcon_password",         "setting rcon_password %+");
	IConsole::AliasRegister("name",                  "setting client_name %+");
	IConsole::AliasRegister("server_name",           "setting server_name %+");
	IConsole::AliasRegister("server_port",           "setting server_port %+");
	IConsole::AliasRegister("max_clients",           "setting max_clients %+");
	IConsole::AliasRegister("max_companies",         "setting max_companies %+");
	IConsole::AliasRegister("max_join_time",         "setting max_join_time %+");
	IConsole::AliasRegister("pause_on_join",         "setting pause_on_join %+");
	IConsole::AliasRegister("autoclean_companies",   "setting autoclean_companies %+");
	IConsole::AliasRegister("autoclean_protected",   "setting autoclean_protected %+");
	IConsole::AliasRegister("restart_game_year",     "setting restart_game_year %+");
	IConsole::AliasRegister("min_players",           "setting min_active_clients %+");
	IConsole::AliasRegister("reload_cfg",            "setting reload_cfg %+");

	/* debugging stuff */
#ifdef _DEBUG
	IConsoleDebugLibRegister();
#endif
	IConsole::CmdRegister("fps",                     ConFramerate);
	IConsole::CmdRegister("fps_wnd",                 ConFramerateWindow);

	/* NewGRF development stuff */
	IConsole::CmdRegister("reload_newgrfs",          ConNewGRFReload,     ConHookNewGRFDeveloperTool);
	IConsole::CmdRegister("newgrf_profile",          ConNewGRFProfile,    ConHookNewGRFDeveloperTool);

	IConsole::CmdRegister("dump_info",               ConDumpInfo);
}
