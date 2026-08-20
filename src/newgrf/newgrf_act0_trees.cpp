/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file newgrf_act0_trees.cpp NewGRF Action 0x00 handler for trees. */

#include "../stdafx.h"
#include "../debug.h"
#include "../newgrf_sound.h"
#include "../newgrf_tree.h"
#include "../tree_func.h"
#include "../tree_type.h"
#include "newgrf_bytereader.h"
#include "newgrf_internal.h"

#include "../safeguards.h"

/**
 * Define properties for trees
 * @param first Local ID of the first tree.
 * @param last Local ID of the last tree.
 * @param prop The property to change.
 * @param buf The property value.
 * @return ChangeInfoResult.
 */
static ChangeInfoResult TreesChangeInfo(uint first, uint last, int prop, ByteReader &buf)
{
	ChangeInfoResult ret = ChangeInfoResult::Success;

	if (last > _tree_mngr.GetMaxMapping()) {
		GrfMsg(1, "TreesChangeInfo: Too many trees loaded ({}), max ({}). Ignoring.", last, 255);
		return ChangeInfoResult::InvalidId;
	}

	/* Allocate tree tile specs if they haven't been allocated already. */
	if (_cur_gps.grffile->treespecs.size() < last) _cur_gps.grffile->treespecs.resize(last);

	for (uint id = first; id < last; ++id) {
		auto &treespec = _cur_gps.grffile->treespecs[id];

		switch (prop) {
			case 0x08: { // Substitute tree type
				uint8_t subs_id = buf.ReadByte();
				if (subs_id >= GetOriginalTreeSpecs().size()) {
					/* The substitute id must be one of the original trees. */
					GrfMsg(2, "TreesChangeInfo: Attempt to use new tree {} as substitute tree for {}. Ignoring.", subs_id, id);
					continue;
				}

				/* Allocate space for this tree tile. */
				if (treespec == nullptr) {
					treespec = std::make_unique<TreeSpec>(GetOriginalTreeSpecs()[subs_id]);
					treespec->grf_prop.local_id = id;
					treespec->grf_prop.subst_id = subs_id;
					treespec->grf_prop.SetGRFFile(_cur_gps.grffile);
					_tree_mngr.AddEntityID(id, _cur_gps.grffile->grfid, subs_id); // pre-reserve the tile slot
				}
				break;
			}

			case 0x09: { // Tree override
				uint8_t ovrid = buf.ReadByte();

				/* The tree tile being overridden must be an original tree. */
				if (ovrid >= GetOriginalTreeSpecs().size()) {
					GrfMsg(2, "TreesChangeInfo: Attempt to override new tree {} with tree id {}. Ignoring.", ovrid, id);
					continue;
				}

				_tree_mngr.Add(id, _cur_gps.grffile->grfid, ovrid);
				break;
			}

			case 0x0A: // Landscapes
				treespec->landscapes = LandscapeTypes{buf.ReadByte()};
				break;

			case 0x0B: // Tropic zones
				treespec->tropiczones = TropicZones{buf.ReadByte()};
				break;

			case 0x0C: // Probability
				for (int j = 0; j < 4; ++j) {
					treespec->probability[j] = buf.ReadByte();
				}
				break;

			case 0x0D: // classes
				treespec->classes[0] = static_cast<TreeClasses>(buf.ReadDWord());
				break;

			case 0x0E: // classes
				treespec->classes[1] = static_cast<TreeClasses>(buf.ReadDWord());
				break;

			case 0x0F: // classes
				treespec->classes[2] = static_cast<TreeClasses>(buf.ReadDWord());
				break;

			case 0x10: // classes
				treespec->classes[3] = static_cast<TreeClasses>(buf.ReadDWord());
				break;

			case 0x11: // Tree flags
				treespec->flags = TreeFlags{static_cast<uint8_t>(buf.ReadDWord())};
				break;

			case 0x12: // Growth cycle interval
				treespec->cycle_interval = buf.ReadByte();
				break;

			case 0x13: // Maximum number of trees on tile.
				treespec->max_trees = Clamp(buf.ReadByte(), 1, 4);
				break;

			case 0x14: // Height of tree.
				treespec->height = Clamp(buf.ReadByte(), 1, 30);
				break;

			default:
				ret = ChangeInfoResult::Unknown;
				break;
		}
	}

	return ret;
}

/** @copybrief GrfChangeInfoHandler::Reserve @return Always ChangeInfoResult::Unhandled. */
template <> ChangeInfoResult GrfChangeInfoHandler<GrfSpecFeature::Trees>::Reserve(uint, uint, int, ByteReader &) { return ChangeInfoResult::Unhandled; }
/** @copydoc GrfChangeInfoHandler::Activation */
template <> ChangeInfoResult GrfChangeInfoHandler<GrfSpecFeature::Trees>::Activation(uint first, uint last, int prop, ByteReader &buf) { return TreesChangeInfo(first, last, prop, buf); }
