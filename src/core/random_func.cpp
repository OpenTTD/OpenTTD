/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file random_func.cpp Implementation of the pseudo random generator. */

#include "../stdafx.h"
#include "random_func.hpp"
#include "bitmath_func.hpp"

#ifdef RANDOM_DEBUG
#include "../network/network.h"
#include "../network/network_server.h"
#include "../network/network_internal.h"
#include "../company_func.h"
#include "../fileio_func.h"
#include "../date_func.h"
#endif /* RANDOM_DEBUG */

#include "../safeguards.h"

Randomizer _random, _interactive_random;

/**
 * Generate the next pseudo random number
 * @return the random number
 */
uint32 Randomizer::Next()
{
	const uint32 s = this->state[0];
	const uint32 t = this->state[1];

	this->state[0] = s + ROR(t ^ 0x1234567F, 7) + 1;
	return this->state[1] = ROR(s, 3) - 1;
}

/**
 * Generate the next pseudo random number scaled to \a limit, excluding \a limit
 * itself.
 * @param limit Limit of the range to be generated from.
 * @return Random number in [0,\a limit)
 */
uint32 Randomizer::Next(uint32 limit)
{
	return ((uint64)this->Next() * (uint64)limit) >> 32;
}

/**
 * (Re)set the state of the random number generator.
 * @param seed the new state
 */
void Randomizer::SetSeed(uint32 seed)
{
	this->state[0] = seed;
	this->state[1] = seed;
}

/**
 * (Re)set the state of the random number generators.
 * @param seed the new state
 */
void SetRandomSeed(uint32 seed)
{
	_random.SetSeed(seed);
	_interactive_random.SetSeed(seed * 0x1234567);
}

#ifdef RANDOM_DEBUG
uint32 DoRandom(int line, const char *file)
{
	if (_networking && (!_network_server || (NetworkClientSocket::IsValidID(0) && NetworkClientSocket::Get(0)->status != NetworkClientSocket::STATUS_INACTIVE))) {
		DEBUG(random, 0, "%08x; %02x; %04x; %02x; %s:%d", _date, _date_fract, _frame_counter, (byte)_current_company, file, line);
	}

	return _random.Next();
}

uint32 DoRandomRange(uint32 limit, int line, const char *file)
{
	return ((uint64)DoRandom(line, file) * (uint64)limit) >> 32;
}
#endif /* RANDOM_DEBUG */
