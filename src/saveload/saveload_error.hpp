/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file saveload.h Functions/types related to errors from savegames. */

#ifndef SAVELOAD_ERROR_HPP
#define SAVELOAD_ERROR_HPP

#include "../3rdparty/fmt/format.h"
#include "../strings_type.h"

[[noreturn]] void SlError(StringID string, const std::string &extra_msg = {});
[[noreturn]] void SlErrorCorrupt(const std::string &msg);

/**
 * Issue an SlErrorCorrupt with a format string.
 * @param format_string The formatting string to tell what to do with the remaining arguments.
 * @param fmt_args The arguments to be passed to fmt.
 * @tparam Args The types of the fmt arguments.
 * @note This function does never return as it throws an exception to
 *       break out of all the saveload code.
 */
template <typename ... Args>
[[noreturn]] inline void SlErrorCorruptFmt(const fmt::format_string<Args...> format, Args&&... fmt_args)
{
	SlErrorCorrupt(fmt::format(format, std::forward<Args>(fmt_args)...));
}

#endif /* SAVELOAD_ERROR_HPP */
