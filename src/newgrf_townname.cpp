/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/**
 * @file newgrf_townname.cpp Implementation of  Action 0F "universal holder" structure and functions.
 *
 * This file implements a linked-lists of townname generators,
 * holding everything that the newgrf action 0F will send over to OpenTTD.
 */

#include "stdafx.h"
#include "newgrf_townname.h"
#include "string_func.h"
#include "strings_internal.h"

#include "table/strings.h"

#include "safeguards.h"

static std::vector<GRFTownName> _grf_townnames;
static std::vector<StringID> _grf_townname_names;

/**
 * Get the \c GRFTownName for the given GRFID.
 * @param grfid The GRFID of the town name.
 * @return The \c GRFTownName or \c nullptr if it does not exist.
 */
GRFTownName *GetGRFTownName(GrfID grfid)
{
	auto found = std::ranges::find(_grf_townnames, grfid, &GRFTownName::grfid);
	if (found != std::end(_grf_townnames)) return &*found;
	return nullptr;
}

/**
 * Get the \c GRFTownName for the given GRFID or allocate one if it does not exist.
 * @param grfid The GRFID of the town name.
 * @return The \c GRFTownName.
 */
GRFTownName *AddGRFTownName(GrfID grfid)
{
	GRFTownName *t = GetGRFTownName(grfid);
	if (t == nullptr) {
		t = &_grf_townnames.emplace_back();
		t->grfid = grfid;
	}
	return t;
}

/**
 * Remove the \c GRFTownName mapping for the given GRFID.
 * @param grfid The NewGRF to remove for.
 */
void DelGRFTownName(GrfID grfid)
{
	_grf_townnames.erase(std::ranges::find(_grf_townnames, grfid, &GRFTownName::grfid));
}

static void RandomPart(StringBuilder &builder, const GRFTownName *t, uint32_t seed, uint8_t id)
{
	assert(t != nullptr);
	for (const auto &partlist : t->partlists[id]) {
		uint8_t count = partlist.bitcount;
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

/**
 * Construct a NewGRF town name into the builder.
 * @param builder The string builder to write to.
 * @param grfid The NewGRF providing the information/logic.
 * @param gen The town name style to get from the NewGRF.
 * @param seed The random 32 bit number to generate the town name for.
 */
void GRFTownNameGenerate(StringBuilder &builder, GrfID grfid, uint16_t gen, uint32_t seed)
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

const std::vector<StringID> &GetGRFTownNameList()
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

GrfID GetGRFTownNameId(uint16_t gen)
{
	for (const auto &t : _grf_townnames) {
		if (gen < t.styles.size()) return t.grfid;
		gen -= static_cast<uint16_t>(t.styles.size());
	}
	/* Fallback to no NewGRF */
	return {};
}

uint16_t GetGRFTownNameType(uint16_t gen)
{
	for (const auto &t : _grf_townnames) {
		if (gen < t.styles.size()) return gen;
		gen -= static_cast<uint16_t>(t.styles.size());
	}
	/* Fallback to the first built in town name (English). */
	return SPECSTR_TOWNNAME_START;
}
