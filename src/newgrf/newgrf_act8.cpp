/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file newgrf_act8.cpp NewGRF Action 0x08 handler. */

#include "../stdafx.h"
#include "../debug.h"
#include "../string_func.h"
#include "newgrf_bytereader.h"
#include "newgrf_internal.h"

#include "table/strings.h"

#include "../safeguards.h"

/* Action 0x08 (GLS_FILESCAN) */
static void ScanInfo(ByteReader &buf)
{
	uint8_t grf_version = buf.ReadByte();
	uint32_t grfid      = buf.ReadDWord();
	std::string_view name = buf.ReadString();

	_cur_gps.grfconfig->ident.grfid = grfid;

	if (grf_version < 2 || grf_version > 8) {
		_cur_gps.grfconfig->flags.Set(GRFConfigFlag::Invalid);
		Debug(grf, 0, "{}: NewGRF \"{}\" (GRFID {:08X}) uses GRF version {}, which is incompatible with this version of OpenTTD.", _cur_gps.grfconfig->filename, StrMakeValid(name), std::byteswap(grfid), grf_version);
	}

	/* GRF IDs starting with 0xFF are reserved for internal TTDPatch use */
	if (GB(grfid, 0, 8) == 0xFF) _cur_gps.grfconfig->flags.Set(GRFConfigFlag::System);

	AddGRFTextToList(_cur_gps.grfconfig->name, 0x7F, grfid, false, name);

	if (buf.HasData()) {
		std::string_view info = buf.ReadString();
		AddGRFTextToList(_cur_gps.grfconfig->info, 0x7F, grfid, true, info);
	}

	/* GLS_INFOSCAN only looks for the action 8, so we can skip the rest of the file */
	_cur_gps.skip_sprites = -1;
}

/* Action 0x08 */
static void GRFInfo(ByteReader &buf)
{
	/* <08> <version> <grf-id> <name> <info>
	 *
	 * B version       newgrf version, currently 06
	 * 4*B grf-id      globally unique ID of this .grf file
	 * S name          name of this .grf set
	 * S info          string describing the set, and e.g. author and copyright */

	uint8_t version    = buf.ReadByte();
	uint32_t grfid     = buf.ReadDWord();
	std::string_view name = buf.ReadString();

	if (_cur_gps.stage < GLS_RESERVE && _cur_gps.grfconfig->status != GCS_UNKNOWN) {
		DisableGrf(STR_NEWGRF_ERROR_MULTIPLE_ACTION_8);
		return;
	}

	if (_cur_gps.grffile->grfid != grfid) {
		Debug(grf, 0, "GRFInfo: GRFID {:08X} in FILESCAN stage does not match GRFID {:08X} in INIT/RESERVE/ACTIVATION stage", std::byteswap(_cur_gps.grffile->grfid), std::byteswap(grfid));
		_cur_gps.grffile->grfid = grfid;
	}

	_cur_gps.grffile->grf_version = version;
	_cur_gps.grfconfig->status = _cur_gps.stage < GLS_RESERVE ? GCS_INITIALISED : GCS_ACTIVATED;

	/* Do swap the GRFID for displaying purposes since people expect that */
	Debug(grf, 1, "GRFInfo: Loaded GRFv{} set {:08X} - {} (palette: {}, version: {})", version, std::byteswap(grfid), StrMakeValid(name), (_cur_gps.grfconfig->palette & GRFP_USE_MASK) ? "Windows" : "DOS", _cur_gps.grfconfig->version);
}

template <> void GrfActionHandler<0x08>::FileScan(ByteReader &buf) { ScanInfo(buf); }
template <> void GrfActionHandler<0x08>::SafetyScan(ByteReader &) { }
template <> void GrfActionHandler<0x08>::LabelScan(ByteReader &) { }
template <> void GrfActionHandler<0x08>::Init(ByteReader &buf) { GRFInfo(buf); }
template <> void GrfActionHandler<0x08>::Reserve(ByteReader &buf) { GRFInfo(buf); }
template <> void GrfActionHandler<0x08>::Activation(ByteReader &buf) { GRFInfo(buf); }
