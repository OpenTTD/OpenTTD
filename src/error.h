/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file error.h Functions related to errors. */

#ifndef ERROR_H
#define ERROR_H

#include "strings_type.h"
#include "company_type.h"
#include "command_type.h"
#include "core/geometry_type.hpp"

#include <chrono>

struct GRFFile;

/** Message severity/type */
enum WarningLevel : uint8_t {
	WL_INFO,     ///< Used for DoCommand-like (and some non-fatal AI GUI) errors/information
	WL_WARNING,  ///< Other information
	WL_ERROR,    ///< Errors (eg. saving/loading failed)
	WL_CRITICAL, ///< Critical errors, the MessageBox is shown in all cases
};

/** The data of the error message. */
class ErrorMessageData {
protected:
	bool is_critical;               ///< Whether the error message is critical.
	EncodedString summary_msg; ///< General error message showed in first line. Must be valid.
	EncodedString detailed_msg; ///< Detailed error message showed in second line. Can be #INVALID_STRING_ID.
	EncodedString extra_msg; ///< Extra error message shown in third line. Can be #INVALID_STRING_ID.
	Point position;                 ///< Position of the error message window.
	CompanyID company; ///< Company belonging to the face being shown. #CompanyID::Invalid() if no face present.

public:
	ErrorMessageData(EncodedString &&summary_msg, EncodedString &&detailed_msg, bool is_critical = false, int x = 0, int y = 0, EncodedString &&extra_msg = {}, CompanyID company = CompanyID::Invalid());

	/** Check whether error window shall display a company manager face */
	bool HasFace() const { return company != CompanyID::Invalid(); }
};

/** Define a queue with errors. */
typedef std::list<ErrorMessageData> ErrorList;

void ScheduleErrorMessage(ErrorList &datas);
void ScheduleErrorMessage(const ErrorMessageData &data);

void ShowErrorMessage(EncodedString &&summary_msg, int x, int y, CommandCost &cc);
void ShowErrorMessage(EncodedString &&summary_msg, EncodedString &&detailed_msg, WarningLevel wl, int x = 0, int y = 0, EncodedString &&extra_msg = {}, CompanyID company = CompanyID::Invalid());
bool HideActiveErrorMessage();

void ClearErrorMessages();
void ShowFirstError();
void UnshowCriticalError();

#endif /* ERROR_H */
