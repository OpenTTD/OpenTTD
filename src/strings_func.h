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
#include "gfx_type.h"
#include "core/bitmath_func.hpp"
#include "core/convertible_through_base.hpp"
#include "vehicle_type.h"

/**
 * Extract the StringTab from a StringID.
 * @param str String identifier
 * @return StringTab from \a str
 */
inline StringTab GetStringTab(StringID str)
{
	StringTab result = (StringTab)(str >> TAB_SIZE_BITS);
	if (result >= TEXT_TAB_NEWGRF_START) return TEXT_TAB_NEWGRF_START;
	if (result >= TEXT_TAB_GAMESCRIPT_START) return TEXT_TAB_GAMESCRIPT_START;
	return result;
}

/**
 * Extract the StringIndex from a StringID.
 * @param str String identifier
 * @return StringIndex from \a str
 */
inline StringIndexInTab GetStringIndex(StringID str)
{
	return StringIndexInTab{str - (GetStringTab(str) << TAB_SIZE_BITS)};
}

/**
 * Create a StringID
 * @param tab StringTab
 * @param index Index of the string within the given tab.
 * @return StringID composed from \a tab and \a index
 */
inline StringID MakeStringID(StringTab tab, StringIndexInTab index)
{
	if (tab == TEXT_TAB_NEWGRF_START) {
		assert(index < TAB_SIZE_NEWGRF);
	} else if (tab == TEXT_TAB_GAMESCRIPT_START) {
		assert(index < TAB_SIZE_GAMESCRIPT);
	} else {
		assert(tab < TEXT_TAB_END);
		assert(index < TAB_SIZE);
	}
	return (tab << TAB_SIZE_BITS) + index.base();
}

/**
 * Prepare the string parameters for the next formatting run, resetting the type information.
 * This is only necessary if parameters are reused for multiple format runs.
 */
static inline void PrepareArgsForNextRun(std::span<StringParameter> args)
{
	for (auto &param : args) param.type = 0;
}

std::string GetStringWithArgs(StringID string, std::span<StringParameter> args);
std::string GetString(StringID string);
std::string_view GetStringPtr(StringID string);
void AppendStringInPlace(std::string &result, StringID string);
void AppendStringWithArgsInPlace(std::string &result, StringID string, std::span<StringParameter> params);

uint ConvertKmhishSpeedToDisplaySpeed(uint speed, VehicleType type);
uint ConvertDisplaySpeedToKmhishSpeed(uint speed, VehicleType type);

/**
 * Pack velocity and vehicle type for use with SCC_VELOCITY string parameter.
 * @param speed Display speed for parameter.
 * @param type Type of vehicle for parameter.
 * @return Bit-packed velocity and vehicle type, for use with string parameters.
 */
inline int64_t PackVelocity(uint speed, VehicleType type)
{
	/* Vehicle type is a byte, so packed into the top 8 bits of the 64-bit
	 * parameter, although only values from 0-3 are relevant. */
	return speed | (static_cast<uint64_t>(type) << 56);
}

uint64_t GetParamMaxValue(uint64_t max_value, uint min_count = 0, FontSize size = FS_NORMAL);
uint64_t GetParamMaxDigits(uint count, FontSize size = FS_NORMAL);

extern TextDirection _current_text_dir; ///< Text direction of the currently selected language

void InitializeLanguagePacks();
const char *GetCurrentLanguageIsoCode();
std::string_view GetListSeparator();

/**
 * Helper to create the StringParameters with its own buffer with the given
 * parameter values.
 * @param args The parameters to set for the to be created StringParameters.
 * @return The constructed StringParameters.
 */
template <typename... Args>
auto MakeParameters(Args &&... args)
{
	return std::array<StringParameter, sizeof...(args)>({std::forward<StringParameter>(args)...});
}

/**
 * Get a parsed string with most special stringcodes replaced by the string parameters.
 * @param string String ID to format.
 * @param args The parameters to set.
 * @return The parsed string.
 */
template <typename... Args>
std::string GetString(StringID string, Args &&... args)
{
	auto params = MakeParameters(std::forward<Args &&>(args)...);
	return GetStringWithArgs(string, params);
}

EncodedString GetEncodedString(StringID str);
EncodedString GetEncodedStringWithArgs(StringID str, std::span<const StringParameter> params);

/**
 * Encode a string with no parameters into an encoded string, if the string id is valid.
 * @note the return encoded string will be empty if the string id is not valid.
 * @param str String to encode.
 * @returns an EncodedString.
 */
static inline EncodedString GetEncodedStringIfValid(StringID str)
{
	if (str == INVALID_STRING_ID) return {};
	return GetEncodedString(str);
}

/**
 * Get an encoded string with parameters.
 * @param string String ID to encode.
 * @param args The parameters to set.
 * @return The encoded string.
 */
template <typename... Args>
EncodedString GetEncodedString(StringID string, const Args&... args)
{
	auto params = MakeParameters(std::forward<const Args&>(args)...);
	return GetEncodedStringWithArgs(string, params);
}

/**
 * A searcher for missing glyphs.
 */
class MissingGlyphSearcher {
public:
	/** Make sure everything gets destructed right. */
	virtual ~MissingGlyphSearcher() = default;

	/**
	 * Get the next string to search through.
	 * @return The next string or nullopt if there is none.
	 */
	virtual std::optional<std::string_view> NextString() = 0;

	/**
	 * Get the default (font) size of the string.
	 * @return The font size.
	 */
	virtual FontSize DefaultSize() = 0;

	/**
	 * Reset the search, i.e. begin from the beginning again.
	 */
	virtual void Reset() = 0;

	/**
	 * Whether to search for a monospace font or not.
	 * @return True if searching for monospace.
	 */
	virtual bool Monospace() = 0;

	/**
	 * Set the right font names.
	 * @param settings  The settings to modify.
	 * @param font_name The new font name.
	 * @param os_data Opaque pointer to OS-specific data.
	 */
	virtual void SetFontNames(struct FontCacheSettings *settings, const char *font_name, const void *os_data = nullptr) = 0;

	bool FindMissingGlyphs();
};

void CheckForMissingGlyphs(bool base_font = true, MissingGlyphSearcher *search = nullptr);

#endif /* STRINGS_FUNC_H */
