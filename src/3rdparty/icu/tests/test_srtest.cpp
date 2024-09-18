// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
 * %W% %E%
 *
 * (C) Copyright IBM Corp. 2001-2016 - All Rights Reserved
 *
 */
/** @file test_srtest.cpp Test icu::ScriptRun result. */

#include "../../../stdafx.h"

#include "../../../3rdparty/catch2/catch.hpp"
#include "../../../3rdparty/fmt/core.h"

#include "../scriptrun.h"

#include <span>
#include <unicode/uscript.h>

static void TestScriptRun(std::span<const char16_t> testChars, std::span<const std::string_view> testResults)
{
	icu::ScriptRun scriptRun(testChars.data(), 0, std::size(testChars));
	size_t i = 0;

	while (scriptRun.next()) {
		int32_t start = scriptRun.getScriptStart();
		int32_t end = scriptRun.getScriptEnd();
		UScriptCode code = scriptRun.getScriptCode();

		REQUIRE(i < std::size(testResults));
		CHECK(fmt::format("Script '{}' from {} to {}.", uscript_getName(code), start, end) == testResults[i]);

		++i;
	}

	REQUIRE(i == std::size(testResults));
}

TEST_CASE("ICU ScriptRun")
{
	/** Example string sequence as in srtest.cpp. */
	static const char16_t testChars[] = {
		0x0020, 0x0946, 0x0939, 0x093F, 0x0928, 0x094D, 0x0926, 0x0940, 0x0020,
		0x0627, 0x0644, 0x0639, 0x0631, 0x0628, 0x064A, 0x0629, 0x0020,
		0x0420, 0x0443, 0x0441, 0x0441, 0x043A, 0x0438, 0x0439, 0x0020,
		'E', 'n', 'g', 'l', 'i', 's', 'h',  0x0020,
		0x6F22, 0x5B75, 0x3068, 0x3072, 0x3089, 0x304C, 0x306A, 0x3068,
		0x30AB, 0x30BF, 0x30AB, 0x30CA,
		0xD801, 0xDC00, 0xD801, 0xDC01, 0xD801, 0xDC02, 0xD801, 0xDC03
	};

	/** Expected results from script run. */
	static const std::string_view testResults[] = {
		"Script 'Devanagari' from 0 to 9.",
		"Script 'Arabic' from 9 to 17.",
		"Script 'Cyrillic' from 17 to 25.",
		"Script 'Latin' from 25 to 33.",
		"Script 'Han' from 33 to 35.",
		"Script 'Hiragana' from 35 to 41.",
		"Script 'Katakana' from 41 to 45.",
		"Script 'Deseret' from 45 to 53.",
	};

	TestScriptRun(testChars, testResults);
}
