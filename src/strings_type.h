/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file strings_type.h Types related to strings. */

#ifndef STRINGS_TYPE_H
#define STRINGS_TYPE_H

#include "core/strong_typedef_type.hpp"

/**
 * Numeric value that represents a string, independent of the selected language.
 */
typedef uint32_t StringID;
static const StringID INVALID_STRING_ID = 0xFFFF; ///< Constant representing an invalid string (16bit in case it is used in savegames)
static const int MAX_CHAR_LENGTH        = 4;      ///< Max. length of UTF-8 encoded unicode character
static const uint MAX_LANG              = 0x7F;   ///< Maximum number of languages supported by the game, and the NewGRF specs

/** Directions a text can go to */
enum TextDirection {
	TD_LTR, ///< Text is written left-to-right by default
	TD_RTL, ///< Text is written right-to-left by default
};

/** StringTabs to group StringIDs */
enum StringTab {
	/* Tabs 0..1 for regular strings */
	TEXT_TAB_TOWN             =  4,
	TEXT_TAB_INDUSTRY         =  9,
	TEXT_TAB_STATION          = 12,
	TEXT_TAB_SPECIAL          = 14,
	TEXT_TAB_OLD_CUSTOM       = 15,
	TEXT_TAB_VEHICLE          = 16,
	/* Tab 17 for regular strings */
	TEXT_TAB_OLD_NEWGRF       = 26,
	TEXT_TAB_END              = 32, ///< End of language files.
	TEXT_TAB_GAMESCRIPT_START = 32, ///< Start of GameScript supplied strings.
	TEXT_TAB_NEWGRF_START     = 64, ///< Start of NewGRF supplied strings.
};

/** Number of bits for the StringIndex within a StringTab */
static const uint TAB_SIZE_BITS       = 11;
/** Number of strings per StringTab */
static const uint TAB_SIZE            = 1 << TAB_SIZE_BITS;

/** Number of strings for GameScripts */
static const uint TAB_SIZE_GAMESCRIPT = TAB_SIZE * 32;

/** Number of strings for NewGRFs */
static const uint TAB_SIZE_NEWGRF     = TAB_SIZE * 256;

/** Special string constants */
enum SpecialStrings {

	/* special strings for town names. the town name is generated dynamically on request. */
	SPECSTR_TOWNNAME_START     = 0x20C0,
	SPECSTR_TOWNNAME_ENGLISH   = SPECSTR_TOWNNAME_START,
	SPECSTR_TOWNNAME_FRENCH,
	SPECSTR_TOWNNAME_GERMAN,
	SPECSTR_TOWNNAME_AMERICAN,
	SPECSTR_TOWNNAME_LATIN,
	SPECSTR_TOWNNAME_SILLY,
	SPECSTR_TOWNNAME_SWEDISH,
	SPECSTR_TOWNNAME_DUTCH,
	SPECSTR_TOWNNAME_FINNISH,
	SPECSTR_TOWNNAME_POLISH,
	SPECSTR_TOWNNAME_SLOVAK,
	SPECSTR_TOWNNAME_NORWEGIAN,
	SPECSTR_TOWNNAME_HUNGARIAN,
	SPECSTR_TOWNNAME_AUSTRIAN,
	SPECSTR_TOWNNAME_ROMANIAN,
	SPECSTR_TOWNNAME_CZECH,
	SPECSTR_TOWNNAME_SWISS,
	SPECSTR_TOWNNAME_DANISH,
	SPECSTR_TOWNNAME_TURKISH,
	SPECSTR_TOWNNAME_ITALIAN,
	SPECSTR_TOWNNAME_CATALAN,
	SPECSTR_TOWNNAME_LAST      = SPECSTR_TOWNNAME_CATALAN,

	/* special strings for company names on the form "TownName transport". */
	SPECSTR_COMPANY_NAME_START = 0x70EA,
	SPECSTR_COMPANY_NAME_LAST  = SPECSTR_COMPANY_NAME_START + SPECSTR_TOWNNAME_LAST - SPECSTR_TOWNNAME_START,

