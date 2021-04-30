/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file core/udp.cpp Basic functions to receive and send UDP packets.
 */

#include "../../stdafx.h"
#include "../../date_func.h"
#include "../../debug.h"
#include "game_info.h"
#include "udp.h"

#include "../../safeguards.h"

/**
 * Create an UDP socket but don't listen yet.
 * @param bind the addresses to bind to.
 */
NetworkUDPSocketHandler::NetworkUDPSocketHandler(NetworkAddressList *bind)
{
	if (bind != nullptr) {
		for (NetworkAddress &addr : *bind) {
			this->bind.push_back(addr);
		}
	} else {
		/* As hostname nullptr and port 0/nullptr don't go well when
		 * resolving it we need to add an address for each of
		 * the address families we support. */
		this->bind.emplace_back(nullptr, 0, AF_INET);
		this->bind.emplace_back(nullptr, 0, AF_INET6);
	}
}


/**
 * Start listening on the given host and port.
 * @return true if at least one port is listening
 */
bool NetworkUDPSocketHandler::Listen()
{
	/* Make sure socket is closed */
	this->Close();

	for (NetworkAddress &addr : this->bind) {
		addr.Listen(SOCK_DGRAM, &this->sockets);
	}

	return this->sockets.size() != 0;
}

/**
 * Close the given UDP socket
 */
void NetworkUDPSocketHandler::Close()
{
	for (auto &s : this->sockets) {
		closesocket(s.second);
	}
	this->sockets.clear();
}

NetworkRecvStatus NetworkUDPSocketHandler::CloseConnection(bool error)
{
	NetworkSocketHandler::CloseConnection(error);
	return NETWORK_RECV_STATUS_OKAY;
}

/**
 * Send a packet over UDP
 * @param p    the packet to send
 * @param recv the receiver (target) of the packet
 * @param all  send the packet using all sockets that can send it
 * @param broadcast whether to send a broadcast message
 */
void NetworkUDPSocketHandler::SendPacket(Packet *p, NetworkAddress *recv, bool all, bool broadcast)
{
	if (this->sockets.size() == 0) this->Listen();

	for (auto &s : this->sockets) {
		/* Make a local copy because if we resolve it we cannot
		 * easily unresolve it so we can resolve it later again. */
		NetworkAddress send(*recv);

		/* Not the same type */
		if (!send.IsFamily(s.first.GetAddress()->ss_family)) continue;

		p->PrepareToSend();

		if (broadcast) {
			/* Enable broadcast */
			unsigned long val = 1;
			if (setsockopt(s.second, SOL_SOCKET, SO_BROADCAST, (char *) &val, sizeof(val)) < 0) {
				DEBUG(net, 1, "[udp] setting broadcast failed with: %s", NetworkError::GetLast().AsString());
			}
		}

		/* Send the buffer */
		int res = sendto(s.second, (const char*)p->buffer, p->size, 0, (const struct sockaddr *)send.GetAddress(), send.GetAddressLength());
		DEBUG(net, 7, "[udp] sendto(%s)", send.GetAddressAsString().c_str());

		/* Check for any errors, but ignore it otherwise */
		if (res == -1) DEBUG(net, 1, "[udp] sendto(%s) failed with: %s", send.GetAddressAsString().c_str(), NetworkError::GetLast().AsString());

		if (!all) break;
	}
}

/**
 * Receive a packet at UDP level
 */
void NetworkUDPSocketHandler::ReceivePackets()
{
	for (auto &s : this->sockets) {
		for (int i = 0; i < 1000; i++) { // Do not infinitely loop when DoSing with UDP
			struct sockaddr_storage client_addr;
			memset(&client_addr, 0, sizeof(client_addr));

			Packet p(this);
			socklen_t client_len = sizeof(client_addr);

			/* Try to receive anything */
			SetNonBlocking(s.second); // Some OSes seem to lose the non-blocking status of the socket
			int nbytes = recvfrom(s.second, (char*)p.buffer, SEND_MTU, 0, (struct sockaddr *)&client_addr, &client_len);

			/* Did we get the bytes for the base header of the packet? */
			if (nbytes <= 0) break;    // No data, i.e. no packet
			if (nbytes <= 2) continue; // Invalid data; try next packet
#ifdef __EMSCRIPTEN__
			client_len = FixAddrLenForEmscripten(client_addr);
#endif

			NetworkAddress address(client_addr, client_len);
			p.PrepareToRead();

			/* If the size does not match the packet must be corrupted.
			 * Otherwise it will be marked as corrupted later on. */
			if (nbytes != p.size) {
				DEBUG(net, 1, "received a packet with mismatching size from %s", address.GetAddressAsString().c_str());
				continue;
			}

			/* Handle the packet */
			this->HandleUDPPacket(&p, &address);
		}
	}
}

/**
 * Handle an incoming packets by sending it to the correct function.
 * @param p the received packet
 * @param client_addr the sender of the packet
 */
