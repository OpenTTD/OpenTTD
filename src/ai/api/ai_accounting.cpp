/* $Id$ */

/** @file ai_accounting.cpp Implementation of AIAccounting. */

#include "ai_accounting.hpp"

Money AIAccounting::GetCosts()
{
	return this->GetDoCommandCosts();
}

void AIAccounting::ResetCosts()
{
	this->SetDoCommandCosts(0);
}

AIAccounting::AIAccounting()
{
	this->last_costs = this->GetDoCommandCosts();
	this->SetDoCommandCosts(0);
}

AIAccounting::~AIAccounting()
{
	this->SetDoCommandCosts(this->last_costs);
}
