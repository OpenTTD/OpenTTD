/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_bridgelist.cpp Implementation of AIBridgeList and friends. */

#include "../../stdafx.h"
#include "ai_bridgelist.hpp"
#include "ai_bridge.hpp"
#include "../../bridge.h"

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
