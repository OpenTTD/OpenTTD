/* $Id$ */

/** @file string_type.h Types for strings. */

#ifndef STRING_TYPE_H
#define STRING_TYPE_H

/**
 * Valid filter types for IsValidChar.
 */
enum CharSetFilter {
	CS_ALPHANUMERAL,      ///< Both numeric and alphabetic and spaces and stuff
	CS_NUMERAL,           ///< Only numeric ones
	CS_ALPHA,             ///< Only alphabetic values
};

typedef uint32 WChar;

#endif /* STRING_TYPE_H */
