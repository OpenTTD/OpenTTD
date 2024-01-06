/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file console_func.h Console functions used outside of the console code. */

#ifndef CONSOLE_FUNC_H
#define CONSOLE_FUNC_H

#include "console_type.h"
#include "core/format.hpp"

/* console modes */
extern IConsoleModes _iconsole_mode;

/* console functions */
void IConsoleInit();
void IConsoleFree();
void IConsoleClose();

/* console output */
void IConsolePrint(TextColour colour_code, const std::string &string);

/**
 * Handle the printing of text entered into the console or redirected there
 * by any other means. Text can be redirected to other clients in a network game
 * as well as to a logfile. If the network server is a dedicated server, all activities
 * are also logged. All lines to print are added to a temporary buffer which can be
 * used as a history to print them onscreen
 * @param colour_code The colour of the command.
 * @param format_string The formatting string to tell what to do with the remaining arguments.
 * @param first_arg The first argument to the format.
 * @param other_args The other arguments to the format.
 * @tparam T The type of formatting parameter.
 * @tparam A The type of the first argument.
 * @tparam Args The types of the other arguments.
 */
template <typename T, typename A, typename ... Args>
inline void IConsolePrint(TextColour colour_code, const T &format, A first_arg, Args&&... other_args)
{
	/* The separate first_arg argument is added to aid overloading.
	 * Otherwise the calls that do no need formatting will still use this function. */
	IConsolePrint(colour_code, fmt::format(format, first_arg, other_args...));
}

/* Parser */
void IConsoleCmdExec(const std::string &command_string, const uint recurse_count = 0);

bool IsValidConsoleColour(TextColour c);

#endif /* CONSOLE_FUNC_H */
