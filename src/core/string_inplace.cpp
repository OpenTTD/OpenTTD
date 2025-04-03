/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file string_inplace.cpp Inplace-replacement of textual and binary data.
 */

#include "../stdafx.h"
#include "string_inplace.hpp"
#include "../safeguards.h"

[[nodiscard]] bool InPlaceBuilder::AnyBytesUnused() const noexcept
{
	return this->consumer.GetBytesRead() > this->position;
}

[[nodiscard]] InPlaceBuilder::size_type InPlaceBuilder::GetBytesUnused() const noexcept
{
	return this->consumer.GetBytesRead() - this->position;
}

void InPlaceBuilder::PutBuffer(const char *str, size_type len)
{
	auto unused = this->GetBytesUnused();
	if (len > unused) NOT_REACHED();
	std::copy(str, str + len, this->dest.data() + this->position);
	this->position += len;
}

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
