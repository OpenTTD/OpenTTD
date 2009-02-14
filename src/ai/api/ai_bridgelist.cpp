/* $Id$ */

/** @file ai_bridgelist.cpp Implementation of AIBridgeList and friends. */

#include "ai_bridgelist.hpp"
#include "ai_bridge.hpp"
#include "../../bridge.h"
#include "../../date_func.h"

AIBridgeList::AIBridgeList()
{
	for (byte j = 0; j < MAX_BRIDGES; j++) {
		if (AIBridge::IsValidBridge(j)) this->AddItem(j);
	}
}

AIBridgeList_Length::AIBridgeList_Length(uint length)
{
	for (byte j = 0; j < MAX_BRIDGES; j++) {
		if (AIBridge::IsValidBridge(j)) {
			if (length >= (uint)AIBridge::GetMinLength(j) && length <= (uint)AIBridge::GetMaxLength(j)) this->AddItem(j);
		}
	}
}
