/* $Id$ */

/** @file ai_execmode.cpp Implementation of AIExecMode. */

#include "ai_execmode.hpp"
#include "../../command_type.h"
#include "../../company_base.h"
#include "../../company_func.h"
#include "../ai_instance.hpp"

bool AIExecMode::ModeProc(TileIndex tile, uint32 p1, uint32 p2, uint procc, CommandCost costs)
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
