/* $Id$ */

/** @file squirrel_helper_type.hpp Helper structs for converting Squirrel data structures to C++. */

#ifndef SQUIRREL_HELPER_TYPE_HPP
#define SQUIRREL_HELPER_TYPE_HPP

struct Array {
	int32 size;
	int32 array[VARARRAY_SIZE];
};

#endif /* SQUIRREL_HELPER_TYPE_HPP */
