/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file townname_gui.cpp Test town name GUI helpers. */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

#include "mock_environment.h"

#include "../fileio_func.h"
#include "../newgrf_townname.h"
#include "../strings_func.h"
#include "../townname_gui.h"

#include "../table/strings.h"

#include "../safeguards.h"

class TownNameGuiTestsFixture {
private:
	MockEnvironment &mock = MockEnvironment::Instance();

	static void InitializeLanguage()
	{
		static bool initialized = false;
		if (initialized) return;

		DeterminePaths("openttd_test.exe", true);
		InitializeLanguagePacks();

		initialized = true;
	}

public:
	TownNameGuiTestsFixture()
	{
		InitializeLanguage();
		CleanUpGRFTownNames();
		InitGRFTownGeneratorNames();
	}

	~TownNameGuiTestsFixture()
	{
		CleanUpGRFTownNames();
		InitGRFTownGeneratorNames();
	}
};

TEST_CASE_METHOD(TownNameGuiTestsFixture, "TownNameDropDown - built-in generators")
{
	DropDownList list = BuildTownNameDropDown();

	REQUIRE(list.size() == BUILTIN_TOWNNAME_GENERATOR_COUNT);

	std::set<int> values;
	for (const auto &item : list) {
		CHECK(item->Selectable());
		values.insert(item->result);
	}

	for (uint i = 0; i < BUILTIN_TOWNNAME_GENERATOR_COUNT; i++) {
		CHECK(values.contains(i));
		CHECK(GetTownNameGeneratorName(i) == STR_MAPGEN_TOWN_NAME_ORIGINAL_ENGLISH + i);
	}
}

TEST_CASE_METHOD(TownNameGuiTestsFixture, "TownNameDropDown - NewGRF generators")
{
	GRFTownName *townname = AddGRFTownName(0x12345678);
	townname->styles.emplace_back(STR_MAPGEN_TOWN_NAME_FRENCH, 0);
	townname->styles.emplace_back(STR_MAPGEN_TOWN_NAME_DANISH, 1);
	InitGRFTownGeneratorNames();

	CHECK(GetTownNameGeneratorName(BUILTIN_TOWNNAME_GENERATOR_COUNT) == STR_MAPGEN_TOWN_NAME_FRENCH);
	CHECK(GetTownNameGeneratorName(BUILTIN_TOWNNAME_GENERATOR_COUNT + 1) == STR_MAPGEN_TOWN_NAME_DANISH);

	DropDownList list = BuildTownNameDropDown();

	REQUIRE(list.size() == BUILTIN_TOWNNAME_GENERATOR_COUNT + 3);

	CHECK(list[0]->Selectable());
	CHECK(list[0]->result >= static_cast<int>(BUILTIN_TOWNNAME_GENERATOR_COUNT));
	CHECK(list[1]->Selectable());
	CHECK(list[1]->result >= static_cast<int>(BUILTIN_TOWNNAME_GENERATOR_COUNT));
	CHECK(!list[2]->Selectable());

	std::set<int> builtin_values;
	for (auto it = std::next(list.begin(), 3); it != list.end(); ++it) {
		CHECK((*it)->Selectable());
		builtin_values.insert((*it)->result);
	}

	for (uint i = 0; i < BUILTIN_TOWNNAME_GENERATOR_COUNT; i++) {
		CHECK(builtin_values.contains(i));
	}
}
