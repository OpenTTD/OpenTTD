/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file string_func.cpp Test functionality from string_func. */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

#include "../string_func.h"
#include "../strings_func.h"
#include "../core/string_builder.hpp"
#include "../core/string_consumer.hpp"
#include "../table/control_codes.h"

#include "table/strings.h"

#include "../safeguards.h"

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

TEST_CASE("StrCompareIgnoreCase - std::string_view")
{
	/*
	 * With std::string_view the only way to access the data is via .data(),
	 * which does not guarantee the termination that would be required by
	 * things such as stricmp/strcasecmp. So, just passing .data() into stricmp
	 * or strcasecmp would fail if it does not account for the length of the
	 * view. Thus, contrary to the string tests, this uses the same base
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

TEST_CASE("StrEqualsIgnoreCase - std::string_view")
{
	/*
	 * With std::string_view the only way to access the data is via .data(),
	 * which does not guarantee the termination that would be required by
	 * things such as stricmp/strcasecmp. So, just passing .data() into stricmp
	 * or strcasecmp would fail if it does not account for the length of the
	 * view. Thus, contrary to the string tests, this uses the same base
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

TEST_CASE("StrStartsWithIgnoreCase - std::string_view")
{
	/*
	 * With std::string_view the only way to access the data is via .data(),
	 * which does not guarantee the termination that would be required by
	 * things such as stricmp/strcasecmp. So, just passing .data() into stricmp
	 * or strcasecmp would fail if it does not account for the length of the
	 * view. Thus, contrary to the string tests, this uses the same base
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

TEST_CASE("StrEndsWithIgnoreCase - std::string_view")
{
	/*
	 * With std::string_view the only way to access the data is via .data(),
	 * which does not guarantee the termination that would be required by
	 * things such as stricmp/strcasecmp. So, just passing .data() into stricmp
	 * or strcasecmp would fail if it does not account for the length of the
	 * view. Thus, contrary to the string tests, this uses the same base
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
	{"   ", ""},
	{"  \r\f\t  ", ""},
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
		CHECK(StrTrimView(input, StringConsumer::WHITESPACE_NO_NEWLINE) == expected);
	}
}

extern void FixSCCEncoded(std::string &str, bool fix_code);

/* Helper to call FixSCCEncoded and return the result in a new string. */
static std::string FixSCCEncodedWrapper(const std::string &str, bool fix_code)
{
	std::string result = str;
	FixSCCEncoded(result, fix_code);
	return result;
}

/* Helper to compose a string part from a unicode character */
static void ComposePart(StringBuilder &builder, char32_t c)
{
	builder.PutUtf8(c);
}

/* Helper to compose a string part from a string. */
static void ComposePart(StringBuilder &builder, const std::string &value)
{
	builder += value;
}

/* Helper to compose a string from unicode or string parts. */
template <typename... Args>
static std::string Compose(Args &&... args)
{
	std::string result;
	StringBuilder builder(result);
	(ComposePart(builder, args), ...);
	return result;
}

TEST_CASE("FixSCCEncoded")
{
	/* Test conversion of empty string. */
	CHECK(FixSCCEncodedWrapper("", false) == "");

	/* Test conversion of old code to new code. */
	CHECK(FixSCCEncodedWrapper("\uE0280", true) == Compose(SCC_ENCODED, "0"));

	/* Test conversion of two old codes to new codes. */
	CHECK(FixSCCEncodedWrapper("\uE0280:\uE0281", true) == Compose(SCC_ENCODED, "0", SCC_RECORD_SEPARATOR, SCC_ENCODED, "1"));

	/* Test conversion with no parameter. */
	CHECK(FixSCCEncodedWrapper("\uE0001", false) == Compose(SCC_ENCODED, "1"));

	/* Test conversion with one numeric parameter. */
	CHECK(FixSCCEncodedWrapper("\uE00022:1", false) == Compose(SCC_ENCODED, "22", SCC_RECORD_SEPARATOR, SCC_ENCODED_NUMERIC, "1"));

	/* Test conversion with signed numeric parameter. */
	CHECK(FixSCCEncodedWrapper("\uE00022:-1", false) == Compose(SCC_ENCODED, "22", SCC_RECORD_SEPARATOR, SCC_ENCODED_NUMERIC, "-1"));

	/* Test conversion with two numeric parameters. */
	CHECK(FixSCCEncodedWrapper("\uE0003:12:2", false) == Compose(SCC_ENCODED, "3", SCC_RECORD_SEPARATOR, SCC_ENCODED_NUMERIC, "12", SCC_RECORD_SEPARATOR, SCC_ENCODED_NUMERIC, "2"));

	/* Test conversion with one string parameter. */
	CHECK(FixSCCEncodedWrapper("\uE0004:\"Foo\"", false) == Compose(SCC_ENCODED, "4", SCC_RECORD_SEPARATOR, SCC_ENCODED_STRING, "Foo"));

	/* Test conversion with two string parameters. */
	CHECK(FixSCCEncodedWrapper("\uE00055:\"Foo\":\"Bar\"", false) == Compose(SCC_ENCODED, "55", SCC_RECORD_SEPARATOR, SCC_ENCODED_STRING, "Foo", SCC_RECORD_SEPARATOR, SCC_ENCODED_STRING, "Bar"));

	/* Test conversion with two string parameters surrounding a numeric parameter. */
	CHECK(FixSCCEncodedWrapper("\uE0006:\"Foo\":7CA:\"Bar\"", false) == Compose(SCC_ENCODED, "6", SCC_RECORD_SEPARATOR, SCC_ENCODED_STRING, "Foo", SCC_RECORD_SEPARATOR, SCC_ENCODED_NUMERIC, "7CA", SCC_RECORD_SEPARATOR, SCC_ENCODED_STRING, "Bar"));

	/* Test conversion with one sub-string and two string parameters. */
	CHECK(FixSCCEncodedWrapper("\uE000777:\uE0008888:\"Foo\":\"BarBaz\"", false) == Compose(SCC_ENCODED, "777", SCC_RECORD_SEPARATOR, SCC_ENCODED, "8888", SCC_RECORD_SEPARATOR, SCC_ENCODED_STRING, "Foo", SCC_RECORD_SEPARATOR, SCC_ENCODED_STRING, "BarBaz"));
}

extern void FixSCCEncodedNegative(std::string &str);

/* Helper to call FixSCCEncodedNegative and return the result in a new string. */
static std::string FixSCCEncodedNegativeWrapper(const std::string &str)
{
	std::string result = str;
	FixSCCEncodedNegative(result);
	return result;
}

TEST_CASE("FixSCCEncodedNegative")
{
	auto positive = Compose(SCC_ENCODED, "777", SCC_RECORD_SEPARATOR, SCC_ENCODED_NUMERIC, "ffffffffffffffff");
	auto negative = Compose(SCC_ENCODED, "777", SCC_RECORD_SEPARATOR, SCC_ENCODED_NUMERIC, "-1");

	CHECK(FixSCCEncodedNegativeWrapper("") == "");
	CHECK(FixSCCEncodedNegativeWrapper(positive) == positive);
	CHECK(FixSCCEncodedNegativeWrapper(negative) == positive);
}

TEST_CASE("EncodedString::ReplaceParam - positive")
{
	/* Test that two encoded strings with different parameters are not the same. */
	EncodedString string1 = GetEncodedString(STR_NULL, "Foo"sv, 10, "Bar"sv);
	EncodedString string2 = GetEncodedString(STR_NULL, "Foo"sv, 15, "Bar"sv);
	CHECK(string1 != string2);

	/* Test that replacing parameter results in the same string. */
	EncodedString string3 = string1.ReplaceParam(1, 15);
	CHECK(string2 == string3);
}

TEST_CASE("EncodedString::ReplaceParam - negative")
{
	EncodedString string1 = GetEncodedString(STR_NULL, "Foo"sv, -1, "Bar"sv);
	EncodedString string2 = GetEncodedString(STR_NULL, "Foo"sv, -2, "Bar"sv);
	EncodedString string3 = GetEncodedString(STR_NULL, "Foo"sv, 0xFFFF'FFFF'FFFF'FFFF, "Bar"sv);
	/* Test that two encoded strings with different parameters are not the same. */
	CHECK(string1 != string2);
	/* Test that signed values are stored as unsigned. */
	CHECK(string1 == string3);

	/* Test that replacing parameter results in the same string. */
	EncodedString string4 = string1.ReplaceParam(1, -2);
	CHECK(string2 == string4);
}
