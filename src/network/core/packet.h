/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file packet.h Basic functions to create, fill and read packets.
 */

#ifndef NETWORK_CORE_PACKET_H
#define NETWORK_CORE_PACKET_H

#include "config.h"
#include "core.h"
#include "../../string_type.h"

typedef uint16 PacketSize; ///< Size of the whole packet.
typedef uint8  PacketType; ///< Identifier for the packet

/**
 * Internal entity of a packet. As everything is sent as a packet,
 * all network communication will need to call the functions that
 * populate the packet.
 * Every packet can be at most SEND_MTU bytes. Overflowing this
 * limit will give an assertion when sending (i.e. writing) the
 * packet. Reading past the size of the packet when receiving
 * will return all 0 values and "" in case of the string.
 *
 * --- Points of attention ---
 *  - all > 1 byte integral values are written in little endian,
 *    unless specified otherwise.
 *      Thus, 0x01234567 would be sent as {0x67, 0x45, 0x23, 0x01}.
 *  - all sent strings are of variable length and terminated by a '\0'.
 *      Thus, the length of the strings is not sent.
 *  - years that are leap years in the 'days since X' to 'date' calculations:
 *     (year % 4 == 0) and ((year % 100 != 0) or (year % 400 == 0))
 */
struct Packet {
	/** The next packet. Used for queueing packets before sending. */
	Packet *next;
	/**
	 * The size of the whole packet for received packets. For packets
	 * that will be sent, the value is filled in just before the
	 * actual transmission.
	 */
	PacketSize size;
	/** The current read/write position in the packet */
	PacketSize pos;
	/** The buffer of this packet, of basically variable length up to SEND_MTU. */
	byte *buffer;

private:
	/** Socket we're associated with. */
	NetworkSocketHandler *cs;

public:
	Packet(NetworkSocketHandler *cs);
	Packet(PacketType type);
	~Packet();

	/* Sending/writing of packets */
	void PrepareToSend();

	void Send_bool  (bool   data);
	void Send_uint8 (uint8  data);
	void Send_uint16(uint16 data);
	void Send_uint32(uint32 data);
	void Send_uint64(uint64 data);
	void Send_string(const char *data);

	/* Reading/receiving of packets */
	void ReadRawPacketSize();
	void PrepareToRead();

	bool   CanReadFromPacket (uint bytes_to_read);
	bool   Recv_bool  ();
	uint8  Recv_uint8 ();
	uint16 Recv_uint16();
	uint32 Recv_uint32();
	uint64 Recv_uint64();
	void   Recv_string(char *buffer, size_t size, StringValidationSettings settings = SVS_REPLACE_WITH_QUESTION_MARK);
};

#endif /* NETWORK_CORE_PACKET_H */
