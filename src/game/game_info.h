/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file game_info.h GameInfo keeps track of all information of an Game, like Author, Description, ... */

#ifndef GAME_INFO_HPP
#define GAME_INFO_HPP

#include "../script/script_info.h"

/** All static information from an Game like name, version, etc. */
class GameInfo : public ScriptInfo {
public:
	GameInfo();

	/**
	 * Register the functions of this class.
	 */
	static void RegisterAPI(Squirrel *engine);

	/**
	 * Create an Game, using this GameInfo as start-template.
	 */
	static SQInteger Constructor(HSQUIRRELVM vm);

	/**
	 * Check if we can start this Game.
	 */
	bool CanLoadFromVersion(int version) const;

	/**
	 * Get the API version this Game is written for.
	 */
	const std::string &GetAPIVersion() const { return this->api_version; }

	bool IsDeveloperOnly() const override { return this->is_developer_only; }

private:
	int min_loadable_version; ///< The Game can load savegame data if the version is equal or greater than this.
	bool is_developer_only;   ///< Is the script selectable by non-developers?
	std::string api_version;  ///< API version used by this Game.
};

/** All static information from an Game library like name, version, etc. */
class GameLibrary : public ScriptInfo {
public:
	GameLibrary() : ScriptInfo() {};

	/**
	 * Register the functions of this class.
	 */
	static void RegisterAPI(Squirrel *engine);

	/**
	 * Create an GSLibrary, using this GSInfo as start-template.
	 */
	static SQInteger Constructor(HSQUIRRELVM vm);

	/**
	 * Get the category this library is in.
	 */
	const std::string &GetCategory() const { return this->category; }

private:
	std::string category; ///< The category this library is in.
};

#endif /* GAME_INFO_HPP */
