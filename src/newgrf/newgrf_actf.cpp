/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_actf.cpp NewGRF Action 0x0F handler. */

#include "../stdafx.h"
#include "../debug.h"
#include "../newgrf_townname.h"
#include "newgrf_bytereader.h"
#include "newgrf_internal.h"

#include "table/strings.h"

#include "../safeguards.h"

/** Action 0x0F - Define Town names */
static void FeatureTownName(ByteReader &buf)
{
	/* <0F> <id> <style-name> <num-parts> <parts>
	 *
	 * B id          ID of this definition in bottom 7 bits (final definition if bit 7 set)
	 * V style-name  Name of the style (only for final definition)
	 * B num-parts   Number of parts in this definition
	 * V parts       The parts */

	uint32_t grfid = _cur_gps.grffile->grfid;

	GRFTownName *townname = AddGRFTownName(grfid);

	uint8_t id = buf.ReadByte();
	GrfMsg(6, "FeatureTownName: definition 0x{:02X}", id & 0x7F);

	if (HasBit(id, 7)) {
		/* Final definition */
		ClrBit(id, 7);
		bool new_scheme = _cur_gps.grffile->grf_version >= 7;

		uint8_t lang = buf.ReadByte();
		StringID style = STR_UNDEFINED;

		do {
			ClrBit(lang, 7);

			std::string_view name = buf.ReadString();

			std::string lang_name = TranslateTTDPatchCodes(grfid, lang, false, name);
			GrfMsg(6, "FeatureTownName: lang 0x{:X} -> '{}'", lang, lang_name);

			style = AddGRFString(grfid, GRFStringID{id}, lang, new_scheme, false, name, STR_UNDEFINED);

			lang = buf.ReadByte();
		} while (lang != 0);
		townname->styles.emplace_back(style, id);
	}

	uint8_t parts = buf.ReadByte();
	GrfMsg(6, "FeatureTownName: {} parts", parts);

	townname->partlists[id].reserve(parts);
	for (uint partnum = 0; partnum < parts; partnum++) {
		NamePartList &partlist = townname->partlists[id].emplace_back();
		uint8_t texts = buf.ReadByte();
		partlist.bitstart = buf.ReadByte();
		partlist.bitcount = buf.ReadByte();
		partlist.maxprob  = 0;
		GrfMsg(6, "FeatureTownName: part {} contains {} texts and will use GB(seed, {}, {})", partnum, texts, partlist.bitstart, partlist.bitcount);

		partlist.parts.reserve(texts);
		for (uint textnum = 0; textnum < texts; textnum++) {
			NamePart &part = partlist.parts.emplace_back();
			part.prob = buf.ReadByte();

			if (HasBit(part.prob, 7)) {
				uint8_t ref_id = buf.ReadByte();
				if (ref_id >= GRFTownName::MAX_LISTS || townname->partlists[ref_id].empty()) {
					GrfMsg(0, "FeatureTownName: definition 0x{:02X} doesn't exist, deactivating", ref_id);
					DelGRFTownName(grfid);
					DisableGrf(STR_NEWGRF_ERROR_INVALID_ID);
					return;
				}
				part.id = ref_id;
				GrfMsg(6, "FeatureTownName: part {}, text {}, uses intermediate definition 0x{:02X} (with probability {})", partnum, textnum, ref_id, part.prob & 0x7F);
			} else {
				std::string_view text = buf.ReadString();
				part.text = TranslateTTDPatchCodes(grfid, 0, false, text);
				GrfMsg(6, "FeatureTownName: part {}, text {}, '{}' (with probability {})", partnum, textnum, part.text, part.prob);
			}
			partlist.maxprob += GB(part.prob, 0, 7);
		}
		GrfMsg(6, "FeatureTownName: part {}, total probability {}", partnum, partlist.maxprob);
	}
}

template <> void GrfActionHandler<0x0F>::FileScan(ByteReader &) { }
template <> void GrfActionHandler<0x0F>::SafetyScan(ByteReader &buf) { GRFUnsafe(buf); }
template <> void GrfActionHandler<0x0F>::LabelScan(ByteReader &) { }
template <> void GrfActionHandler<0x0F>::Init(ByteReader &buf) { FeatureTownName(buf); }
template <> void GrfActionHandler<0x0F>::Reserve(ByteReader &) { }
template <> void GrfActionHandler<0x0F>::Activation(ByteReader &) { }
