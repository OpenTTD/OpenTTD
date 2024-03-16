/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file string_func.cpp Test functionality from string_func. */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

#include "../string_func.h"

/**** String compare/equals *****/

TEST_CASE("StrCompareIgnoreCase - std::string")
{
	/* Same string, with different cases. */
	CHECK(StrCompareIgnoreCase(std::string{""}, std::string{""}) == 0);
	CHECK(StrCompareIgnoreCase(std::string{"a"}, std::string{"a"}) == 0);
	CHECK(StrCompareIgnoreCase(std::string{"a"}, std::string{"A"}) == 0);
	CHECK(StrCompareIgnoreCase(std::string{"A"}, std::string{"a"}) == 0);
	CHECK(StrCompareIgnoreCase(std::string{"A"}, std::string{"A"}) == 0);

	/* Not the same string. */
	CHECK(StrCompareIgnoreCase(std::string{""}, std::string{"b"}) < 0);
	CHECK(StrCompareIgnoreCase(std::string{"a"}, std::string{""}) > 0);

	CHECK(StrCompareIgnoreCase(std::string{"a"}, std::string{"b"}) < 0);
	CHECK(StrCompareIgnoreCase(std::string{"b"}, std::string{"a"}) > 0);
	CHECK(StrCompareIgnoreCase(std::string{"a"}, std::string{"B"}) < 0);
	CHECK(StrCompareIgnoreCase(std::string{"b"}, std::string{"A"}) > 0);
	CHECK(StrCompareIgnoreCase(std::string{"A"}, std::string{"b"}) < 0);
	CHECK(StrCompareIgnoreCase(std::string{"B"}, std::string{"a"}) > 0);

	CHECK(StrCompareIgnoreCase(std::string{"a"}, std::string{"aa"}) < 0);
	CHECK(StrCompareIgnoreCase(std::string{"aa"}, std::string{"a"}) > 0);
}

TEST_CASE("StrCompareIgnoreCase - char pointer")
{
	/* Same string, with different cases. */
	CHECK(StrCompareIgnoreCase("", "") == 0);
	CHECK(StrCompareIgnoreCase("a", "a") == 0);
	CHECK(StrCompareIgnoreCase("a", "A") == 0);
	CHECK(StrCompareIgnoreCase("A", "a") == 0);
	CHECK(StrCompareIgnoreCase("A", "A") == 0);

	/* Not the same string. */
	CHECK(StrCompareIgnoreCase("", "b") < 0);
	CHECK(StrCompareIgnoreCase("a", "") > 0);

	CHECK(StrCompareIgnoreCase("a", "b") < 0);
	CHECK(StrCompareIgnoreCase("b", "a") > 0);
	CHECK(StrCompareIgnoreCase("a", "B") < 0);
	CHECK(StrCompareIgnoreCase("b", "A") > 0);
	CHECK(StrCompareIgnoreCase("A", "b") < 0);
	CHECK(StrCompareIgnoreCase("B", "a") > 0);

	CHECK(StrCompareIgnoreCase("a", "aa") < 0);
	CHECK(StrCompareIgnoreCase("aa", "a") > 0);
}

TEST_CASE("StrCompareIgnoreCase - std::string_view")
{
	/*
	 * With std::string_view the only way to access the data is via .data(),
	 * which does not guarantee the termination that would be required by
	 * things such as stricmp/strcasecmp. So, just passing .data() into stricmp
	 * or strcasecmp would fail if it does not account for the length of the
	 * view. Thus, contrary to the string/char* tests, this uses the same base
	 * string but gets different sections to trigger these corner cases.
	 */
	std::string_view base{"aaAbB"};

	/* Same string, with different cases. */
	CHECK(StrCompareIgnoreCase(base.substr(0, 0), base.substr(1, 0)) == 0); // Different positions
	CHECK(StrCompareIgnoreCase(base.substr(0, 1), base.substr(1, 1)) == 0); // Different positions
	CHECK(StrCompareIgnoreCase(base.substr(0, 1), base.substr(2, 1)) == 0);
	CHECK(StrCompareIgnoreCase(base.substr(2, 1), base.substr(1, 1)) == 0);
	CHECK(StrCompareIgnoreCase(base.substr(2, 1), base.substr(2, 1)) == 0);

	/* Not the same string. */
	CHECK(StrCompareIgnoreCase(base.substr(3, 0), base.substr(3, 1)) < 0); // Same position, different lengths
	CHECK(StrCompareIgnoreCase(base.substr(0, 1), base.substr(0, 0)) > 0); // Same position, different lengths

	CHECK(StrCompareIgnoreCase(base.substr(0, 1), base.substr(3, 1)) < 0);
	CHECK(StrCompareIgnoreCase(base.substr(3, 1), base.substr(0, 1)) > 0);
	CHECK(StrCompareIgnoreCase(base.substr(0, 1), base.substr(4, 1)) < 0);
	CHECK(StrCompareIgnoreCase(base.substr(3, 1), base.substr(2, 1)) > 0);
	CHECK(StrCompareIgnoreCase(base.substr(2, 1), base.substr(3, 1)) < 0);
	CHECK(StrCompareIgnoreCase(base.substr(4, 1), base.substr(0, 1)) > 0);

	CHECK(StrCompareIgnoreCase(base.substr(0, 1), base.substr(0, 2)) < 0); // Same position, different lengths
	CHECK(StrCompareIgnoreCase(base.substr(0, 2), base.substr(0, 1)) > 0); // Same position, different lengths
}