	SPECSTR_SILLY_NAME         = 0x70E5,
	SPECSTR_ANDCO_NAME         = 0x70E6,
	SPECSTR_PRESIDENT_NAME     = 0x70E7,
};

/** Data that is to be stored when backing up StringParameters. */
struct StringParameterBackup {
	uint64_t data; ///< The data field; valid *when* string has no value.
	std::optional<std::string> string; ///< The string value.

	/**
	 * Assign the numeric data with the given value, while clearing the stored string.
	 * @param data The new value of the data field.
	 * @return This object.
	 */
	StringParameterBackup &operator=(uint64_t data)
	{
		this->string.reset();
		this->data = data;
		return *this;
	}

	/**
	 * Assign a copy of the given string to the string field, while clearing the data field.
	 * @param string The new value of the string.
	 * @return This object.
	 */
	StringParameterBackup &operator=(const std::string_view string)
	{
		this->data = 0;
		this->string.emplace(string);
		return *this;
	}
};

/** The data required to format and validate a single parameter of a string. */
struct StringParameter {
	uint64_t data; ///< The data of the parameter.
	const char *string_view; ///< The string value, if it has any.
	std::unique_ptr<std::string> string; ///< Copied string value, if it has any.
	char32_t type; ///< The #StringControlCode to interpret this data with when it's the first parameter, otherwise '\0'.
};

class StringParameters {
protected:
	StringParameters *parent = nullptr; ///< If not nullptr, this instance references data from this parent instance.
	std::span<StringParameter> parameters = {}; ///< Array with the actual parameters.

	size_t offset = 0; ///< Current offset in the parameters span.
	char32_t next_type = 0; ///< The type of the next data that is retrieved.

	StringParameters(std::span<StringParameter> parameters = {}) :
		parameters(parameters)
	{}

	StringParameter *GetNextParameterPointer();

public:
	/**
	 * Create a new StringParameters instance that can reference part of the data of
	 * the given parent instance.
	 */
	StringParameters(StringParameters &parent, size_t size) :
		parent(&parent),
		parameters(parent.parameters.subspan(parent.offset, size))
	{}

	void PrepareForNextRun();
	void SetTypeOfNextParameter(char32_t type) { this->next_type = type; }

	/**
	 * Get the current offset, so it can be backed up for certain processing
	 * steps, or be used to offset the argument index within sub strings.
	 * @return The current offset.
	 */
	size_t GetOffset() { return this->offset; }

	/**
	 * Set the offset within the string from where to return the next result of
	 * \c GetInt64 or \c GetInt32.
	 * @param offset The offset.
	 */
	void SetOffset(size_t offset)
	{
		/*
		 * The offset must be fewer than the number of parameters when it is
		 * being set. Unless restoring a backup, then the original value is
		 * correct as well as long as the offset was not changed. In other
		 * words, when the offset was already at the end of the parameters and
		 * the string did not consume any parameters.
		 */
		assert(offset < this->parameters.size() || this->offset == offset);
		this->offset = offset;
	}

	/**
	 * Advance the offset within the string from where to return the next result of
	 * \c GetInt64 or \c GetInt32.
	 * @param advance The amount to advance the offset by.
	 */
	void AdvanceOffset(size_t advance)
	{
		this->offset += advance;
		assert(this->offset <= this->parameters.size());
	}

	/**
	 * Get the next parameter from our parameters.
	 * This updates the offset, so the next time this is called the next parameter
	 * will be read.
	 * @return The next parameter's value.
	 */
	template <typename T>
	T GetNextParameter()
	{
		auto ptr = GetNextParameterPointer();
		return static_cast<T>(ptr->data);
	}

	/**
	 * Get the next string parameter from our parameters.
	 * This updates the offset, so the next time this is called the next parameter
	 * will be read.
	 * @return The next parameter's value.
	 */
	const char *GetNextParameterString()
	{
		auto ptr = GetNextParameterPointer();
		return ptr->string != nullptr ? ptr->string->c_str() : ptr->string_view;
	}

