/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file error_func.h Error reporting related functions. */

#ifndef ERROR_FUNC_H
#define ERROR_FUNC_H

#include "3rdparty/fmt/format.h"

void NORETURN UserErrorI(const std::string &str);
void NORETURN FatalErrorI(const std::string &str);
#define UserError(format_string, ...) UserErrorI(fmt::format(FMT_STRING(format_string), ## __VA_ARGS__))
#define FatalError(format_string, ...) FatalErrorI(fmt::format(FMT_STRING(format_string), ## __VA_ARGS__))

#endif /* ERROR_FUNC_H */
