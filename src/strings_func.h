/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file strings_func.h Functions related to OTTD's strings. */

#ifndef STRINGS_FUNC_H
#define STRINGS_FUNC_H

#include "strings_type.h"
#include "string_type.h"

class StringParameters {
	StringParameters *parent; ///< If not NULL, this instance references data from this parent instance.
	uint64 *data;             ///< Array with the actual data.
	WChar *type;              ///< Array with type information about the data. Can be NULL when no type information is needed. See #StringControlCode.

public:
	uint offset;              ///< Current offset in the data/type arrays.
	uint num_param;           ///< Length of the data array.

	/** Create a new StringParameters instance. */
	StringParameters(uint64 *data, uint num_param, WChar *type) :
		parent(NULL),
		data(data),
		type(type),
		offset(0),
		num_param(num_param)
	{ }

	/** Create a new StringParameters instance. */
	template <size_t Tnum_param>
	StringParameters(int64 (&data)[Tnum_param]) :
		parent(NULL),
		data((uint64 *)data),
		type(NULL),
		offset(0),
		num_param(Tnum_param)
	{
		assert_compile(sizeof(data[0]) == sizeof(uint64));
	}

	/**
	 * Create a new StringParameters instance that can reference part of the data of
	 * the given partent instance.
	 */
	StringParameters(StringParameters &parent, uint size) :
		parent(&parent),
		data(parent.data + parent.offset),
		offset(0),
		num_param(size)
	{
		assert(size <= parent.num_param - parent.offset);
		if (parent.type == NULL) {
			this->type = NULL;
		} else {
			this->type = parent.type + parent.offset;
		}
	}

	~StringParameters()
	{
		if (this->parent != NULL) {
			this->parent->offset += this->num_param;
		}
	}

	void ClearTypeInformation();

	/**
	 * Read an int64 from the argument array. The offset is increased
	 * so the next time GetInt64 is called the next value is read.
	 */
	int64 GetInt64(WChar type = 0)
	{
		assert(this->offset < this->num_param);
		if (this->type != NULL) {
			assert(this->type[this->offset] == 0 || this->type[this->offset] == type);
			this->type[this->offset] = type;
		}
		return this->data[this->offset++];
	}

	/** Read an int32 from the argument array. @see GetInt64. */
	int32 GetInt32(WChar type = 0)
	{
		return (int32)this->GetInt64(type);
	}

	void ShiftParameters(uint amount);

	/** Get a pointer to the current element in the data array. */
	uint64 *GetDataPointer() const
	{
		return &this->data[this->offset];
	}

	/** Get a pointer to a specific element in the data array. */
	uint64 *GetPointerToOffset(uint offset) const
	{
		assert(offset < this->num_param);
		return &this->data[offset];
	}

	/** Does this instance store information about the type of the parameters. */
	bool HasTypeInformation() const
	{
		return this->type != NULL;
	}

	/** Get the type of a specific element. */
	WChar GetTypeAtOffset(uint offset) const
	{
		assert(offset < this->num_param);
		assert(this->HasTypeInformation());
		return this->type[offset];
	}

	void SetParam(uint n, uint64 v)
	{
		assert(n < this->num_param);
		this->data[n] = v;
	}

	uint64 GetParam(uint n) const
	{
		assert(n < this->num_param);
		return this->data[n];
	}
};
extern StringParameters _global_string_params;

char *InlineString(char *buf, StringID string);
char *GetString(char *buffr, StringID string, const char *last);
char *GetStringWithArgs(char *buffr, uint string, StringParameters *args, const char *last);
const char *GetStringPtr(StringID string);

void InjectDParam(uint amount);

/**
 * Set a string parameter \a v at index \a n in a given array \a s.
 * @param s Array of string parameters.
 * @param n Index of the string parameter.
 * @param v Value of the string parameter.
 */
static inline void SetDParamX(uint64 *s, uint n, uint64 v)
{
	s[n] = v;
}

/**
 * Set a string parameter \a v at index \a n in the global string parameter array.
 * @param n Index of the string parameter.
 * @param v Value of the string parameter.
 */
static inline void SetDParam(uint n, uint64 v)
{
	_global_string_params.SetParam(n, v);
}

void SetDParamStr(uint n, const char *str);

void CopyInDParam(int offs, const uint64 *src, int num);
void CopyOutDParam(uint64 *dst, int offs, int num);

/**
 * Get the current string parameter at index \a n from parameter array \a s.
 * @param s Array of string parameters.
 * @param n Index of the string parameter.
 * @return Value of the requested string parameter.
 */
static inline uint64 GetDParamX(const uint64 *s, uint n)
{
	return s[n];
}

/**
 * Get the current string parameter at index \a n from the global string parameter array.
 * @param n Index of the string parameter.
 * @return Value of the requested string parameter.
 */
static inline uint64 GetDParam(uint n)
{
	return _global_string_params.GetParam(n);
}

extern TextDirection _current_text_dir; ///< Text direction of the currently selected language

void InitializeLanguagePacks();
const char *GetCurrentLanguageIsoCode();

int CDECL StringIDSorter(const StringID *a, const StringID *b);

/** Key comparison function for std::map */
struct StringIDCompare
{
	bool operator()(StringID s1, StringID s2) const { return StringIDSorter(&s1, &s2) < 0; }
};

void CheckForMissingGlyphsInLoadedLanguagePack();

int strnatcmp(const char *s1, const char *s2);

#endif /* STRINGS_FUNC_H */