TEST_CASE("StrEqualsIgnoreCase - std::string")
{
	/* Same string, with different cases. */
	CHECK(StrEqualsIgnoreCase(std::string{""}, std::string{""}));
	CHECK(StrEqualsIgnoreCase(std::string{"a"}, std::string{"a"}));
	CHECK(StrEqualsIgnoreCase(std::string{"a"}, std::string{"A"}));
	CHECK(StrEqualsIgnoreCase(std::string{"A"}, std::string{"a"}));
	CHECK(StrEqualsIgnoreCase(std::string{"A"}, std::string{"A"}));

	/* Not the same string. */
	CHECK(!StrEqualsIgnoreCase(std::string{""}, std::string{"b"}));
	CHECK(!StrEqualsIgnoreCase(std::string{"a"}, std::string{""}));
	CHECK(!StrEqualsIgnoreCase(std::string{"a"}, std::string{"b"}));
	CHECK(!StrEqualsIgnoreCase(std::string{"b"}, std::string{"a"}));
	CHECK(!StrEqualsIgnoreCase(std::string{"a"}, std::string{"aa"}));
	CHECK(!StrEqualsIgnoreCase(std::string{"aa"}, std::string{"a"}));
}

TEST_CASE("StrEqualsIgnoreCase - char pointer")
{
	/* Same string, with different cases. */
	CHECK(StrEqualsIgnoreCase("", ""));
	CHECK(StrEqualsIgnoreCase("a", "a"));
	CHECK(StrEqualsIgnoreCase("a", "A"));
	CHECK(StrEqualsIgnoreCase("A", "a"));
	CHECK(StrEqualsIgnoreCase("A", "A"));

	/* Not the same string. */
	CHECK(!StrEqualsIgnoreCase("", "b"));
	CHECK(!StrEqualsIgnoreCase("a", ""));
	CHECK(!StrEqualsIgnoreCase("a", "b"));
	CHECK(!StrEqualsIgnoreCase("b", "a"));
	CHECK(!StrEqualsIgnoreCase("a", "aa"));
	CHECK(!StrEqualsIgnoreCase("aa", "a"));
}

TEST_CASE("StrEqualsIgnoreCase - std::string_view")
{
	/*
	 * With std::string_view the only way to access the data is via .data(),
	 * which does not guarantee the termination that would be required by
	 * things such as stricmp/strcasecmp. So, just passing .data() into stricmp
	 * or strcasecmp would fail if it does not account for the length of the
	 * view. Thus, contrary to the string/char* tests, this uses the same base
	 * string but gets different sections to trigger these corner cases.
	 */
	std::string_view base{"aaAb"};

	/* Same string, with different cases. */
	CHECK(StrEqualsIgnoreCase(base.substr(0, 0), base.substr(1, 0))); // Different positions
	CHECK(StrEqualsIgnoreCase(base.substr(0, 1), base.substr(1, 1))); // Different positions
	CHECK(StrEqualsIgnoreCase(base.substr(0, 1), base.substr(2, 1)));
	CHECK(StrEqualsIgnoreCase(base.substr(2, 1), base.substr(1, 1)));
	CHECK(StrEqualsIgnoreCase(base.substr(2, 1), base.substr(2, 1)));

	/* Not the same string. */
	CHECK(!StrEqualsIgnoreCase(base.substr(3, 0), base.substr(3, 1))); // Same position, different lengths
	CHECK(!StrEqualsIgnoreCase(base.substr(0, 1), base.substr(0, 0)));
	CHECK(!StrEqualsIgnoreCase(base.substr(0, 1), base.substr(3, 1)));
	CHECK(!StrEqualsIgnoreCase(base.substr(3, 1), base.substr(0, 1)));
	CHECK(!StrEqualsIgnoreCase(base.substr(0, 1), base.substr(0, 2))); // Same position, different lengths
	CHECK(!StrEqualsIgnoreCase(base.substr(0, 2), base.substr(0, 1))); // Same position, different lengths
}

