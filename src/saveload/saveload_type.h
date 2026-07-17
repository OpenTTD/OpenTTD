/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file saveload_type.h Types often used outside the saveload code related to saving and loading games. */

#ifndef SAVELOAD_TYPE_H
#define SAVELOAD_TYPE_H

/** Save or load result codes. */
enum class SaveLoadResult : uint8_t {
	Ok, ///< completed successfully
	Error, ///< error that was caught before internal structures were modified
	ReInit, ///< error that was caught in the middle of updating game state, need to clear it. (can only happen during load)
};

/** A table of SaveLoad entries. */
using SaveLoadTable = std::span<const struct SaveLoad>;

#endif /* SAVELOAD_TYPE_H */
