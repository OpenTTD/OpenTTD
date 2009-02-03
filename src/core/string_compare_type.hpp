/* $Id$ */

/** @file string_compare_type.hpp Comparator class for "const char *" so it can be used as a key for std::map */

#ifndef STRING_COMPARE_TYPE_HPP
#define STRING_COMPARE_TYPE_HPP

struct StringCompare {
	bool operator () (const char *a, const char *b) const
	{
		return strcmp(a, b) < 0;
	}
};

#endif /* STRING_COMPARE_TYPE_HPP */
