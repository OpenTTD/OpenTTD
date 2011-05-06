/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file tcp_content.cpp Basic functions to receive and send Content packets.
 */

#ifdef ENABLE_NETWORK

#include "../../stdafx.h"
#include "tcp_content.h"

/** Clear everything in the struct */
ContentInfo::ContentInfo()
{
	memset(this, 0, sizeof(*this));
}

/** Free everything allocated */
ContentInfo::~ContentInfo()
{
	free(this->dependencies);
	free(this->tags);
}

/**
 * Copy data from other #ContentInfo and take ownership of allocated stuff.
 * @param other Source to copy from. #dependencies and #tags will be NULLed.
 */
void ContentInfo::TransferFrom(ContentInfo *other)
{
	if (other != this) {
		free(this->dependencies);
		free(this->tags);
		memcpy(this, other, sizeof(ContentInfo));
		other->dependencies = NULL;
		other->tags = NULL;
	}
}

/**
 * Get the size of the data as send over the network.
 * @return the size.
 */
size_t ContentInfo::Size() const
{
	size_t len = 0;
	for (uint i = 0; i < this->tag_count; i++) len += strlen(this->tags[i]) + 1;

	/* The size is never larger than the content info size plus the size of the
	 * tags and dependencies */
	return sizeof(*this) +
			sizeof(this->dependency_count) +
			sizeof(*this->dependencies) * this->dependency_count;
}

/**
 * Is the state either selected or autoselected?
 * @return true iff that's the case
 */
bool ContentInfo::IsSelected() const
{
	switch (this->state) {
		case ContentInfo::SELECTED:
		case ContentInfo::AUTOSELECTED:
		case ContentInfo::ALREADY_HERE:
			return true;

		default:
			return false;
	}
}

/**
 * Is the information from this content info valid?
 * @return true iff it's valid
 */
bool ContentInfo::IsValid() const
{
	return this->state < ContentInfo::INVALID && this->type >= CONTENT_TYPE_BEGIN && this->type < CONTENT_TYPE_END;
}

void NetworkContentSocketHandler::Close()
{
	CloseConnection();
	if (this->sock == INVALID_SOCKET) return;

	closesocket(this->sock);
	this->sock = INVALID_SOCKET;
}

/**
 * Defines a simple (switch) case for each network packet
 * @param type the packet type to create the case for
 */
#define CONTENT_COMMAND(type) case type: return this->NetworkPacketReceive_ ## type ## _command(p); break;

/**
 * Handle the given packet, i.e. pass it to the right
 * parser receive command.
 * @param p the packet to handle
 * @return true if we should immediately handle further packets, false otherwise
 */
bool NetworkContentSocketHandler::HandlePacket(Packet *p)
{
	PacketContentType type = (PacketContentType)p->Recv_uint8();

	switch (this->HasClientQuit() ? PACKET_CONTENT_END : type) {
		case PACKET_CONTENT_CLIENT_INFO_LIST:      return this->Receive_CLIENT_INFO_LIST(p);
		case PACKET_CONTENT_CLIENT_INFO_ID:        return this->Receive_CLIENT_INFO_ID(p);
		case PACKET_CONTENT_CLIENT_INFO_EXTID:     return this->Receive_CLIENT_INFO_EXTID(p);
		case PACKET_CONTENT_CLIENT_INFO_EXTID_MD5: return this->Receive_CLIENT_INFO_EXTID_MD5(p);
		case PACKET_CONTENT_SERVER_INFO:           return this->Receive_SERVER_INFO(p);
		case PACKET_CONTENT_CLIENT_CONTENT:        return this->Receive_CLIENT_CONTENT(p);
		case PACKET_CONTENT_SERVER_CONTENT:        return this->Receive_SERVER_CONTENT(p);

		default:
			if (this->HasClientQuit()) {
				DEBUG(net, 0, "[tcp/content] received invalid packet type %d from %s", type, this->client_addr.GetAddressAsString());
			} else {
				DEBUG(net, 0, "[tcp/content] received illegal packet from %s", this->client_addr.GetAddressAsString());
			}
			return false;
	}
}

/**
 * Receive a packet at TCP level
 */
void NetworkContentSocketHandler::ReceivePackets()
{
	Packet *p;
	while ((p = this->ReceivePacket()) != NULL) {
		bool cont = this->HandlePacket(p);
		delete p;
		if (!cont) return;
	}
}


/**
 * Helper for logging receiving invalid packets.
 * @param type The received packet type.
 * @return Always false, as it's an error.
 */
bool NetworkContentSocketHandler::ReceiveInvalidPacket(PacketContentType type)
{
	DEBUG(net, 0, "[tcp/content] received illegal packet type %d from %s", type, this->client_addr.GetAddressAsString());
	return false;
}

bool NetworkContentSocketHandler::Receive_CLIENT_INFO_LIST(Packet *p) { return this->ReceiveInvalidPacket(PACKET_CONTENT_CLIENT_INFO_LIST); }
bool NetworkContentSocketHandler::Receive_CLIENT_INFO_ID(Packet *p) { return this->ReceiveInvalidPacket(PACKET_CONTENT_CLIENT_INFO_ID); }
bool NetworkContentSocketHandler::Receive_CLIENT_INFO_EXTID(Packet *p) { return this->ReceiveInvalidPacket(PACKET_CONTENT_CLIENT_INFO_EXTID); }
bool NetworkContentSocketHandler::Receive_CLIENT_INFO_EXTID_MD5(Packet *p) { return this->ReceiveInvalidPacket(PACKET_CONTENT_CLIENT_INFO_EXTID_MD5); }
bool NetworkContentSocketHandler::Receive_SERVER_INFO(Packet *p) { return this->ReceiveInvalidPacket(PACKET_CONTENT_SERVER_INFO); }
bool NetworkContentSocketHandler::Receive_CLIENT_CONTENT(Packet *p) { return this->ReceiveInvalidPacket(PACKET_CONTENT_CLIENT_CONTENT); }
bool NetworkContentSocketHandler::Receive_SERVER_CONTENT(Packet *p) { return this->ReceiveInvalidPacket(PACKET_CONTENT_SERVER_CONTENT); }

#endif /* ENABLE_NETWORK */
