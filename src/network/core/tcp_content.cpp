/* $Id$ */

/**
 * @file tcp_content.cpp Basic functions to receive and send Content packets.
 */

#ifdef ENABLE_NETWORK

#include "../../stdafx.h"
#include "../../debug.h"
#include "tcp_content.h"

ContentInfo::ContentInfo()
{
	memset(this, 0, sizeof(*this));
}

ContentInfo::~ContentInfo()
{
	free(this->dependencies);
	free(this->tags);
}

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

bool ContentInfo::IsSelected() const
{
	switch (this->state) {
		case ContentInfo::SELECTED:
		case ContentInfo::AUTOSELECTED:
			return true;

		default:
			return false;
	}
}

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
 * Handle an incoming packets by sending it to the correct function.
 * @param p the received packet
 */
bool NetworkContentSocketHandler::HandlePacket(Packet *p)
{
	PacketContentType type = (PacketContentType)p->Recv_uint8();

	switch (this->HasClientQuit() ? PACKET_CONTENT_END : type) {
		CONTENT_COMMAND(PACKET_CONTENT_CLIENT_INFO_LIST);
		CONTENT_COMMAND(PACKET_CONTENT_CLIENT_INFO_ID);
		CONTENT_COMMAND(PACKET_CONTENT_CLIENT_INFO_EXTID);
		CONTENT_COMMAND(PACKET_CONTENT_CLIENT_INFO_EXTID_MD5);
		CONTENT_COMMAND(PACKET_CONTENT_SERVER_INFO);
		CONTENT_COMMAND(PACKET_CONTENT_CLIENT_CONTENT);
		CONTENT_COMMAND(PACKET_CONTENT_SERVER_CONTENT);

		default:
			if (this->HasClientQuit()) {
				DEBUG(net, 0, "[tcp/content] received invalid packet type %d from %s:%d", type,  inet_ntoa(this->client_addr.sin_addr), ntohs(this->client_addr.sin_port));
			} else {
				DEBUG(net, 0, "[tcp/content] received illegal packet from %s:%d", inet_ntoa(this->client_addr.sin_addr), ntohs(this->client_addr.sin_port));
			}
			return false;
	}
}

/**
 * Receive a packet at UDP level
 */
void NetworkContentSocketHandler::Recv_Packets()
{
	Packet *p;
	NetworkRecvStatus res;
	while ((p = this->Recv_Packet(&res)) != NULL) {
		bool cont = HandlePacket(p);
		delete p;
		if (!cont) return;
	}
}

/**
 * Create stub implementations for all receive commands that only
 * show a warning that the given command is not available for the
 * socket where the packet came from.
 * @param type the packet type to create the stub for
 */
#define DEFINE_UNAVAILABLE_CONTENT_RECEIVE_COMMAND(type) \
bool NetworkContentSocketHandler::NetworkPacketReceive_## type ##_command(Packet *p) { \
	DEBUG(net, 0, "[tcp/content] received illegal packet type %d from %s:%d", \
			type, inet_ntoa(this->client_addr.sin_addr), ntohs(this->client_addr.sin_port)); \
	return false; \
}

DEFINE_UNAVAILABLE_CONTENT_RECEIVE_COMMAND(PACKET_CONTENT_CLIENT_INFO_LIST);
DEFINE_UNAVAILABLE_CONTENT_RECEIVE_COMMAND(PACKET_CONTENT_CLIENT_INFO_ID);
DEFINE_UNAVAILABLE_CONTENT_RECEIVE_COMMAND(PACKET_CONTENT_CLIENT_INFO_EXTID);
DEFINE_UNAVAILABLE_CONTENT_RECEIVE_COMMAND(PACKET_CONTENT_CLIENT_INFO_EXTID_MD5);
DEFINE_UNAVAILABLE_CONTENT_RECEIVE_COMMAND(PACKET_CONTENT_SERVER_INFO);
DEFINE_UNAVAILABLE_CONTENT_RECEIVE_COMMAND(PACKET_CONTENT_CLIENT_CONTENT);
DEFINE_UNAVAILABLE_CONTENT_RECEIVE_COMMAND(PACKET_CONTENT_SERVER_CONTENT);

#endif /* ENABLE_NETWORK */
