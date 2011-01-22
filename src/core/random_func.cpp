/* $Id$ */

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
 * Generate the next pseudo random number scaled to max
 * @param max the maximum value of the returned random number
 * @return the random number
 */
uint32 Randomizer::Next(uint32 max)
{
	return ((uint64)this->Next() * (uint64)max) >> 32;
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
#include "../network/network.h"
#include "../network/network_server.h"
#include "../network/network_internal.h"
#include "../company_func.h"

uint32 DoRandom(int line, const char *file)
{
	if (_networking && (!_network_server || (NetworkClientSocket::IsValidID(0) && NetworkClientSocket::Get(0)->status != NetworkClientSocket::STATUS_INACTIVE))) {
		printf("Random [%d/%d] %s:%d\n", _frame_counter, (byte)_current_company, file, line);
	}

	return _random.Next();
}

uint32 DoRandomRange(uint32 max, int line, const char *file)
{
	return ((uint64)DoRandom(line, file) * (uint64)max) >> 32;
}
#endif /* RANDOM_DEBUG */
