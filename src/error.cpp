/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file error.cpp Reporting of errors. */

#include "stdafx.h"
#include "error_func.h"
#include "safeguards.h"

[[noreturn]] void NOT_REACHED(const std::source_location location)
{
	FatalError("NOT_REACHED triggered at line {} of {}", location.line(), location.file_name());
}

[[noreturn]] void AssertFailedError(std::string_view expression, const std::source_location location)
{
	FatalError("Assertion failed at line {} of {}: {}", location.line(), location.file_name(), expression);
}
