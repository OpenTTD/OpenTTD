/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_actb.cpp NewGRF Action 0x0B handler. */

#include "../stdafx.h"
#include "../debug.h"
#include "newgrf_bytereader.h"
#include "newgrf_internal.h"

#include "table/strings.h"

#include "../safeguards.h"

/* Action 0x0B */
static void GRFLoadError(ByteReader &buf)
{
	/* <0B> <severity> <language-id> <message-id> [<message...> 00] [<data...>] 00 [<parnum>]
	 *
	 * B severity      00: notice, continue loading grf file
	 *                 01: warning, continue loading grf file
	 *                 02: error, but continue loading grf file, and attempt
	 *                     loading grf again when loading or starting next game
	 *                 03: error, abort loading and prevent loading again in
	 *                     the future (only when restarting the patch)
	 * B language-id   see action 4, use 1F for built-in error messages
	 * B message-id    message to show, see below
	 * S message       for custom messages (message-id FF), text of the message
	 *                 not present for built-in messages.
	 * V data          additional data for built-in (or custom) messages
	 * B parnum        parameter numbers to be shown in the message (maximum of 2) */

	static const StringID msgstr[] = {
		STR_NEWGRF_ERROR_VERSION_NUMBER,
		STR_NEWGRF_ERROR_DOS_OR_WINDOWS,
		STR_NEWGRF_ERROR_UNSET_SWITCH,
		STR_NEWGRF_ERROR_INVALID_PARAMETER,
		STR_NEWGRF_ERROR_LOAD_BEFORE,
		STR_NEWGRF_ERROR_LOAD_AFTER,
		STR_NEWGRF_ERROR_OTTD_VERSION_NUMBER,
	};

	static const StringID sevstr[] = {
		STR_NEWGRF_ERROR_MSG_INFO,
		STR_NEWGRF_ERROR_MSG_WARNING,
		STR_NEWGRF_ERROR_MSG_ERROR,
		STR_NEWGRF_ERROR_MSG_FATAL
	};

	uint8_t severity   = buf.ReadByte();
	uint8_t lang       = buf.ReadByte();
	uint8_t message_id = buf.ReadByte();

	/* Skip the error if it isn't valid for the current language. */
	if (!CheckGrfLangID(lang, _cur_gps.grffile->grf_version)) return;

	/* Skip the error until the activation stage unless bit 7 of the severity
	 * is set. */
	if (!HasBit(severity, 7) && _cur_gps.stage == GLS_INIT) {
		GrfMsg(7, "GRFLoadError: Skipping non-fatal GRFLoadError in stage {}", _cur_gps.stage);
		return;
	}
	ClrBit(severity, 7);

	if (severity >= lengthof(sevstr)) {
		GrfMsg(7, "GRFLoadError: Invalid severity id {}. Setting to 2 (non-fatal error).", severity);
		severity = 2;
	} else if (severity == 3) {
		/* This is a fatal error, so make sure the GRF is deactivated and no
		 * more of it gets loaded. */
		DisableGrf();

		/* Make sure we show fatal errors, instead of silly infos from before */
		_cur_gps.grfconfig->error.reset();
	}

	if (message_id >= lengthof(msgstr) && message_id != 0xFF) {
		GrfMsg(7, "GRFLoadError: Invalid message id.");
		return;
	}

	if (buf.Remaining() <= 1) {
		GrfMsg(7, "GRFLoadError: No message data supplied.");
		return;
	}

	/* For now we can only show one message per newgrf file. */
	if (_cur_gps.grfconfig->error.has_value()) return;

	_cur_gps.grfconfig->error = {sevstr[severity]};
	GRFError *error = &_cur_gps.grfconfig->error.value();

	if (message_id == 0xFF) {
		/* This is a custom error message. */
		if (buf.HasData()) {
			std::string_view message = buf.ReadString();

			error->custom_message = TranslateTTDPatchCodes(_cur_gps.grffile->grfid, lang, true, message, SCC_RAW_STRING_POINTER);
		} else {
			GrfMsg(7, "GRFLoadError: No custom message supplied.");
			error->custom_message.clear();
		}
	} else {
		error->message = msgstr[message_id];
	}

	if (buf.HasData()) {
		std::string_view data = buf.ReadString();

		error->data = TranslateTTDPatchCodes(_cur_gps.grffile->grfid, lang, true, data);
	} else {
		GrfMsg(7, "GRFLoadError: No message data supplied.");
		error->data.clear();
	}

	/* Only two parameter numbers can be used in the string. */
	for (uint i = 0; i < error->param_value.size() && buf.HasData(); i++) {
		uint param_number = buf.ReadByte();
		error->param_value[i] = _cur_gps.grffile->GetParam(param_number);
	}
}

template <> void GrfActionHandler<0x0B>::FileScan(ByteReader &) { }
template <> void GrfActionHandler<0x0B>::SafetyScan(ByteReader &) { }
template <> void GrfActionHandler<0x0B>::LabelScan(ByteReader &) { }
template <> void GrfActionHandler<0x0B>::Init(ByteReader &buf) { GRFLoadError(buf); }
template <> void GrfActionHandler<0x0B>::Reserve(ByteReader &buf) { GRFLoadError(buf); }
template <> void GrfActionHandler<0x0B>::Activation(ByteReader &buf) { GRFLoadError(buf); }
