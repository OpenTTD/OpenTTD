/* $Id$ */

/** @file ai_bridgelist.cpp Implementation of AIBridgeList and friends. */

#include "ai_bridgelist.hpp"
#include "ai_bridge.hpp"
#include "../../openttd.h"
#include "../../bridge.h"
#include "../../date_func.h"

AIBridgeList::AIBridgeList()
{
	/* Add all bridges, no matter if they are available or not */
	for (byte j = 0; j < MAX_BRIDGES; j++)
		if (::GetBridgeSpec(j)->avail_year <= _cur_year)
			this->AddItem(j);
}

AIBridgeList_Length::AIBridgeList_Length(uint length)
{
	for (byte j = 0; j < MAX_BRIDGES; j++)
		if (::GetBridgeSpec(j)->avail_year <= _cur_year)
			if (length >= (uint)AIBridge::GetMinLength(j) && length <= (uint)AIBridge::GetMaxLength(j))
				this->AddItem(j);
}
