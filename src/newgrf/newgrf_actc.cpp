/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file newgrf_actc.cpp NewGRF Action 0x0C handler. */

#include "../stdafx.h"
#include "../debug.h"
#include "../string_func.h"
#include "newgrf_bytereader.h"
#include "newgrf_internal.h"

#include "../safeguards.h"

/* Action 0x0C */
static void GRFComment(ByteReader &buf)
{
	/* <0C> [<ignored...>]
	 *
	 * V ignored       Anything following the 0C is ignored */

	if (!buf.HasData()) return;

	std::string_view text = buf.ReadString();
	GrfMsg(2, "GRFComment: {}", StrMakeValid(text));
}

/** @copybrief GrfActionHandler::FileScan */
template <> void GrfActionHandler<0x0C>::FileScan(ByteReader &) { }
/** @copybrief GrfActionHandler::SafetyScan */
template <> void GrfActionHandler<0x0C>::SafetyScan(ByteReader &) { }
/** @copybrief GrfActionHandler::LabelScan */
template <> void GrfActionHandler<0x0C>::LabelScan(ByteReader &) { }
/** @copydoc GrfActionHandler::Init */
template <> void GrfActionHandler<0x0C>::Init(ByteReader &buf) { GRFComment(buf); }
/** @copybrief GrfActionHandler::Reserve */
template <> void GrfActionHandler<0x0C>::Reserve(ByteReader &) { }
/** @copydoc GrfActionHandler::Activation */
template <> void GrfActionHandler<0x0C>::Activation(ByteReader &buf) { GRFComment(buf); }
