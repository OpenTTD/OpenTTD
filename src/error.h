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
#include "command_type.h"
#include "core/geometry_type.hpp"

#include <chrono>

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
	bool is_critical;               ///< Whether the error message is critical.
	std::vector<StringParameterBackup> params; ///< Backup of parameters of the message strings.
	const GRFFile *textref_stack_grffile; ///< NewGRF that filled the #TextRefStack for the error message.
	uint textref_stack_size;        ///< Number of uint32_t values to put on the #TextRefStack for the error message.
	uint32_t textref_stack[16];       ///< Values to put on the #TextRefStack for the error message.
	StringID summary_msg;           ///< General error message showed in first line. Must be valid.
	StringID detailed_msg;          ///< Detailed error message showed in second line. Can be #INVALID_STRING_ID.
	StringID extra_msg;             ///< Extra error message shown in third line. Can be #INVALID_STRING_ID.
	Point position;                 ///< Position of the error message window.
	CompanyID face;                 ///< Company belonging to the face being shown. #INVALID_COMPANY if no face present.

public:
	ErrorMessageData(const ErrorMessageData &data);
	ErrorMessageData(StringID summary_msg, StringID detailed_msg, bool is_critical = false, int x = 0, int y = 0, const GRFFile *textref_stack_grffile = nullptr, uint textref_stack_size = 0, const uint32_t *textref_stack = nullptr, StringID extra_msg = INVALID_STRING_ID);

	/* Remove the copy assignment, as the default implementation will not do the right thing. */
	ErrorMessageData &operator=(ErrorMessageData &rhs) = delete;

	/** Check whether error window shall display a company manager face */
	bool HasFace() const { return face != INVALID_COMPANY; }

	void SetDParam(uint n, uint64_t v);
	void SetDParamStr(uint n, const char *str);
	void SetDParamStr(uint n, const std::string &str);

	void CopyOutDParams();
};

/** Define a queue with errors. */
typedef std::list<ErrorMessageData> ErrorList;

void ScheduleErrorMessage(ErrorList &datas);
void ScheduleErrorMessage(const ErrorMessageData &data);

void ShowErrorMessage(StringID summary_msg, int x, int y, CommandCost cc);
void ShowErrorMessage(StringID summary_msg, StringID detailed_msg, WarningLevel wl, int x = 0, int y = 0, const GRFFile *textref_stack_grffile = nullptr, uint textref_stack_size = 0, const uint32_t *textref_stack = nullptr, StringID extra_msg = INVALID_STRING_ID);
bool HideActiveErrorMessage();

void ClearErrorMessages();
void ShowFirstError();
void UnshowCriticalError();

#endif /* ERROR_H */
