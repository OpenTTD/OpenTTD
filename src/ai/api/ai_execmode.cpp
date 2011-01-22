/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_execmode.cpp Implementation of AIExecMode. */

#include "../../stdafx.h"
#include "ai_execmode.hpp"
#include "../../company_base.h"
#include "../../company_func.h"
#include "../ai_instance.hpp"

bool AIExecMode::ModeProc()
{
	/* In execution mode we only return 'true', telling the DoCommand it
	 *  should continue with the real execution of the command. */
	return true;
}

AIExecMode::AIExecMode()
{
	this->last_mode     = this->GetDoCommandMode();
	this->last_instance = this->GetDoCommandModeInstance();
	this->SetDoCommandMode(&AIExecMode::ModeProc, this);
}

AIExecMode::~AIExecMode()
{
	if (this->GetDoCommandModeInstance() != this) {
		AIInstance *instance = Company::Get(_current_company)->ai_instance;
		/* Ignore this error if the AI already died. */
		if (!instance->IsDead()) {
			throw AI_FatalError("AIExecMode object was removed while it was not the latest AI*Mode object created.");
		}
	}
	this->SetDoCommandMode(this->last_mode, this->last_instance);
}
