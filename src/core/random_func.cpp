/* $Id$ */

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
#include "../network/network_data.h"
#include "../variables.h" /* _frame_counter */
#include "../player_func.h"

uint32 DoRandom(int line, const char *file)
{
	if (_networking && (DEREF_CLIENT(0)->status != STATUS_INACTIVE || !_network_server)) {
		printf("Random [%d/%d] %s:%d\n",_frame_counter, (byte)_current_player, file, line);
	}

	return _random.Next();
}

uint DoRandomRange(uint max, int line, const char *file)
{
	return GB(DoRandom(line, file), 0, 16) * max >> 16;
}
#endif /* RANDOM_DEBUG */
