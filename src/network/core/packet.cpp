/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file packet.cpp Basic functions to create, fill and read packets.
 */

#include "../../stdafx.h"
#include "../../string_func.h"

#include "packet.h"

#include "../../safeguards.h"

/**
 * Create a packet that is used to read from a network socket
 * @param cs the socket handler associated with the socket we are reading from
 */
Packet::Packet(NetworkSocketHandler *cs)
{
	assert(cs != nullptr);

	this->cs     = cs;
	this->next   = nullptr;
	this->pos    = 0; // We start reading from here
	this->size   = 0;
	this->buffer = MallocT<byte>(SEND_MTU);
}

/**
 * Creates a packet to send
 * @param type of the packet to send
 */
Packet::Packet(PacketType type)
{
	this->cs                   = nullptr;
	this->next                 = nullptr;

	/* Skip the size so we can write that in before sending the packet */
	this->pos                  = 0;
	this->size                 = sizeof(PacketSize);
	this->buffer               = MallocT<byte>(SEND_MTU);
	this->buffer[this->size++] = type;
}

/**
 * Free the buffer of this packet.
 */
Packet::~Packet()
{
	free(this->buffer);
}

/**
 * Writes the packet size from the raw packet from packet->size
 */
void Packet::PrepareToSend()
{
	assert(this->cs == nullptr && this->next == nullptr);

	this->buffer[0] = GB(this->size, 0, 8);
	this->buffer[1] = GB(this->size, 8, 8);

	this->pos  = 0; // We start reading from here
}

/*
 * The next couple of functions make sure we can send
 *  uint8, uint16, uint32 and uint64 endian-safe
 *  over the network. The least significant bytes are
 *  sent first.
 *
 *  So 0x01234567 would be sent as 67 45 23 01.
 *
 * A bool is sent as a uint8 where zero means false
 *  and non-zero means true.
 */

/**
 * Package a boolean in the packet.
 * @param data The data to send.
 */
void Packet::Send_bool(bool data)
{
	this->Send_uint8(data ? 1 : 0);
}

/**
 * Package a 8 bits integer in the packet.
 * @param data The data to send.
 */
void Packet::Send_uint8(uint8 data)
{
	assert(this->size < SEND_MTU - sizeof(data));
	this->buffer[this->size++] = data;
}

/**
 * Package a 16 bits integer in the packet.
 * @param data The data to send.
 */
void Packet::Send_uint16(uint16 data)
{
	assert(this->size < SEND_MTU - sizeof(data));
	this->buffer[this->size++] = GB(data, 0, 8);
	this->buffer[this->size++] = GB(data, 8, 8);
}

/**
 * Package a 32 bits integer in the packet.
 * @param data The data to send.
 */
void Packet::Send_uint32(uint32 data)
{
	assert(this->size < SEND_MTU - sizeof(data));
	this->buffer[this->size++] = GB(data,  0, 8);
	this->buffer[this->size++] = GB(data,  8, 8);
	this->buffer[this->size++] = GB(data, 16, 8);
	this->buffer[this->size++] = GB(data, 24, 8);
}

/**
 * Package a 64 bits integer in the packet.
 * @param data The data to send.
 */
void Packet::Send_uint64(uint64 data)
{
	assert(this->size < SEND_MTU - sizeof(data));
	this->buffer[this->size++] = GB(data,  0, 8);
	this->buffer[this->size++] = GB(data,  8, 8);
	this->buffer[this->size++] = GB(data, 16, 8);
	this->buffer[this->size++] = GB(data, 24, 8);
	this->buffer[this->size++] = GB(data, 32, 8);
	this->buffer[this->size++] = GB(data, 40, 8);
	this->buffer[this->size++] = GB(data, 48, 8);
	this->buffer[this->size++] = GB(data, 56, 8);
}

/**
 * Sends a string over the network. It sends out
 * the string + '\0'. No size-byte or something.
 * @param data The string to send
 */
void Packet::Send_string(const char *data)
{
	assert(data != nullptr);
	/* The <= *is* valid due to the fact that we are comparing sizes and not the index. */
	assert(this->size + strlen(data) + 1 <= SEND_MTU);
	while ((this->buffer[this->size++] = *data++) != '\0') {}
}


