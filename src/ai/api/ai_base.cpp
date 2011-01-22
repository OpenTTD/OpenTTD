/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_base.cpp Implementation of AIBase. */

#include "../../stdafx.h"
#include "ai_base.hpp"
#include "../../network/network.h"
#include "../../core/random_func.hpp"

/* static */ uint32 AIBase::Rand()
{
	/* We pick RandomRange if we are in SP (so when saved, we do the same over and over)
	 *   but we pick InteractiveRandomRange if we are a network_server or network-client. */
	if (_networking) return ::InteractiveRandom();
	return ::Random();
}

/* static */ uint32 AIBase::RandItem(int unused_param)
{
	return AIBase::Rand();
}

/* static */ uint AIBase::RandRange(uint max)
{
	/* We pick RandomRange if we are in SP (so when saved, we do the same over and over)
	 *   but we pick InteractiveRandomRange if we are a network_server or network-client. */
	if (_networking) return ::InteractiveRandomRange(max);
	return ::RandomRange(max);
}

/* static */ uint32 AIBase::RandRangeItem(int unused_param, uint max)
{
	return AIBase::RandRange(max);
}

/* static */ bool AIBase::Chance(uint out, uint max)
{
	return (uint16)Rand() <= (uint16)((65536 * out) / max);
}

/* static */ bool AIBase::ChanceItem(int unused_param, uint out, uint max)
{
	return AIBase::Chance(out, max);
}
