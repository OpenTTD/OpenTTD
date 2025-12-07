/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file newgrf_act0_objects.cpp NewGRF Action 0x00 handler for objects. */

#include "../stdafx.h"
#include "../debug.h"
#include "../newgrf_object.h"
#include "newgrf_bytereader.h"
#include "newgrf_internal.h"
#include "newgrf_stringmapping.h"

#include "../safeguards.h"

/**
 * Ignore properties for objects
 * @param prop The property to ignore.
 * @param buf The property value.
 * @return ChangeInfoResult.
 */
static ChangeInfoResult IgnoreObjectProperty(uint prop, ByteReader &buf)
{
	ChangeInfoResult ret = CIR_SUCCESS;

	switch (prop) {
		case 0x0B:
		case 0x0C:
		case 0x0D:
		case 0x12:
		case 0x14:
		case 0x16:
		case 0x17:
		case 0x18:
			buf.ReadByte();
			break;

		case 0x09:
		case 0x0A:
		case 0x10:
		case 0x11:
		case 0x13:
		case 0x15:
			buf.ReadWord();
			break;

		case 0x08:
		case 0x0E:
		case 0x0F:
			buf.ReadDWord();
			break;

		case 0x19: // Badge list
			SkipBadgeList(buf);
			break;

		default:
			ret = CIR_UNKNOWN;
			break;
	}

	return ret;
}

/**
 * Define properties for objects
 * @param first Local ID of the first object.
 * @param last Local ID of the last object.
 * @param prop The property to change.
 * @param buf The property value.
 * @return ChangeInfoResult.
 */
static ChangeInfoResult ObjectChangeInfo(uint first, uint last, int prop, ByteReader &buf)
{
	ChangeInfoResult ret = CIR_SUCCESS;

	if (last > NUM_OBJECTS_PER_GRF) {
		GrfMsg(1, "ObjectChangeInfo: Too many objects loaded ({}), max ({}). Ignoring.", last, NUM_OBJECTS_PER_GRF);
		return CIR_INVALID_ID;
	}

	/* Allocate object specs if they haven't been allocated already. */
	if (_cur_gps.grffile->objectspec.size() < last) _cur_gps.grffile->objectspec.resize(last);

	for (uint id = first; id < last; ++id) {
		auto &spec = _cur_gps.grffile->objectspec[id];

		if (prop != 0x08 && spec == nullptr) {
			/* If the object property 08 is not yet set, ignore this property */
			ChangeInfoResult cir = IgnoreObjectProperty(prop, buf);
			if (cir > ret) ret = cir;
			continue;
		}

		switch (prop) {
			case 0x08: { // Class ID
				/* Allocate space for this object. */
				if (spec == nullptr) {
					spec = std::make_unique<ObjectSpec>();
					spec->views = 1; // Default for NewGRFs that don't set it.
					spec->size = OBJECT_SIZE_1X1; // Default for NewGRFs that manage to not set it (1x1)
				}

				/* Swap classid because we read it in BE. */
				uint32_t classid = buf.ReadDWord();
				spec->class_index = ObjectClass::Allocate(std::byteswap(classid));
				break;
			}

			case 0x09: { // Class name
				AddStringForMapping(GRFStringID{buf.ReadWord()}, [spec = spec.get()](StringID str) { ObjectClass::Get(spec->class_index)->name = str; });
				break;
			}

			case 0x0A: // Object name
				AddStringForMapping(GRFStringID{buf.ReadWord()}, &spec->name);
				break;

			case 0x0B: // Climate mask
				spec->climate = LandscapeTypes{buf.ReadByte()};
				break;

			case 0x0C: // Size
				spec->size = buf.ReadByte();
				if (GB(spec->size, 0, 4) == 0 || GB(spec->size, 4, 4) == 0) {
					GrfMsg(0, "ObjectChangeInfo: Invalid object size requested (0x{:X}) for object id {}. Ignoring.", spec->size, id);
					spec->size = OBJECT_SIZE_1X1;
				}
				break;

			case 0x0D: // Build cost multiplier
				spec->build_cost_multiplier = buf.ReadByte();
				spec->clear_cost_multiplier = spec->build_cost_multiplier;
				break;

			case 0x0E: // Introduction date
				spec->introduction_date = TimerGameCalendar::Date(buf.ReadDWord());
				break;

			case 0x0F: // End of life
				spec->end_of_life_date = TimerGameCalendar::Date(buf.ReadDWord());
				break;

			case 0x10: // Flags
				spec->flags = (ObjectFlags)buf.ReadWord();
				_loaded_newgrf_features.has_2CC |= spec->flags.Test(ObjectFlag::Uses2CC);
				break;

			case 0x11: // Animation info
				spec->animation.frames = buf.ReadByte();
				spec->animation.status = static_cast<AnimationStatus>(buf.ReadByte());
				break;

			case 0x12: // Animation speed
				spec->animation.speed = buf.ReadByte();
				break;

			case 0x13: // Animation triggers
				spec->animation.triggers = static_cast<ObjectAnimationTriggers>(buf.ReadWord());
				break;

			case 0x14: // Removal cost multiplier
				spec->clear_cost_multiplier = buf.ReadByte();
				break;

			case 0x15: // Callback mask
				spec->callback_mask = static_cast<ObjectCallbackMasks>(buf.ReadWord());
				break;

			case 0x16: // Building height
				spec->height = buf.ReadByte();
				break;

			case 0x17: // Views
				spec->views = buf.ReadByte();
				if (spec->views != 1 && spec->views != 2 && spec->views != 4) {
					GrfMsg(2, "ObjectChangeInfo: Invalid number of views ({}) for object id {}. Ignoring.", spec->views, id);
					spec->views = 1;
				}
				break;

			case 0x18: // Amount placed on 256^2 map on map creation
				spec->generate_amount = buf.ReadByte();
				break;

			case 0x19: // Badge list
				spec->badges = ReadBadgeList(buf, GSF_OBJECTS);
				break;

			default:
				ret = CIR_UNKNOWN;
				break;
		}
	}

	return ret;
}

template <> ChangeInfoResult GrfChangeInfoHandler<GSF_OBJECTS>::Reserve(uint, uint, int, ByteReader &) { return CIR_UNHANDLED; }
template <> ChangeInfoResult GrfChangeInfoHandler<GSF_OBJECTS>::Activation(uint first, uint last, int prop, ByteReader &buf) { return ObjectChangeInfo(first, last, prop, buf); }
