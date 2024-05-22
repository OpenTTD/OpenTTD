/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_newgrf.hpp NewGRF info for scripts. */

#ifndef SCRIPT_NEWGRF_HPP
#define SCRIPT_NEWGRF_HPP

#include "script_list.hpp"

/**
 * Create a list of loaded NewGRFs.
 * @api ai game
 * @ingroup ScriptList
 */
class ScriptNewGRFList : public ScriptList {
public:
	ScriptNewGRFList();
};


/**
 * Class that handles all NewGRF related functions.
 * @api ai game
 */
class ScriptNewGRF : public ScriptObject {
public:
	/**
	 * Check if a NewGRF with a given grfid is loaded.
	 * @param grfid The grfid to check.
	 * @return True if and only if a NewGRF with the given grfid is loaded in the game.
	 */
	static bool IsLoaded(SQInteger grfid);

	/**
	 * Get the version of a loaded NewGRF.
	 * @param grfid The NewGRF to query.
	 * @pre ScriptNewGRF::IsLoaded(grfid).
	 * @return Version of the NewGRF or 0 if the NewGRF specifies no version.
	 */
	static SQInteger GetVersion(SQInteger grfid);

	/**
	 * Get the name of a loaded NewGRF.
	 * @param grfid The NewGRF to query.
	 * @pre ScriptNewGRF::IsLoaded(grfid).
	 * @return The name of the NewGRF or null if no name is defined.
	 */
	static std::optional<std::string> GetName(SQInteger grfid);
};

#endif /* SCRIPT_NEWGRF_HPP */
