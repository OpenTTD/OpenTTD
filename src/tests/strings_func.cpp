/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file strings_func.cpp Test functionality from strings_func. */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

#include "../strings_func.h"

TEST_CASE("HaveDParamChanged")
{
	SetDParam(0, 0);
	SetDParamStr(1, "some string");

	std::vector<StringParameterBackup> backup;
	CopyOutDParam(backup, 2);

	CHECK(HaveDParamChanged(backup) == false);

	/* A different parameter 0 (both string and numeric). */
	SetDParam(0, 1);
	CHECK(HaveDParamChanged(backup) == true);

	SetDParamStr(0, "some other string");
	CHECK(HaveDParamChanged(backup) == true);

	/* Back to the original state, nothing should have changed. */
	SetDParam(0, 0);
	CHECK(HaveDParamChanged(backup) == false);

	/* A different parameter 1 (both string and numeric). */
	SetDParamStr(1, "some other string");
	CHECK(HaveDParamChanged(backup) == true);

	SetDParam(1, 0);
	CHECK(HaveDParamChanged(backup) == true);

	/* Back to the original state, nothing should have changed. */
	SetDParamStr(1, "some string");
	CHECK(HaveDParamChanged(backup) == false);

	/* Changing paramter 2 should not have any effect, as the backup is only 2 long. */
	SetDParam(2, 3);
	CHECK(HaveDParamChanged(backup) == false);

}
