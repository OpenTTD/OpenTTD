/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file error.h Functions related to errors. */

#ifndef ERROR_H
#define ERROR_H

#include "strings_type.h"
#include "company_type.h"
#include "core/geometry_type.hpp"

struct GRFFile;

/** Message severity/type */
enum WarningLevel {
	WL_INFO,     ///< Used for DoCommand-like (and some non-fatal AI GUI) errors/information
	WL_WARNING,  ///< Other information
	WL_ERROR,    ///< Errors (eg. saving/loading failed)
	WL_CRITICAL, ///< Critical errors, the MessageBox is shown in all cases
};

/** The data of the error message. */
class ErrorMessageData {
protected:
	uint duration;                  ///< Length of display of the message. 0 means forever,
	uint64 decode_params[20];       ///< Parameters of the message strings.
	const char *strings[20];        ///< Copies of raw strings that were used.
	const GRFFile *textref_stack_grffile; ///< NewGRF that filled the #TextRefStack for the error message.
	uint textref_stack_size;        ///< Number of uint32 values to put on the #TextRefStack for the error message.
	uint32 textref_stack[16];       ///< Values to put on the #TextRefStack for the error message.
	StringID summary_msg;           ///< General error message showed in first line. Must be valid.
	StringID detailed_msg;          ///< Detailed error message showed in second line. Can be #INVALID_STRING_ID.
	Point position;                 ///< Position of the error message window.
	CompanyID face;                 ///< Company belonging to the face being shown. #INVALID_COMPANY if no face present.

public:
	ErrorMessageData(const ErrorMessageData &data);
	~ErrorMessageData();
	ErrorMessageData(StringID summary_msg, StringID detailed_msg, uint duration = 0, int x = 0, int y = 0, const GRFFile *textref_stack_grffile = nullptr, uint textref_stack_size = 0, const uint32 *textref_stack = nullptr);

	/** Check whether error window shall display a company manager face */
	bool HasFace() const { return face != INVALID_COMPANY; }

	void SetDParam(uint n, uint64 v);
	void SetDParamStr(uint n, const char *str);

	void CopyOutDParams();
};

void ScheduleErrorMessage(const ErrorMessageData &data);

void ShowErrorMessage(StringID summary_msg, StringID detailed_msg, WarningLevel wl, int x = 0, int y = 0, const GRFFile *textref_stack_grffile = nullptr, uint textref_stack_size = 0, const uint32 *textref_stack = nullptr);
bool HideActiveErrorMessage();

void ClearErrorMessages();
void ShowFirstError();
void UnshowCriticalError();

#endif /* ERROR_H */
