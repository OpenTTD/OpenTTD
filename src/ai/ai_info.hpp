/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_info.hpp AIInfo keeps track of all information of an AI, like Author, Description, ... */

#ifndef AI_INFO_HPP
#define AI_INFO_HPP

#include "../script/script_info.hpp"

/** All static information from an AI like name, version, etc. */
class AIInfo : public ScriptInfo {
public:
	/* All valid AI API versions, in order. */
	static constexpr std::string_view ApiVersions[]{ "0.7", "1.0", "1.1", "1.2", "1.3", "1.4", "1.5", "1.6", "1.7", "1.8", "1.9", "1.10", "1.11", "12", "13", "14", "15" };

	AIInfo();

	/**
	 * Register the functions of this class.
	 */
	static void RegisterAPI(Squirrel *engine);

	/**
	 * Create an AI, using this AIInfo as start-template.
	 */
	static SQInteger Constructor(HSQUIRRELVM vm);

	/**
	 * Create a dummy-AI.
	 */
	static SQInteger DummyConstructor(HSQUIRRELVM vm);

	/**
	 * Check if we can start this AI.
	 */
	bool CanLoadFromVersion(int version) const;

	/**
	 * Use this AI as a random AI.
	 */
	bool UseAsRandomAI() const { return this->use_as_random; }

	/**
	 * Get the API version this AI is written for.
	 */
	const std::string &GetAPIVersion() const { return this->api_version; }

private:
	int min_loadable_version; ///< The AI can load savegame data if the version is equal or greater than this.
	bool use_as_random;       ///< Should this AI be used when the user wants a "random AI"?
	std::string api_version;  ///< API version used by this AI.
};

/** All static information from an AI library like name, version, etc. */
class AILibrary : public ScriptInfo {
public:
	AILibrary() : ScriptInfo() {};

	/**
	 * Register the functions of this class.
	 */
	static void RegisterAPI(Squirrel *engine);

	/**
	 * Create an AI, using this AIInfo as start-template.
	 */
	static SQInteger Constructor(HSQUIRRELVM vm);

	/**
	 * Get the category this library is in.
	 */
	const std::string &GetCategory() const { return this->category; }

private:
	std::string category; ///< The category this library is in.
};

#endif /* AI_INFO_HPP */
