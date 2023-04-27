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
