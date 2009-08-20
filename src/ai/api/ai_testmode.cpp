/* $Id$ */

/** @file ai_testmode.cpp Implementation of AITestMode. */

#include "ai_testmode.hpp"
#include "../../command_type.h"
#include "../../company_base.h"
#include "../../company_func.h"
#include "../ai_instance.hpp"

bool AITestMode::ModeProc(TileIndex tile, uint32 p1, uint32 p2, uint procc, CommandCost costs)
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
