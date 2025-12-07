/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file newgrf_acte.cpp NewGRF Action 0x0E handler. */

#include "../stdafx.h"
#include "../debug.h"
#include "newgrf_bytereader.h"
#include "newgrf_internal.h"

#include "table/strings.h"

#include "../safeguards.h"

/* Action 0x0E (GLS_SAFETYSCAN) */
static void SafeGRFInhibit(ByteReader &buf)
{
	/* <0E> <num> <grfids...>
	 *
	 * B num           Number of GRFIDs that follow
	 * D grfids        GRFIDs of the files to deactivate */

	uint8_t num = buf.ReadByte();

	for (uint i = 0; i < num; i++) {
		uint32_t grfid = buf.ReadDWord();

		/* GRF is unsafe it if tries to deactivate other GRFs */
		if (grfid != _cur_gps.grfconfig->ident.grfid) {
			GRFUnsafe(buf);
			return;
		}
	}
}

/* Action 0x0E */
static void GRFInhibit(ByteReader &buf)
{
	/* <0E> <num> <grfids...>
	 *
	 * B num           Number of GRFIDs that follow
	 * D grfids        GRFIDs of the files to deactivate */

	uint8_t num = buf.ReadByte();

	for (uint i = 0; i < num; i++) {
		uint32_t grfid = buf.ReadDWord();
		GRFConfig *file = GetGRFConfig(grfid);

		/* Unset activation flag */
		if (file != nullptr && file != _cur_gps.grfconfig) {
			GrfMsg(2, "GRFInhibit: Deactivating file '{}'", file->filename);
			GRFError *error = DisableGrf(STR_NEWGRF_ERROR_FORCEFULLY_DISABLED, file);
			error->data = _cur_gps.grfconfig->GetName();
		}
	}
}

template <> void GrfActionHandler<0x0E>::FileScan(ByteReader &) { }
template <> void GrfActionHandler<0x0E>::SafetyScan(ByteReader &buf) { SafeGRFInhibit(buf); }
template <> void GrfActionHandler<0x0E>::LabelScan(ByteReader &) { }
template <> void GrfActionHandler<0x0E>::Init(ByteReader &buf) { GRFInhibit(buf); }
template <> void GrfActionHandler<0x0E>::Reserve(ByteReader &buf) { GRFInhibit(buf); }
template <> void GrfActionHandler<0x0E>::Activation(ByteReader &buf) { GRFInhibit(buf); }
