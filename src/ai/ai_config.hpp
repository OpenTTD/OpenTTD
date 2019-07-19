/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_config.hpp AIConfig stores the configuration settings of every AI. */

#ifndef AI_CONFIG_HPP
#define AI_CONFIG_HPP

#include "../script/script_config.hpp"
#include "../company_type.h"

class AIConfig : public ScriptConfig {
public:
	/**
	 * Get the config of a company.
	 */
	static AIConfig *GetConfig(CompanyID company, ScriptSettingSource source = SSS_DEFAULT);

	AIConfig() :
		ScriptConfig()
	{}

	AIConfig(const AIConfig *config) :
		ScriptConfig(config)
	{}

	class AIInfo *GetInfo() const;

	/**
	 * When ever the AI Scanner is reloaded, all infos become invalid. This
	 *  function tells AIConfig about this.
	 * @param force_exact_match If true try to find the exact same version
	 *   as specified. If false any version is ok.
	 * @return \c true if the reset was successful, \c false if the AI was no longer
	 *  found.
	 */
	bool ResetInfo(bool force_exact_match);

protected:
	ScriptInfo *FindInfo(const char *name, int version, bool force_exact_match) override;
};

#endif /* AI_CONFIG_HPP */
