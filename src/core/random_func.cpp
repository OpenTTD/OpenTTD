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

uint32 Randomizer::Next()
{
	const uint32 s = this->state[0];
	const uint32 t = this->state[1];

	this->state[0] = s + ROR(t ^ 0x1234567F, 7) + 1;
	return this->state[1] = ROR(s, 3) - 1;
}

uint32 Randomizer::Next(uint16 max)
{
	return GB(this->Next(), 0, 16) * max >> 16;
}

void Randomizer::SetSeed(uint32 seed)
{
	this->state[0] = seed;
	this->state[1] = seed;
}

void SetRandomSeed(uint32 seed)
{
	_random.SetSeed(seed);
	_interactive_random.SetSeed(seed * 0x1234567);
}

#ifdef RANDOM_DEBUG
#include "../network/network_internal.h"
#include "../company_func.h"

uint32 DoRandom(int line, const char *file)
{
	if (_networking && (!_network_server || (NetworkClientSocket::IsValidID(0) && NetworkClientSocket::Get(0)->status != STATUS_INACTIVE))) {
		printf("Random [%d/%d] %s:%d\n", _frame_counter, (byte)_current_company, file, line);
	}

	return _random.Next();
}

uint DoRandomRange(uint max, int line, const char *file)
{
	return GB(DoRandom(line, file), 0, 16) * max >> 16;
}
#endif /* RANDOM_DEBUG */