/**** String starts with *****/

TEST_CASE("StrStartsWithIgnoreCase - std::string")
{
	/* Everything starts with an empty prefix. */
	CHECK(StrStartsWithIgnoreCase(std::string{""}, std::string{""}));
	CHECK(StrStartsWithIgnoreCase(std::string{"a"}, std::string{""}));

	/* Equals string, ignoring case. */
	CHECK(StrStartsWithIgnoreCase(std::string{"a"}, std::string{"a"}));
	CHECK(StrStartsWithIgnoreCase(std::string{"a"}, std::string{"A"}));
	CHECK(StrStartsWithIgnoreCase(std::string{"A"}, std::string{"a"}));
	CHECK(StrStartsWithIgnoreCase(std::string{"A"}, std::string{"A"}));

	/* Starts with same, ignoring case. */
	CHECK(StrStartsWithIgnoreCase(std::string{"ab"}, std::string{"a"}));
	CHECK(StrStartsWithIgnoreCase(std::string{"ab"}, std::string{"A"}));
	CHECK(StrStartsWithIgnoreCase(std::string{"Ab"}, std::string{"a"}));
	CHECK(StrStartsWithIgnoreCase(std::string{"Ab"}, std::string{"A"}));

	/* Does not start the same. */
	CHECK(!StrStartsWithIgnoreCase(std::string{""}, std::string{"b"}));
	CHECK(!StrStartsWithIgnoreCase(std::string{"a"}, std::string{"b"}));
	CHECK(!StrStartsWithIgnoreCase(std::string{"b"}, std::string{"a"}));
	CHECK(!StrStartsWithIgnoreCase(std::string{"a"}, std::string{"aa"}));
}

TEST_CASE("StrStartsWithIgnoreCase - char pointer")
{
	/* Everything starts with an empty prefix. */
	CHECK(StrStartsWithIgnoreCase("", ""));
	CHECK(StrStartsWithIgnoreCase("a", ""));

	/* Equals string, ignoring case. */
	CHECK(StrStartsWithIgnoreCase("a", "a"));
	CHECK(StrStartsWithIgnoreCase("a", "A"));
	CHECK(StrStartsWithIgnoreCase("A", "a"));
	CHECK(StrStartsWithIgnoreCase("A", "A"));

	/* Starts with same, ignoring case. */
	CHECK(StrStartsWithIgnoreCase("ab", "a"));
	CHECK(StrStartsWithIgnoreCase("ab", "A"));
	CHECK(StrStartsWithIgnoreCase("Ab", "a"));
	CHECK(StrStartsWithIgnoreCase("Ab", "A"));

	/* Does not start the same. */
	CHECK(!StrStartsWithIgnoreCase("", "b"));
	CHECK(!StrStartsWithIgnoreCase("a", "b"));
	CHECK(!StrStartsWithIgnoreCase("b", "a"));
	CHECK(!StrStartsWithIgnoreCase("a", "aa"));
}

TEST_CASE("StrStartsWithIgnoreCase - std::string_view")
{
	/*
	 * With std::string_view the only way to access the data is via .data(),
	 * which does not guarantee the termination that would be required by
	 * things such as stricmp/strcasecmp. So, just passing .data() into stricmp
	 * or strcasecmp would fail if it does not account for the length of the
	 * view. Thus, contrary to the string/char* tests, this uses the same base
	 * string but gets different sections to trigger these corner cases.
	 */
	std::string_view base{"aabAb"};

	/* Everything starts with an empty prefix. */
	CHECK(StrStartsWithIgnoreCase(base.substr(0, 0), base.substr(1, 0))); // Different positions
	CHECK(StrStartsWithIgnoreCase(base.substr(0, 1), base.substr(0, 0)));

	/* Equals string, ignoring case. */
	CHECK(StrStartsWithIgnoreCase(base.substr(0, 1), base.substr(1, 1))); // Different positions
	CHECK(StrStartsWithIgnoreCase(base.substr(0, 1), base.substr(3, 1)));
	CHECK(StrStartsWithIgnoreCase(base.substr(3, 1), base.substr(0, 1)));
	CHECK(StrStartsWithIgnoreCase(base.substr(3, 1), base.substr(3, 1)));

	/* Starts with same, ignoring case. */
	CHECK(StrStartsWithIgnoreCase(base.substr(1, 2), base.substr(0, 1)));
	CHECK(StrStartsWithIgnoreCase(base.substr(1, 2), base.substr(3, 1)));
	CHECK(StrStartsWithIgnoreCase(base.substr(3, 2), base.substr(0, 1)));
	CHECK(StrStartsWithIgnoreCase(base.substr(3, 2), base.substr(3, 1)));

	/* Does not start the same. */
	CHECK(!StrStartsWithIgnoreCase(base.substr(2, 0), base.substr(2, 1)));
	CHECK(!StrStartsWithIgnoreCase(base.substr(0, 1), base.substr(2, 1)));
	CHECK(!StrStartsWithIgnoreCase(base.substr(2, 1), base.substr(0, 1)));
	CHECK(!StrStartsWithIgnoreCase(base.substr(0, 1), base.substr(0, 2)));
}

