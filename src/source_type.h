/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file source_type.h Type for the source of cargo. */

#ifndef SOURCE_TYPE_H
#define SOURCE_TYPE_H

#include "company_type.h"
#include "industry_type.h"
#include "news_type.h"
#include "strings_type.h"
#include "town_type.h"

/** Types of cargo source and destination */
enum class SourceType : uint8_t {
	Industry = 0, ///< Source/destination is an industry
	Town = 1, ///< Source/destination is a town
	Headquarters = 2, ///< Source/destination are company headquarters
};

using SourceID = uint16_t; ///< Contains either industry ID, town ID or company ID (or Source::Invalid)
static_assert(sizeof(SourceID) >= sizeof(CompanyID));
static_assert(sizeof(SourceID) >= sizeof(IndustryID));
static_assert(sizeof(SourceID) >= sizeof(TownID));

/** A location from where cargo can come from (or go to). Specifically industries, towns and headquarters. */
struct Source {
public:
	static constexpr SourceID Invalid = 0xFFFF; ///< Invalid/unknown index of source

	SourceID id; ///< Index of industry/town/HQ, Source::Invalid if unknown/invalid.
	SourceType type; ///< Type of \c source_id.

	Source() = default;
	Source(ConvertibleThroughBase auto id, SourceType type) : id(id.base()), type(type) {}
	Source(SourceID id, SourceType type) : id(id), type(type) {}

	constexpr CompanyID ToCompanyID() const { assert(this->type == SourceType::Headquarters); return static_cast<CompanyID>(this->id); }
	constexpr IndustryID ToIndustryID() const { assert(this->type == SourceType::Industry); return static_cast<IndustryID>(this->id); }
	constexpr TownID ToTownID() const { assert(this->type == SourceType::Town); return static_cast<TownID>(this->id); }

	constexpr void MakeInvalid() { this->id = Source::Invalid; }
	constexpr void SetIndex(SourceID index) { this->id = index; }
	constexpr void SetIndex(ConvertibleThroughBase auto index) { this->id = index.base(); }

	constexpr bool IsValid() const noexcept { return this->id != Source::Invalid; }
	auto operator<=>(const Source &source) const = default;

	NewsReference GetNewsReference() const;
	StringID GetFormat() const;
};

#endif /* SOURCE_TYPE_H */