/*
 * Receiving commands
 * Again, the next couple of functions are endian-safe
 *  see the comment before Send_bool for more info.
 */


/**
 * Is it safe to read from the packet, i.e. didn't we run over the buffer ?
 * @param bytes_to_read The amount of bytes we want to try to read.
 * @return True if that is safe, otherwise false.
 */
bool Packet::CanReadFromPacket(uint bytes_to_read)
{
	/* Don't allow reading from a quit client/client who send bad data */
	if (this->cs->HasClientQuit()) return false;

	/* Check if variable is within packet-size */
	if (this->pos + bytes_to_read > this->size) {
		this->cs->NetworkSocketHandler::CloseConnection();
		return false;
	}

	return true;
}

/**
 * Reads the packet size from the raw packet and stores it in the packet->size
 */
void Packet::ReadRawPacketSize()
{
	assert(this->cs != nullptr && this->next == nullptr);
	this->size  = (PacketSize)this->buffer[0];
	this->size += (PacketSize)this->buffer[1] << 8;
}

/**
 * Prepares the packet so it can be read
 */
void Packet::PrepareToRead()
{
	this->ReadRawPacketSize();

	/* Put the position on the right place */
	this->pos = sizeof(PacketSize);
}

/**
 * Read a boolean from the packet.
 * @return The read data.
 */
bool Packet::Recv_bool()
{
	return this->Recv_uint8() != 0;
}

/**
 * Read a 8 bits integer from the packet.
 * @return The read data.
 */
uint8 Packet::Recv_uint8()
{
	uint8 n;

	if (!this->CanReadFromPacket(sizeof(n))) return 0;

	n = this->buffer[this->pos++];
	return n;
}

/**
 * Read a 16 bits integer from the packet.
 * @return The read data.
 */
uint16 Packet::Recv_uint16()
{
	uint16 n;

	if (!this->CanReadFromPacket(sizeof(n))) return 0;

	n  = (uint16)this->buffer[this->pos++];
	n += (uint16)this->buffer[this->pos++] << 8;
	return n;
}

/**
 * Read a 32 bits integer from the packet.
 * @return The read data.
 */
uint32 Packet::Recv_uint32()
{
	uint32 n;

	if (!this->CanReadFromPacket(sizeof(n))) return 0;

	n  = (uint32)this->buffer[this->pos++];
	n += (uint32)this->buffer[this->pos++] << 8;
	n += (uint32)this->buffer[this->pos++] << 16;
	n += (uint32)this->buffer[this->pos++] << 24;
	return n;
}

/**
 * Read a 64 bits integer from the packet.
 * @return The read data.
 */
uint64 Packet::Recv_uint64()
{
	uint64 n;

	if (!this->CanReadFromPacket(sizeof(n))) return 0;

	n  = (uint64)this->buffer[this->pos++];
	n += (uint64)this->buffer[this->pos++] << 8;
	n += (uint64)this->buffer[this->pos++] << 16;
	n += (uint64)this->buffer[this->pos++] << 24;
	n += (uint64)this->buffer[this->pos++] << 32;
	n += (uint64)this->buffer[this->pos++] << 40;
	n += (uint64)this->buffer[this->pos++] << 48;
	n += (uint64)this->buffer[this->pos++] << 56;
	return n;
}

/**
 * Reads a string till it finds a '\0' in the stream.
 * @param buffer The buffer to put the data into.
 * @param size   The size of the buffer.
 * @param settings The string validation settings.
 */
void Packet::Recv_string(char *buffer, size_t size, StringValidationSettings settings)
{
	PacketSize pos;
	char *bufp = buffer;
	const char *last = buffer + size - 1;

	/* Don't allow reading from a closed socket */
	if (cs->HasClientQuit()) return;

	pos = this->pos;
	while (--size > 0 && pos < this->size && (*buffer++ = this->buffer[pos++]) != '\0') {}

	if (size == 0 || pos == this->size) {
		*buffer = '\0';
		/* If size was sooner to zero then the string in the stream
		 *  skip till the \0, so than packet can be read out correctly for the rest */
		while (pos < this->size && this->buffer[pos] != '\0') pos++;
		pos++;
	}
	this->pos = pos;

	str_validate(bufp, last, settings);
}
