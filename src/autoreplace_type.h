/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file autoreplace_type.h Types related to autoreplacing. */

#ifndef AUTOREPLACE_TYPE_H
#define AUTOREPLACE_TYPE_H

#include "engine_type.h"
#include "group_type.h"

/**
 * Information about engine autorenews and autoreplacements.
 */
struct EngineRenew {
	EngineID from = EngineID::Invalid(); ///< Engine to replace from.
	EngineID to = EngineID::Invalid(); ///< Engine to replace to.
	bool replace_when_old = false; ///< Do replacement only when vehicle is old.
};

/**
 * EngineRenew with group for savegame conversion.
 */
struct OldEngineRenew : EngineRenew {
	GroupID group_id; ///< Group of replacement.
};

/**
 * A list to group EngineRenew directives together (such as per-company).
 */
using EngineRenewList = std::vector<EngineRenew>;

#endif /* AUTOREPLACE_TYPE_H */
