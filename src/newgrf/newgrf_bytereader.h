/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_bytereader.h NewGRF buffer reader definition. */

#ifndef NEWGRF_BYTEREADER_H
#define NEWGRF_BYTEREADER_H

class OTTDByteReaderSignal { };

/** Class to read from a NewGRF file */
class ByteReader {
public:
	ByteReader(const uint8_t *data, const uint8_t *end) : data(data), end(end) { }

	const uint8_t *ReadBytes(size_t size)
	{
		if (this->data + size >= this->end) {
			/* Put data at the end, as would happen if every byte had been individually read. */
			this->data = this->end;
			throw OTTDByteReaderSignal();
		}

		const uint8_t *ret = this->data;
		this->data += size;
		return ret;
	}

	/**
	 * Read a single byte (8 bits).
	 * @return Value read from buffer.
	 */
	uint8_t ReadByte()
	{
		if (this->data < this->end) return *this->data++;
		throw OTTDByteReaderSignal();
	}

	/**
	 * Read a single Word (16 bits).
	 * @return Value read from buffer.
	 */
	uint16_t ReadWord()
	{
		uint16_t val = this->ReadByte();
		return val | (this->ReadByte() << 8);
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
		uint32_t val = this->ReadWord();
		return val | (this->ReadWord() << 16);
	}

	uint32_t PeekDWord();
	uint32_t ReadVarSize(uint8_t size);
	std::string_view ReadString();

	size_t Remaining() const
	{
		return this->end - this->data;
	}

	bool HasData(size_t count = 1) const
	{
		return this->data + count <= this->end;
	}

	void Skip(size_t len)
	{
		this->data += len;
		/* It is valid to move the buffer to exactly the end of the data,
		 * as there may not be any more data read. */
		if (this->data > this->end) throw OTTDByteReaderSignal();
	}

private:
	const uint8_t *data; ///< Current position within data.
	const uint8_t *end; ///< Last position of data.
};

#endif /* NEWGRF_BYTEREADER_H */
