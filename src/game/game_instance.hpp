/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file game_instance.hpp The GameInstance tracks games. */

#ifndef GAME_INSTANCE_HPP
#define GAME_INSTANCE_HPP

#include "../script/script_instance.hpp"

/** Runtime information about a game script like a pointer to the squirrel vm and the current state. */
class GameInstance : public ScriptInstance {
public:
	GameInstance();

	/**
	 * Initialize the script and prepare it for its first run.
	 * @param info The GameInfo to start.
	 */
	void Initialize(class GameInfo *info);

	int GetSetting(const char *name) override;
	ScriptInfo *FindLibrary(const char *library, int version) override;

private:
	void RegisterAPI() override;
	void Died() override;
	CommandCallback *GetDoCommandCallback() override;
	void LoadDummyScript() override {}
};

#endif /* GAME_INSTANCE_HPP */
