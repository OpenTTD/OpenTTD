/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_testmode.cpp Implementation of AITestMode. */

#include "../../stdafx.h"
#include "ai_testmode.hpp"
#include "../../company_base.h"
#include "../../company_func.h"
#include "../ai_instance.hpp"

bool AITestMode::ModeProc()
{
	/* In test mode we only return 'false', telling the DoCommand it
	 *  should stop after testing the command and return with that result. */
	return false;
}

AITestMode::AITestMode()
{
	this->last_mode     = this->GetDoCommandMode();
	this->last_instance = this->GetDoCommandModeInstance();
	this->SetDoCommandMode(&AITestMode::ModeProc, this);
}

AITestMode::~AITestMode()
{
	if (this->GetDoCommandModeInstance() != this) {
		AIInstance *instance = Company::Get(_current_company)->ai_instance;
		/* Ignore this error if the AI already died. */
		if (!instance->IsDead()) {
			throw AI_FatalError("AITestmode object was removed while it was not the latest AI*Mode object created.");
		}
	}
	this->SetDoCommandMode(this->last_mode, this->last_instance);
}
