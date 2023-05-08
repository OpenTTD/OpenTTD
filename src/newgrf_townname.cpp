/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file newgrf_townname.cpp
 * Implementation of  Action 0F "universal holder" structure and functions.
 * This file implements a linked-lists of townname generators,
 * holding everything that the newgrf action 0F will send over to OpenTTD.
 */

#include "stdafx.h"
#include "newgrf_townname.h"
#include "core/alloc_func.hpp"
#include "string_func.h"
#include "strings_internal.h"

#include "table/strings.h"

#include "safeguards.h"

static std::vector<GRFTownName> _grf_townnames;
static std::vector<StringID> _grf_townname_names;

GRFTownName *GetGRFTownName(uint32_t grfid)
{
	auto found = std::find_if(std::begin(_grf_townnames), std::end(_grf_townnames), [&grfid](const GRFTownName &t){ return t.grfid == grfid; });
	if (found != std::end(_grf_townnames)) return &*found;
	return nullptr;
}

GRFTownName *AddGRFTownName(uint32_t grfid)
{
	GRFTownName *t = GetGRFTownName(grfid);
	if (t == nullptr) {
		t = &_grf_townnames.emplace_back();
		t->grfid = grfid;
	}
	return t;
}

void DelGRFTownName(uint32_t grfid)
{
	_grf_townnames.erase(std::find_if(std::begin(_grf_townnames), std::end(_grf_townnames), [&grfid](const GRFTownName &t){ return t.grfid == grfid; }));
}

static void RandomPart(StringBuilder &builder, const GRFTownName *t, uint32_t seed, byte id)
{
	assert(t != nullptr);
	for (const auto &partlist : t->partlists[id]) {
		byte count = partlist.bitcount;
		uint16_t maxprob = partlist.maxprob;
		uint32_t r = (GB(seed, partlist.bitstart, count) * maxprob) >> count;
		for (const auto &part : partlist.parts) {
			maxprob -= GB(part.prob, 0, 7);
			if (maxprob > r) continue;
			if (HasBit(part.prob, 7)) {
				RandomPart(builder, t, seed, part.id);
			} else {
				builder += part.text;
			}
			break;
		}
	}
}

void GRFTownNameGenerate(StringBuilder &builder, uint32_t grfid, uint16_t gen, uint32_t seed)
{
	const GRFTownName *t = GetGRFTownName(grfid);
	if (t != nullptr) {
		assert(gen < t->styles.size());
		RandomPart(builder, t, seed, t->styles[gen].id);
	}
}


/** Allocate memory for the NewGRF town names. */
void InitGRFTownGeneratorNames()
{
	_grf_townname_names.clear();
	for (const auto &t : _grf_townnames) {
		for (const auto &style : t.styles) {
			_grf_townname_names.push_back(style.name);
		}
	}
}

const std::vector<StringID>& GetGRFTownNameList()
{
	return _grf_townname_names;
}

StringID GetGRFTownNameName(uint16_t gen)
{
	return gen < _grf_townname_names.size() ? _grf_townname_names[gen] : STR_UNDEFINED;
}

void CleanUpGRFTownNames()
{
	_grf_townnames.clear();
}

uint32_t GetGRFTownNameId(uint16_t gen)
{
	for (const auto &t : _grf_townnames) {
		if (gen < t.styles.size()) return t.grfid;
		gen -= static_cast<uint16_t>(t.styles.size());
	}
	/* Fallback to no NewGRF */
	return 0;
}

uint16_t GetGRFTownNameType(uint16_t gen)
{
	for (const auto &t : _grf_townnames) {
		if (gen < t.styles.size()) return gen;
		gen -= static_cast<uint16_t>(t.styles.size());
	}
	/* Fallback to english original */
	return SPECSTR_TOWNNAME_ENGLISH;
}
