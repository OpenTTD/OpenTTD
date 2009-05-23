/* $Id$ */

/** @file ai_signlist.cpp Implementation of AISignList and friends. */

#include "ai_signlist.hpp"
#include "ai_sign.hpp"
#include "../../signs_base.h"

AISignList::AISignList()
{
	Sign *s;
	FOR_ALL_SIGNS(s) {
		if (AISign::IsValidSign(s->index)) this->AddItem(s->index);
	}
}
