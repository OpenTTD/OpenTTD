/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file error.cpp Reporting of errors. */

#include "stdafx.h"
#include "error_func.h"

[[noreturn]] void NotReachedError(int line, const char *file)
{
	FatalError("NOT_REACHED triggered at line {} of {}", line, file);
}

[[noreturn]] void AssertFailedError(int line, const char *file, const char *expression)
{
	FatalError("Assertion failed at line {} of {}: {}", line, file, expression);
}
