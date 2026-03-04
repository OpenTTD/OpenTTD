/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file string_inplace.hpp Inplace-replacement of textual and binary data. */

#ifndef STRING_INPLACE_HPP
#define STRING_INPLACE_HPP

#include "string_builder.hpp"
#include "string_consumer.hpp"

/**
 * Builder implementation for InPlaceReplacement.
 */
class InPlaceBuilder final : public BaseStringBuilder {
	std::span<char> dest; ///< The buffer to process.
	size_type position = 0; ///< The location to write to.
	const StringConsumer &consumer; ///< The string consumer to read with.

	friend class InPlaceReplacement;

	/**
	 * Create this builder.
	 * @param dest The buffer to write to.
	 * @param consumer The consumer to read with.
	 */
	explicit InPlaceBuilder(std::span<char> dest, const StringConsumer &consumer) : dest(dest), consumer(consumer) {}

	/**
	 * Create this builder based on an existing builder's buffer.
	 * @param src The existing builder to use the buffer from.
	 * @param consumer The consumer to read with.
	 */
	InPlaceBuilder(const InPlaceBuilder &src, const StringConsumer &consumer) : dest(src.dest), position(src.position), consumer(consumer) {}

	/**
	 * Copy assignment of the buffer and its position.
	 * @param src The builder to copy the information from.
	 */
	void AssignBuffer(const InPlaceBuilder &src) { this->dest = src.dest; this->position = src.position; }
public:
	InPlaceBuilder(const InPlaceBuilder &) = delete;
	InPlaceBuilder& operator=(const InPlaceBuilder &) = delete;

	/**
	 * Check whether any bytes have been written.
	 * @return \c true if any bytes were written.
	 */
	[[nodiscard]] bool AnyBytesWritten() const noexcept { return this->position != 0; }
	/**
	 * Get number of already written bytes.
	 * @return The number of bytes written so far.
	 */
	[[nodiscard]] size_type GetBytesWritten() const noexcept { return this->position; }
	/**
	 * Get already written data.
	 * @return The part of the buffer we have written so far.
	 */
	[[nodiscard]] std::string_view GetWrittenData() const noexcept { return {this->dest.data(), this->position}; }

	[[nodiscard]] bool AnyBytesUnused() const noexcept;
	[[nodiscard]] size_type GetBytesUnused() const noexcept;

	void PutBuffer(std::span<const char> str) override;

	/**
	 * Implementation of std::back_insert_iterator for non-growing destination buffer.
	 */
	class back_insert_iterator {
		InPlaceBuilder *parent = nullptr; ///< The builder we belong to.
	public:
		using value_type = void; ///< C++ specification trait 'value_type' of std::back_insert_iterator.
		using difference_type = void; ///< C++ specification trait 'difference_type' of std::back_insert_iterator.
		using iterator_category = std::output_iterator_tag; ///< C++ specification trait 'iterator_category' of std::back_insert_iterator.
		using pointer = void; ///< C++ specification trait 'pointer' of std::back_insert_iterator.
		using reference = void; ///< C++ specification trait 'reference' of std::back_insert_iterator.

		/**
		 * Create the iterator.
		 * @param parent The builder to insert data to.
		 */
		back_insert_iterator(InPlaceBuilder &parent) : parent(&parent) {}

		back_insert_iterator &operator++() { return *this; }
		back_insert_iterator operator++(int) { return *this; }
		back_insert_iterator &operator*() { return *this; }

		back_insert_iterator &operator=(char value)
		{
			this->parent->PutChar(value);
			return *this;
		}
	};
	/**
	 * Create a back-insert-iterator.
	 * @return The back insert iterator.
	 */
	back_insert_iterator back_inserter()
	{
		return back_insert_iterator(*this);
	}
};

/**
 * Compose data into a fixed size buffer, which is consumed at the same time.
 * - The Consumer reads data from a buffer.
 * - The Builder writes data to the buffer, replacing already consumed data.
 * - The Builder asserts, if it overtakes the consumer.
 */
class InPlaceReplacement {
public:
	StringConsumer consumer; ///< Consumer from shared buffer
	InPlaceBuilder builder; ///< Builder into shared buffer

public:
	InPlaceReplacement(std::span<char> buffer);
	InPlaceReplacement(const InPlaceReplacement &src);
	InPlaceReplacement& operator=(const InPlaceReplacement &src);
};

#endif /* STRING_INPLACE_HPP */
