/* $Id$ */

/**
 * @file packet.h Basic functions to create, fill and read packets.
 */

#ifndef NETWORK_CORE_PACKET_H
#define NETWORK_CORE_PACKET_H

#ifdef ENABLE_NETWORK

#include "config.h"
#include "core.h"

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
 */
struct Packet {
	/** The next packet. Used for queueing packets before sending. */
	Packet *next;
	/** The size of the whole packet for received packets. For packets
	 * that will be sent, the value is filled in just before the
	 * actual transmission. */
	PacketSize size;
	/** The current read/write position in the packet */
	PacketSize pos;
	/** The buffer of this packet */
	byte buffer[SEND_MTU];
private:
	NetworkSocketHandler *cs;

public:
	Packet(NetworkSocketHandler *cs);
	Packet(PacketType type);

	/* Sending/writing of packets */
	void PrepareToSend(void);

	void Send_bool  (bool   data);
	void Send_uint8 (uint8  data);
	void Send_uint16(uint16 data);
	void Send_uint32(uint32 data);
	void Send_uint64(uint64 data);
	void Send_string(const char* data);

	/* Reading/receiving of packets */
	void ReadRawPacketSize(void);
	void PrepareToRead(void);

	bool   CanReadFromPacket (uint bytes_to_read);
	bool   Recv_bool  (void);
	uint8  Recv_uint8 (void);
	uint16 Recv_uint16(void);
	uint32 Recv_uint32(void);
	uint64 Recv_uint64(void);
	void   Recv_string(char* buffer, size_t size);
};

Packet *NetworkSend_Init(PacketType type);

#endif /* ENABLE_NETWORK */

#endif /* NETWORK_CORE_PACKET_H */
