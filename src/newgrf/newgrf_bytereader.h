/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file newgrf_bytereader.h NewGRF buffer reader definition. */

#ifndef NEWGRF_BYTEREADER_H
#define NEWGRF_BYTEREADER_H

#include "../core/string_consumer.hpp"

class OTTDByteReaderSignal { };

/** Class to read from a NewGRF file */
class ByteReader {
	StringConsumer consumer;
public:
	ByteReader(const uint8_t *data, size_t len) : consumer(std::string_view{reinterpret_cast<const char *>(data), len}) { }

	const uint8_t *ReadBytes(size_t size)
	{
		auto result = this->consumer.Read(size);
		if (result.size() != size) throw OTTDByteReaderSignal();
		return reinterpret_cast<const uint8_t *>(result.data());
	}

	/**
	 * Read a single byte (8 bits).
	 * @return Value read from buffer.
	 */
	uint8_t ReadByte()
	{
		auto result = this->consumer.TryReadUint8();
		if (!result.has_value()) throw OTTDByteReaderSignal();
		return *result;
	}

	/**
	 * Read a single Word (16 bits).
	 * @return Value read from buffer.
	 */
	uint16_t ReadWord()
	{
		auto result = this->consumer.TryReadUint16LE();
		if (!result.has_value()) throw OTTDByteReaderSignal();
		return *result;
	}

	/**
	 * Read a single Extended Byte (8 or 16 bits).
	 * @return Value read from buffer.
	 */
	uint16_t ReadExtendedByte()
	{
		uint16_t val = this->ReadByte();
		return val == 0xFF ? this->ReadWord() : val;
	}

	/**
	 * Read a single DWord (32 bits).
	 * @return Value read from buffer.
	 */
	uint32_t ReadDWord()
	{
		auto result = this->consumer.TryReadUint32LE();
		if (!result.has_value()) throw OTTDByteReaderSignal();
		return *result;
	}

	/**
	 * Read a single DWord (32 bits).
	 * @note The buffer is NOT advanced.
	 * @returns Value read from buffer.
	 */
	uint32_t PeekDWord()
	{
		auto result = this->consumer.PeekUint32LE();
		if (!result.has_value()) throw OTTDByteReaderSignal();
		return *result;
	}

	uint32_t ReadVarSize(uint8_t size);

	/**
	 * Read a NUL-terminated string.
	 * @returns String read from the buffer.
	 */
	std::string_view ReadString()
	{
		/* Terminating NUL may be missing at the end of sprite. */
		return this->consumer.ReadUntilChar('\0', StringConsumer::SKIP_ONE_SEPARATOR);
	}

	size_t Remaining() const
	{
		return this->consumer.GetBytesLeft();
	}

	bool HasData(size_t count = 1) const
	{
		return count <= this->consumer.GetBytesLeft();
	}

	void Skip(size_t len)
	{
		auto result = this->consumer.Read(len);
		if (result.size() != len) throw OTTDByteReaderSignal();
	}

};

#endif /* NEWGRF_BYTEREADER_H */
