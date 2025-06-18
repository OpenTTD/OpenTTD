/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_act10.cpp NewGRF Action 0x10 handler. */

#include "../stdafx.h"
#include "../debug.h"
#include "newgrf_bytereader.h"
#include "newgrf_internal.h"

#include "../safeguards.h"

/** Action 0x10 - Define goto label */
static void DefineGotoLabel(ByteReader &buf)
{
	/* <10> <label> [<comment>]
	 *
	 * B label      The label to define
	 * V comment    Optional comment - ignored */

	uint8_t nfo_label = buf.ReadByte();

	_cur_gps.grffile->labels.emplace_back(nfo_label, _cur_gps.nfo_line, _cur_gps.file->GetPos());

	GrfMsg(2, "DefineGotoLabel: GOTO target with label 0x{:02X}", nfo_label);
}

template <> void GrfActionHandler<0x10>::FileScan(ByteReader &) { }
template <> void GrfActionHandler<0x10>::SafetyScan(ByteReader &) { }
template <> void GrfActionHandler<0x10>::LabelScan(ByteReader &buf) { DefineGotoLabel(buf); }
template <> void GrfActionHandler<0x10>::Init(ByteReader &) { }
template <> void GrfActionHandler<0x10>::Reserve(ByteReader &) { }
template <> void GrfActionHandler<0x10>::Activation(ByteReader &) { }