/**** String ends with *****/

TEST_CASE("StrEndsWithIgnoreCase - std::string")
{
	/* Everything ends with an empty prefix. */
	CHECK(StrEndsWithIgnoreCase(std::string{""}, std::string{""}));
	CHECK(StrEndsWithIgnoreCase(std::string{"a"}, std::string{""}));

	/* Equals string, ignoring case. */
	CHECK(StrEndsWithIgnoreCase(std::string{"a"}, std::string{"a"}));
	CHECK(StrEndsWithIgnoreCase(std::string{"a"}, std::string{"A"}));
	CHECK(StrEndsWithIgnoreCase(std::string{"A"}, std::string{"a"}));
	CHECK(StrEndsWithIgnoreCase(std::string{"A"}, std::string{"A"}));

	/* Ends with same, ignoring case. */
	CHECK(StrEndsWithIgnoreCase(std::string{"ba"}, std::string{"a"}));
	CHECK(StrEndsWithIgnoreCase(std::string{"ba"}, std::string{"A"}));
	CHECK(StrEndsWithIgnoreCase(std::string{"bA"}, std::string{"a"}));
	CHECK(StrEndsWithIgnoreCase(std::string{"bA"}, std::string{"A"}));

	/* Does not end the same. */
	CHECK(!StrEndsWithIgnoreCase(std::string{""}, std::string{"b"}));
	CHECK(!StrEndsWithIgnoreCase(std::string{"a"}, std::string{"b"}));
	CHECK(!StrEndsWithIgnoreCase(std::string{"b"}, std::string{"a"}));
	CHECK(!StrEndsWithIgnoreCase(std::string{"a"}, std::string{"aa"}));
}

TEST_CASE("StrEndsWithIgnoreCase - char pointer")
{
	/* Everything ends with an empty prefix. */
	CHECK(StrEndsWithIgnoreCase("", ""));
	CHECK(StrEndsWithIgnoreCase("a", ""));

	/* Equals string, ignoring case. */
	CHECK(StrEndsWithIgnoreCase("a", "a"));
	CHECK(StrEndsWithIgnoreCase("a", "A"));
	CHECK(StrEndsWithIgnoreCase("A", "a"));
	CHECK(StrEndsWithIgnoreCase("A", "A"));

	/* Ends with same, ignoring case. */
	CHECK(StrEndsWithIgnoreCase("ba", "a"));
	CHECK(StrEndsWithIgnoreCase("ba", "A"));
	CHECK(StrEndsWithIgnoreCase("bA", "a"));
	CHECK(StrEndsWithIgnoreCase("bA", "A"));

	/* Does not end the same. */
	CHECK(!StrEndsWithIgnoreCase("", "b"));
	CHECK(!StrEndsWithIgnoreCase("a", "b"));
	CHECK(!StrEndsWithIgnoreCase("b", "a"));
	CHECK(!StrEndsWithIgnoreCase("a", "aa"));
}