void NetworkUDPSocketHandler::HandleUDPPacket(Packet *p, NetworkAddress *client_addr)
{
	PacketUDPType type;

	/* New packet == new client, which has not quit yet */
	this->Reopen();

	type = (PacketUDPType)p->Recv_uint8();

	switch (this->HasClientQuit() ? PACKET_UDP_END : type) {
		case PACKET_UDP_CLIENT_FIND_SERVER:   this->Receive_CLIENT_FIND_SERVER(p, client_addr);   break;
		case PACKET_UDP_SERVER_RESPONSE:      this->Receive_SERVER_RESPONSE(p, client_addr);      break;
		case PACKET_UDP_CLIENT_DETAIL_INFO:   this->Receive_CLIENT_DETAIL_INFO(p, client_addr);   break;
		case PACKET_UDP_SERVER_DETAIL_INFO:   this->Receive_SERVER_DETAIL_INFO(p, client_addr);   break;
		case PACKET_UDP_SERVER_REGISTER:      this->Receive_SERVER_REGISTER(p, client_addr);      break;
		case PACKET_UDP_MASTER_ACK_REGISTER:  this->Receive_MASTER_ACK_REGISTER(p, client_addr);  break;
		case PACKET_UDP_CLIENT_GET_LIST:      this->Receive_CLIENT_GET_LIST(p, client_addr);      break;
		case PACKET_UDP_MASTER_RESPONSE_LIST: this->Receive_MASTER_RESPONSE_LIST(p, client_addr); break;
		case PACKET_UDP_SERVER_UNREGISTER:    this->Receive_SERVER_UNREGISTER(p, client_addr);    break;
		case PACKET_UDP_CLIENT_GET_NEWGRFS:   this->Receive_CLIENT_GET_NEWGRFS(p, client_addr);   break;
		case PACKET_UDP_SERVER_NEWGRFS:       this->Receive_SERVER_NEWGRFS(p, client_addr);       break;
		case PACKET_UDP_MASTER_SESSION_KEY:   this->Receive_MASTER_SESSION_KEY(p, client_addr);   break;

		default:
			if (this->HasClientQuit()) {
				DEBUG(net, 0, "[udp] received invalid packet type %d from %s", type, client_addr->GetAddressAsString().c_str());
			} else {
				DEBUG(net, 0, "[udp] received illegal packet from %s", client_addr->GetAddressAsString().c_str());
			}
			break;
	}
}

/**
 * Helper for logging receiving invalid packets.
 * @param type The received packet type.
 * @param client_addr The address we received the packet from.
 */
void NetworkUDPSocketHandler::ReceiveInvalidPacket(PacketUDPType type, NetworkAddress *client_addr)
{
	DEBUG(net, 0, "[udp] received packet type %d on wrong port from %s", type, client_addr->GetAddressAsString().c_str());
}

void NetworkUDPSocketHandler::Receive_CLIENT_FIND_SERVER(Packet *p, NetworkAddress *client_addr) { this->ReceiveInvalidPacket(PACKET_UDP_CLIENT_FIND_SERVER, client_addr); }
void NetworkUDPSocketHandler::Receive_SERVER_RESPONSE(Packet *p, NetworkAddress *client_addr) { this->ReceiveInvalidPacket(PACKET_UDP_SERVER_RESPONSE, client_addr); }
void NetworkUDPSocketHandler::Receive_CLIENT_DETAIL_INFO(Packet *p, NetworkAddress *client_addr) { this->ReceiveInvalidPacket(PACKET_UDP_CLIENT_DETAIL_INFO, client_addr); }
void NetworkUDPSocketHandler::Receive_SERVER_DETAIL_INFO(Packet *p, NetworkAddress *client_addr) { this->ReceiveInvalidPacket(PACKET_UDP_SERVER_DETAIL_INFO, client_addr); }
void NetworkUDPSocketHandler::Receive_SERVER_REGISTER(Packet *p, NetworkAddress *client_addr) { this->ReceiveInvalidPacket(PACKET_UDP_SERVER_REGISTER, client_addr); }
void NetworkUDPSocketHandler::Receive_MASTER_ACK_REGISTER(Packet *p, NetworkAddress *client_addr) { this->ReceiveInvalidPacket(PACKET_UDP_MASTER_ACK_REGISTER, client_addr); }
void NetworkUDPSocketHandler::Receive_CLIENT_GET_LIST(Packet *p, NetworkAddress *client_addr) { this->ReceiveInvalidPacket(PACKET_UDP_CLIENT_GET_LIST, client_addr); }
void NetworkUDPSocketHandler::Receive_MASTER_RESPONSE_LIST(Packet *p, NetworkAddress *client_addr) { this->ReceiveInvalidPacket(PACKET_UDP_MASTER_RESPONSE_LIST, client_addr); }
void NetworkUDPSocketHandler::Receive_SERVER_UNREGISTER(Packet *p, NetworkAddress *client_addr) { this->ReceiveInvalidPacket(PACKET_UDP_SERVER_UNREGISTER, client_addr); }
void NetworkUDPSocketHandler::Receive_CLIENT_GET_NEWGRFS(Packet *p, NetworkAddress *client_addr) { this->ReceiveInvalidPacket(PACKET_UDP_CLIENT_GET_NEWGRFS, client_addr); }
void NetworkUDPSocketHandler::Receive_SERVER_NEWGRFS(Packet *p, NetworkAddress *client_addr) { this->ReceiveInvalidPacket(PACKET_UDP_SERVER_NEWGRFS, client_addr); }
void NetworkUDPSocketHandler::Receive_MASTER_SESSION_KEY(Packet *p, NetworkAddress *client_addr) { this->ReceiveInvalidPacket(PACKET_UDP_MASTER_SESSION_KEY, client_addr); }
