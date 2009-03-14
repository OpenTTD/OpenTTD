/* $Id$ */

/** @file ai_testmode.cpp Implementation of AITestMode. */

#include "ai_testmode.hpp"
#include "../../command_type.h"

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
	assert(this->GetDoCommandModeInstance() == this);
	this->SetDoCommandMode(this->last_mode, this->last_instance);
}
