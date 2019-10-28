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

#include "safeguards.h"

static GRFTownName *_grf_townnames = nullptr;

GRFTownName *GetGRFTownName(uint32 grfid)
{
	GRFTownName *t = _grf_townnames;
	for (; t != nullptr; t = t->next) {
		if (t->grfid == grfid) return t;
	}
	return nullptr;
}

GRFTownName *AddGRFTownName(uint32 grfid)
{
	GRFTownName *t = GetGRFTownName(grfid);
	if (t == nullptr) {
		t = CallocT<GRFTownName>(1);
		t->grfid = grfid;
		t->next = _grf_townnames;
		_grf_townnames = t;
	}
	return t;
}

void DelGRFTownName(uint32 grfid)
{
	GRFTownName *t = _grf_townnames;
	GRFTownName *p = nullptr;
	for (;t != nullptr; p = t, t = t->next) if (t->grfid == grfid) break;
	if (t != nullptr) {
		for (int i = 0; i < 128; i++) {
			for (int j = 0; j < t->nbparts[i]; j++) {
				for (int k = 0; k < t->partlist[i][j].partcount; k++) {
					if (!HasBit(t->partlist[i][j].parts[k].prob, 7)) free(t->partlist[i][j].parts[k].data.text);
				}
				free(t->partlist[i][j].parts);
			}
			free(t->partlist[i]);
		}
		if (p != nullptr) {
			p->next = t->next;
		} else {
			_grf_townnames = t->next;
		}
		free(t);
	}
}

static char *RandomPart(char *buf, GRFTownName *t, uint32 seed, byte id, const char *last)
{
	assert(t != nullptr);
	for (int i = 0; i < t->nbparts[id]; i++) {
		byte count = t->partlist[id][i].bitcount;
		uint16 maxprob = t->partlist[id][i].maxprob;
		uint32 r = (GB(seed, t->partlist[id][i].bitstart, count) * maxprob) >> count;
		for (int j = 0; j < t->partlist[id][i].partcount; j++) {
			byte prob = t->partlist[id][i].parts[j].prob;
			maxprob -= GB(prob, 0, 7);
			if (maxprob > r) continue;
			if (HasBit(prob, 7)) {
				buf = RandomPart(buf, t, seed, t->partlist[id][i].parts[j].data.id, last);
			} else {
				buf = strecat(buf, t->partlist[id][i].parts[j].data.text, last);
			}
			break;
		}
	}
	return buf;
}

char *GRFTownNameGenerate(char *buf, uint32 grfid, uint16 gen, uint32 seed, const char *last)
{
	strecpy(buf, "", last);
	for (GRFTownName *t = _grf_townnames; t != nullptr; t = t->next) {
		if (t->grfid == grfid) {
			assert(gen < t->nb_gen);
			buf = RandomPart(buf, t, seed, t->id[gen], last);
			break;
		}
	}
	return buf;
}

StringID *GetGRFTownNameList()
{
	int nb_names = 0, n = 0;
	for (GRFTownName *t = _grf_townnames; t != nullptr; t = t->next) nb_names += t->nb_gen;
	StringID *list = MallocT<StringID>(nb_names + 1);
	for (GRFTownName *t = _grf_townnames; t != nullptr; t = t->next) {
		for (int j = 0; j < t->nb_gen; j++) list[n++] = t->name[j];
	}
	list[n] = INVALID_STRING_ID;
	return list;
}

void CleanUpGRFTownNames()
{
	while (_grf_townnames != nullptr) DelGRFTownName(_grf_townnames->grfid);
}

uint32 GetGRFTownNameId(int gen)
{
	for (GRFTownName *t = _grf_townnames; t != nullptr; t = t->next) {
		if (gen < t->nb_gen) return t->grfid;
		gen -= t->nb_gen;
	}
	/* Fallback to no NewGRF */
	return 0;
}

uint16 GetGRFTownNameType(int gen)
{
	for (GRFTownName *t = _grf_townnames; t != nullptr; t = t->next) {
		if (gen < t->nb_gen) return gen;
		gen -= t->nb_gen;
	}
	/* Fallback to english original */
	return SPECSTR_TOWNNAME_ENGLISH;
}
