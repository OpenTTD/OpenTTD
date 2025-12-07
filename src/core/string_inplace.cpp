/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/**
 * @file string_inplace.cpp Inplace-replacement of textual and binary data.
 */

#include "../stdafx.h"
#include "string_inplace.hpp"
#include "../safeguards.h"

/**
 * Check whether any unused bytes are left between the Builder and Consumer position.
 */
[[nodiscard]] bool InPlaceBuilder::AnyBytesUnused() const noexcept
{
	return this->consumer.GetBytesRead() > this->position;
}

/**
 * Get number of unused bytes left between the Builder and Consumer position.
 */
[[nodiscard]] InPlaceBuilder::size_type InPlaceBuilder::GetBytesUnused() const noexcept
{
	return this->consumer.GetBytesRead() - this->position;
}

/**
 * Append buffer.
 */
void InPlaceBuilder::PutBuffer(std::span<const char> str)
{
	auto unused = this->GetBytesUnused();
	if (str.size() > unused) NOT_REACHED();
	std::ranges::copy(str, this->dest.data() + this->position);
	this->position += str.size();
}

/**
 * Create coupled Consumer+Builder pair.
 * @param buffer Data to consume and replace.
 * @note The lifetime of the buffer must exceed the lifetime of both the Consumer and the Builder.
 */
InPlaceReplacement::InPlaceReplacement(std::span<char> buffer)
	: consumer(buffer), builder(buffer, consumer)
{
}

InPlaceReplacement::InPlaceReplacement(const InPlaceReplacement &src)
	: consumer(src.consumer), builder(src.builder, consumer)
{
}

InPlaceReplacement& InPlaceReplacement::operator=(const InPlaceReplacement &src)
{
	this->consumer = src.consumer;
	this->builder.AssignBuffer(src.builder);
	return *this;
}