TEST_CASE("StrEndsWithIgnoreCase - std::string_view")
{
	/*
	 * With std::string_view the only way to access the data is via .data(),
	 * which does not guarantee the termination that would be required by
	 * things such as stricmp/strcasecmp. So, just passing .data() into stricmp
	 * or strcasecmp would fail if it does not account for the length of the
	 * view. Thus, contrary to the string/char* tests, this uses the same base
	 * string but gets different sections to trigger these corner cases.
	 */
	std::string_view base{"aabAba"};

	/* Everything ends with an empty prefix. */
	CHECK(StrEndsWithIgnoreCase(base.substr(0, 0), base.substr(1, 0))); // Different positions
	CHECK(StrEndsWithIgnoreCase(base.substr(0, 1), base.substr(0, 0)));

	/* Equals string, ignoring case. */
	CHECK(StrEndsWithIgnoreCase(base.substr(0, 1), base.substr(1, 1))); // Different positions
	CHECK(StrEndsWithIgnoreCase(base.substr(0, 1), base.substr(3, 1)));
	CHECK(StrEndsWithIgnoreCase(base.substr(3, 1), base.substr(0, 1)));
	CHECK(StrEndsWithIgnoreCase(base.substr(3, 1), base.substr(3, 1)));

	/* Ends with same, ignoring case. */
	CHECK(StrEndsWithIgnoreCase(base.substr(2, 2), base.substr(0, 1)));
	CHECK(StrEndsWithIgnoreCase(base.substr(2, 2), base.substr(3, 1)));
	CHECK(StrEndsWithIgnoreCase(base.substr(4, 2), base.substr(0, 1)));
	CHECK(StrEndsWithIgnoreCase(base.substr(4, 2), base.substr(3, 1)));

	/* Does not end the same. */
	CHECK(!StrEndsWithIgnoreCase(base.substr(2, 0), base.substr(2, 1)));
	CHECK(!StrEndsWithIgnoreCase(base.substr(0, 1), base.substr(2, 1)));
	CHECK(!StrEndsWithIgnoreCase(base.substr(2, 1), base.substr(0, 1)));
	CHECK(!StrEndsWithIgnoreCase(base.substr(0, 1), base.substr(0, 2)));
}


TEST_CASE("FormatArrayAsHex")
{
	CHECK(FormatArrayAsHex(std::array<uint8_t, 0>{}) == "");
	CHECK(FormatArrayAsHex(std::array<uint8_t, 1>{0x12}) == "12");
	CHECK(FormatArrayAsHex(std::array<uint8_t, 4>{0x13, 0x38, 0x42, 0xAF}) == "133842AF");
}

TEST_CASE("ConvertHexToBytes")
{
	CHECK(ConvertHexToBytes("", {}) == true);
	CHECK(ConvertHexToBytes("1", {}) == false);
	CHECK(ConvertHexToBytes("12", {}) == false);

	std::array<uint8_t, 1> bytes1;
	CHECK(ConvertHexToBytes("1", bytes1) == false);
	CHECK(ConvertHexToBytes("12", bytes1) == true);
	CHECK(bytes1[0] == 0x12);
	CHECK(ConvertHexToBytes("123", bytes1) == false);
	CHECK(ConvertHexToBytes("1g", bytes1) == false);
	CHECK(ConvertHexToBytes("g1", bytes1) == false);

	std::array<uint8_t, 2> bytes2;
	CHECK(ConvertHexToBytes("12", bytes2) == false);
	CHECK(ConvertHexToBytes("1234", bytes2) == true);
	CHECK(bytes2[0] == 0x12);
	CHECK(bytes2[1] == 0x34);

	std::array<uint8_t, 8> bytes3;
	CHECK(ConvertHexToBytes("123456789abcdef0", bytes3) == true);
	CHECK(bytes3[0] == 0x12);
	CHECK(bytes3[1] == 0x34);
	CHECK(bytes3[2] == 0x56);
	CHECK(bytes3[3] == 0x78);
	CHECK(bytes3[4] == 0x9a);
	CHECK(bytes3[5] == 0xbc);
	CHECK(bytes3[6] == 0xde);
	CHECK(bytes3[7] == 0xf0);

	CHECK(ConvertHexToBytes("123456789ABCDEF0", bytes3) == true);
	CHECK(bytes3[0] == 0x12);
	CHECK(bytes3[1] == 0x34);
	CHECK(bytes3[2] == 0x56);
	CHECK(bytes3[3] == 0x78);
	CHECK(bytes3[4] == 0x9a);
	CHECK(bytes3[5] == 0xbc);
	CHECK(bytes3[6] == 0xde);
	CHECK(bytes3[7] == 0xf0);
}

static const std::vector<std::pair<std::string, std::string>> _str_trim_testcases = {
	{"a", "a"},
	{"  a", "a"},
	{"a  ", "a"},
	{"  a   ", "a"},
	{"  a  b  c  ", "a  b  c"},
	{"   ", ""}
};

TEST_CASE("StrTrimInPlace")
{
	for (auto [input, expected] : _str_trim_testcases) {
		StrTrimInPlace(input);
		CHECK(input == expected);
	}
}

TEST_CASE("StrTrimView") {
	for (const auto& [input, expected] : _str_trim_testcases) {
		CHECK(StrTrimView(input) == expected);
	}
}

