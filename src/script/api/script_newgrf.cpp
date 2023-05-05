/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_newgrf.cpp Implementation of ScriptNewGRF and friends. */

#include "../../stdafx.h"
#include "script_newgrf.hpp"
#include "../../core/bitmath_func.hpp"
#include "../../newgrf_config.h"
#include "../../string_func.h"

#include "../../safeguards.h"

ScriptNewGRFList::ScriptNewGRFList()
{
	for (auto c = _grfconfig; c != nullptr; c = c->next) {
		if (!HasBit(c->flags, GCF_STATIC)) {
			this->AddItem(BSWAP32(c->ident.grfid));
		}
	}
}

/* static */ bool ScriptNewGRF::IsLoaded(SQInteger grfid)
{
	grfid = BSWAP32(GB(grfid, 0, 32)); // Match people's expectations.

	for (auto c = _grfconfig; c != nullptr; c = c->next) {
		if (!HasBit(c->flags, GCF_STATIC) && c->ident.grfid == grfid) {
			return true;
		}
	}

	return false;
}

/* static */ SQInteger ScriptNewGRF::GetVersion(SQInteger grfid)
{
	grfid = BSWAP32(GB(grfid, 0, 32)); // Match people's expectations.

	for (auto c = _grfconfig; c != nullptr; c = c->next) {
		if (!HasBit(c->flags, GCF_STATIC) && c->ident.grfid == grfid) {
			return c->version;
		}
	}

	return 0;
}

/* static */ std::optional<std::string> ScriptNewGRF::GetName(SQInteger grfid)
{
	grfid = BSWAP32(GB(grfid, 0, 32)); // Match people's expectations.

	for (auto c = _grfconfig; c != nullptr; c = c->next) {
		if (!HasBit(c->flags, GCF_STATIC) && c->ident.grfid == grfid) {
			return c->GetName();
		}
	}

	return std::nullopt;
}
