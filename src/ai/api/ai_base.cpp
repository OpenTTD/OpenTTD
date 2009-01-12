/* $Id$ */

/** @file ai_base.cpp Implementation of AIBase. */

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
