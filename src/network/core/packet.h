/* $Id$ */

#ifndef NETWORK_CORE_PACKET_H
#define NETWORK_CORE_PACKET_H

#ifdef ENABLE_NETWORK

#include "config.h"

/**
 * @file packet.h Basic functions to create, fill and read packets.
 */

typedef struct NetworkClientState NetworkClientState;

/**
 * Queries the network client state struct to determine whether
 * the client has quit. It indirectly also queries whether the
 * packet is corrupt as the connection will be closed if it is
 * reading beyond the boundary of the received packet.
 * @param cs the state to query
 * @param true if the connection should be considered dropped
 */
bool HasClientQuit(const NetworkClientState *cs);

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
typedef struct Packet {
	/** The next packet. Used for queueing packets before sending. */
	struct Packet *next;
	/** The size of the whole packet for received packets. For packets
	 * that will be sent, the value is filled in just before the
	 * actual transmission. */
	PacketSize size;
	/** The current read/write position in the packet */
	PacketSize pos;
	/** The buffer of this packet */
	byte buffer[SEND_MTU];
} Packet;


Packet *NetworkSend_Init(const PacketType type);
void NetworkSend_FillPacketSize(Packet *packet);
void NetworkSend_uint8 (Packet *packet, uint8 data);
void NetworkSend_uint16(Packet *packet, uint16 data);
void NetworkSend_uint32(Packet *packet, uint32 data);
void NetworkSend_uint64(Packet *packet, uint64 data);
void NetworkSend_string(Packet *packet, const char* data);

void NetworkRecv_ReadPacketSize(Packet *packet);
uint8  NetworkRecv_uint8 (NetworkClientState *cs, Packet *packet);
uint16 NetworkRecv_uint16(NetworkClientState *cs, Packet *packet);
uint32 NetworkRecv_uint32(NetworkClientState *cs, Packet *packet);
uint64 NetworkRecv_uint64(NetworkClientState *cs, Packet *packet);
void   NetworkRecv_string(NetworkClientState *cs, Packet *packet, char* buffer, size_t size);

#endif /* ENABLE_NETWORK */

#endif /* NETWORK_CORE_PACKET_H */
