/* $Id$ */

/** @file ai_execmode.cpp Implementation of AIExecMode. */

#include "ai_execmode.hpp"
#include "../../command_type.h"

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
	assert(this->GetDoCommandModeInstance() == this);
	this->SetDoCommandMode(this->last_mode, this->last_instance);
}