	/**
	 * Get a new instance of StringParameters that is a "range" into the
	 * remaining existing parameters. Upon destruction the offset in the parent
	 * is not updated. However, calls to SetDParam do update the parameters.
	 *
	 * The returned StringParameters must not outlive this StringParameters.
	 * @return A "range" of the string parameters.
	 */
	StringParameters GetRemainingParameters() { return GetRemainingParameters(this->offset); }

	/**
	 * Get a new instance of StringParameters that is a "range" into the
	 * remaining existing parameters from the given offset. Upon destruction the
	 * offset in the parent is not updated. However, calls to SetDParam do
	 * update the parameters.
	 *
	 * The returned StringParameters must not outlive this StringParameters.
	 * @param offset The offset to get the remaining parameters for.
	 * @return A "range" of the string parameters.
	 */
	StringParameters GetRemainingParameters(size_t offset)
	{
		return StringParameters(this->parameters.subspan(offset, this->parameters.size() - offset));
	}

	/** Return the amount of elements which can still be read. */
	size_t GetDataLeft() const
	{
		return this->parameters.size() - this->offset;
	}

	/** Get the type of a specific element. */
	char32_t GetTypeAtOffset(size_t offset) const
	{
		assert(offset < this->parameters.size());
		return this->parameters[offset].type;
	}

	void SetParam(size_t n, uint64_t v)
	{
		assert(n < this->parameters.size());
		this->parameters[n].data = v;
		this->parameters[n].string.reset();
		this->parameters[n].string_view = nullptr;
	}

	template <typename T, std::enable_if_t<std::is_base_of<StrongTypedefBase, T>::value, int> = 0>
	void SetParam(size_t n, T v)
	{
		SetParam(n, v.base());
	}

	void SetParam(size_t n, const char *str)
	{
		assert(n < this->parameters.size());
		this->parameters[n].data = 0;
		this->parameters[n].string.reset();
		this->parameters[n].string_view = str;
	}

	void SetParam(size_t n, const std::string &str) { this->SetParam(n, str.c_str()); }

	void SetParam(size_t n, std::string &&str)
	{
		assert(n < this->parameters.size());
		this->parameters[n].data = 0;
		this->parameters[n].string = std::make_unique<std::string>(std::move(str));
		this->parameters[n].string_view = nullptr;
	}

	uint64_t GetParam(size_t n) const
	{
		assert(n < this->parameters.size());
		assert(this->parameters[n].string_view == nullptr && this->parameters[n].string == nullptr);
		return this->parameters[n].data;
	}

	/**
	 * Get the stored string of the parameter, or \c nullptr when there is none.
	 * @param n The index into the parameters.
	 * @return The stored string.
	 */
	const char *GetParamStr(size_t n) const
	{
		assert(n < this->parameters.size());
		auto &param = this->parameters[n];
		return param.string != nullptr ? param.string->c_str() : param.string_view;
	}
};

/**
 * Extension of StringParameters with its own statically sized buffer for
 * the parameters.
 */
template <size_t N>
class ArrayStringParameters : public StringParameters {
	std::array<StringParameter, N> params{}; ///< The actual parameters

public:
	ArrayStringParameters()
	{
		this->parameters = std::span(params.data(), params.size());
	}

	ArrayStringParameters(ArrayStringParameters&& other) noexcept
	{
		*this = std::move(other);
	}

	ArrayStringParameters& operator=(ArrayStringParameters &&other) noexcept
	{
		this->offset = other.offset;
		this->next_type = other.next_type;
		this->params = std::move(other.params);
		this->parameters = std::span(params.data(), params.size());
		return *this;
	}

	ArrayStringParameters(const ArrayStringParameters &other) = delete;
	ArrayStringParameters& operator=(const ArrayStringParameters &other) = delete;
};

/**
 * Helper to create the StringParameters with its own buffer with the given
 * parameter values.
 * @param args The parameters to set for the to be created StringParameters.
 * @return The constructed StringParameters.
 */
template <typename... Args>
static auto MakeParameters(const Args&... args)
{
	ArrayStringParameters<sizeof...(args)> parameters;
	size_t index = 0;
	(parameters.SetParam(index++, std::forward<const Args&>(args)), ...);
	return parameters;
}

#endif /* STRINGS_TYPE_H */
