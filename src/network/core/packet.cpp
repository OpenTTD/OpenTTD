/* $Id$ */

#ifdef ENABLE_NETWORK

#include "../../stdafx.h"
#include "../../macros.h"
#include "../../string.h"
#include "../../helpers.hpp"
#include "../network_data.h"

#include "packet.h"

/**
 * @file packet.h Basic functions to create, fill and read packets.
 */


/* Do not want to include functions.h and all required headers */
extern void NORETURN CDECL error(const char *str, ...);


/**
 * Create a packet for sending
 * @param type the of packet
 * @return the newly created packet
 */
Packet *NetworkSend_Init(const PacketType type)
{
	Packet *packet;
	MallocT(&packet, 1);
	/* An error is inplace here, because it simply means we ran out of memory. */
	if (packet == NULL) error("Failed to allocate Packet");

	/* Skip the size so we can write that in before sending the packet */
	packet->size = sizeof(packet->size);
	packet->buffer[packet->size++] = type;
	packet->pos = 0;

	return packet;
}

/**
 * Writes the packet size from the raw packet from packet->size
 * @param packet the packet to write the size of
 */
void NetworkSend_FillPacketSize(Packet *packet)
{
	packet->buffer[0] = GB(packet->size, 0, 8);
	packet->buffer[1] = GB(packet->size, 8, 8);
}

/**
 * The next couple of functions make sure we can send
 *  uint8, uint16, uint32 and uint64 endian-safe
 *  over the network. The least significant bytes are
 *  sent first.
 *
 *  So 0x01234567 would be sent as 67 45 23 01.
 */

void NetworkSend_uint8(Packet *packet, uint8 data)
{
	assert(packet->size < sizeof(packet->buffer) - sizeof(data));
	packet->buffer[packet->size++] = data;
}

void NetworkSend_uint16(Packet *packet, uint16 data)
{
	assert(packet->size < sizeof(packet->buffer) - sizeof(data));
	packet->buffer[packet->size++] = GB(data, 0, 8);
	packet->buffer[packet->size++] = GB(data, 8, 8);
}

void NetworkSend_uint32(Packet *packet, uint32 data)
{
	assert(packet->size < sizeof(packet->buffer) - sizeof(data));
	packet->buffer[packet->size++] = GB(data,  0, 8);
	packet->buffer[packet->size++] = GB(data,  8, 8);
	packet->buffer[packet->size++] = GB(data, 16, 8);
	packet->buffer[packet->size++] = GB(data, 24, 8);
}

void NetworkSend_uint64(Packet *packet, uint64 data)
{
	assert(packet->size < sizeof(packet->buffer) - sizeof(data));
	packet->buffer[packet->size++] = GB(data,  0, 8);
	packet->buffer[packet->size++] = GB(data,  8, 8);
	packet->buffer[packet->size++] = GB(data, 16, 8);
	packet->buffer[packet->size++] = GB(data, 24, 8);
	packet->buffer[packet->size++] = GB(data, 32, 8);
	packet->buffer[packet->size++] = GB(data, 40, 8);
	packet->buffer[packet->size++] = GB(data, 48, 8);
	packet->buffer[packet->size++] = GB(data, 56, 8);
}

/**
 *  Sends a string over the network. It sends out
 *  the string + '\0'. No size-byte or something.
 */
void NetworkSend_string(Packet *packet, const char* data)
{
	assert(data != NULL);
	assert(packet->size < sizeof(packet->buffer) - strlen(data) - 1);
	while ((packet->buffer[packet->size++] = *data++) != '\0') {}
}


/**
 * Receiving commands
 * Again, the next couple of functions are endian-safe
 *  see the comment before NetworkSend_uint8 for more info.
 */


extern NetworkRecvStatus CloseConnection(NetworkClientState *cs);

/** Is it safe to read from the packet, i.e. didn't we run over the buffer ? */
static inline bool CanReadFromPacket(NetworkClientState *cs, const Packet *packet, const uint bytes_to_read)
{
	/* Don't allow reading from a closed socket */
	if (HasClientQuit(cs)) return false;

	/* Check if variable is within packet-size */
	if (packet->pos + bytes_to_read > packet->size) {
		CloseConnection(cs);
		return false;
	}

	return true;
}

/**
 * Reads the packet size from the raw packet and stores it in the packet->size
 * @param packet the packet to read the size of
 */
void NetworkRecv_ReadPacketSize(Packet *packet)
{
	packet->size  = (uint16)packet->buffer[0];
	packet->size += (uint16)packet->buffer[1] << 8;
}

uint8 NetworkRecv_uint8(NetworkClientState *cs, Packet *packet)
{
	uint8 n;

	if (!CanReadFromPacket(cs, packet, sizeof(n))) return 0;

	n = packet->buffer[packet->pos++];
	return n;
}

uint16 NetworkRecv_uint16(NetworkClientState *cs, Packet *packet)
{
	uint16 n;

	if (!CanReadFromPacket(cs, packet, sizeof(n))) return 0;

	n  = (uint16)packet->buffer[packet->pos++];
	n += (uint16)packet->buffer[packet->pos++] << 8;
	return n;
}

uint32 NetworkRecv_uint32(NetworkClientState *cs, Packet *packet)
{
	uint32 n;

	if (!CanReadFromPacket(cs, packet, sizeof(n))) return 0;

	n  = (uint32)packet->buffer[packet->pos++];
	n += (uint32)packet->buffer[packet->pos++] << 8;
	n += (uint32)packet->buffer[packet->pos++] << 16;
	n += (uint32)packet->buffer[packet->pos++] << 24;
	return n;
}

uint64 NetworkRecv_uint64(NetworkClientState *cs, Packet *packet)
{
	uint64 n;

	if (!CanReadFromPacket(cs, packet, sizeof(n))) return 0;

	n  = (uint64)packet->buffer[packet->pos++];
	n += (uint64)packet->buffer[packet->pos++] << 8;
	n += (uint64)packet->buffer[packet->pos++] << 16;
	n += (uint64)packet->buffer[packet->pos++] << 24;
	n += (uint64)packet->buffer[packet->pos++] << 32;
	n += (uint64)packet->buffer[packet->pos++] << 40;
	n += (uint64)packet->buffer[packet->pos++] << 48;
	n += (uint64)packet->buffer[packet->pos++] << 56;
	return n;
}

/** Reads a string till it finds a '\0' in the stream */
void NetworkRecv_string(NetworkClientState *cs, Packet *p, char *buffer, size_t size)
{
	PacketSize pos;
	char *bufp = buffer;

	/* Don't allow reading from a closed socket */
	if (HasClientQuit(cs)) return;

	pos = p->pos;
	while (--size > 0 && pos < p->size && (*buffer++ = p->buffer[pos++]) != '\0') {}

	if (size == 0 || pos == p->size) {
		*buffer = '\0';
		/* If size was sooner to zero then the string in the stream
		 *  skip till the \0, so than packet can be read out correctly for the rest */
		while (pos < p->size && p->buffer[pos] != '\0') pos++;
		pos++;
	}
	p->pos = pos;

	str_validate(bufp);
}

#endif /* ENABLE_NETWORK */
