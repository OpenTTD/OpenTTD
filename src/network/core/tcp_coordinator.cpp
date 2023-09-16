/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file tcp_coordinator.cpp Basic functions to receive and send Game Coordinator packets.
 */

#include "../../stdafx.h"
#include "../../timer/timer_game_calendar.h"
#include "../../debug.h"
#include "tcp_coordinator.h"

#include "../../safeguards.h"

/**
 * Handle the given packet, i.e. pass it to the right.
 * parser receive command.
 * @param p The packet to handle.
 * @return True iff we should immediately handle further packets.
 */
bool NetworkCoordinatorSocketHandler::HandlePacket(Packet *p)
{
	PacketCoordinatorType type = (PacketCoordinatorType)p->Recv_uint8();

	switch (type) {
		case PACKET_COORDINATOR_GC_ERROR:              return this->Receive_GC_ERROR(p);
		case PACKET_COORDINATOR_SERVER_REGISTER:       return this->Receive_SERVER_REGISTER(p);
		case PACKET_COORDINATOR_GC_REGISTER_ACK:       return this->Receive_GC_REGISTER_ACK(p);
		case PACKET_COORDINATOR_SERVER_UPDATE:         return this->Receive_SERVER_UPDATE(p);
		case PACKET_COORDINATOR_CLIENT_LISTING:        return this->Receive_CLIENT_LISTING(p);
		case PACKET_COORDINATOR_GC_LISTING:            return this->Receive_GC_LISTING(p);
		case PACKET_COORDINATOR_CLIENT_CONNECT:        return this->Receive_CLIENT_CONNECT(p);
		case PACKET_COORDINATOR_GC_CONNECTING:         return this->Receive_GC_CONNECTING(p);
		case PACKET_COORDINATOR_SERCLI_CONNECT_FAILED: return this->Receive_SERCLI_CONNECT_FAILED(p);
		case PACKET_COORDINATOR_GC_CONNECT_FAILED:     return this->Receive_GC_CONNECT_FAILED(p);
		case PACKET_COORDINATOR_CLIENT_CONNECTED:      return this->Receive_CLIENT_CONNECTED(p);
		case PACKET_COORDINATOR_GC_DIRECT_CONNECT:     return this->Receive_GC_DIRECT_CONNECT(p);
		case PACKET_COORDINATOR_GC_STUN_REQUEST:       return this->Receive_GC_STUN_REQUEST(p);
		case PACKET_COORDINATOR_SERCLI_STUN_RESULT:    return this->Receive_SERCLI_STUN_RESULT(p);
		case PACKET_COORDINATOR_GC_STUN_CONNECT:       return this->Receive_GC_STUN_CONNECT(p);
		case PACKET_COORDINATOR_GC_NEWGRF_LOOKUP:      return this->Receive_GC_NEWGRF_LOOKUP(p);
		case PACKET_COORDINATOR_GC_TURN_CONNECT:       return this->Receive_GC_TURN_CONNECT(p);

		default:
			Debug(net, 0, "[tcp/coordinator] Received invalid packet type {}", type);
			return false;
	}
}

/**
 * Receive a packet at TCP level.
 * @return Whether at least one packet was received.
 */
bool NetworkCoordinatorSocketHandler::ReceivePackets()
{
	/*
	 * We read only a few of the packets. This allows the GUI to update when
	 * a large set of servers is being received. Otherwise the interface
	 * "hangs" while the game is updating the server-list.
	 *
	 * What arbitrary number to choose is the ultimate question though.
	 */
	Packet *p;
	static const int MAX_PACKETS_TO_RECEIVE = 42;
	int i = MAX_PACKETS_TO_RECEIVE;
	while (--i != 0 && (p = this->ReceivePacket()) != nullptr) {
		bool cont = this->HandlePacket(p);
		delete p;
		if (!cont) return true;
	}

	return i != MAX_PACKETS_TO_RECEIVE - 1;
}

/**
 * Helper for logging receiving invalid packets.
 * @param type The received packet type.
 * @return Always false, as it's an error.
 */
bool NetworkCoordinatorSocketHandler::ReceiveInvalidPacket(PacketCoordinatorType type)
{
	Debug(net, 0, "[tcp/coordinator] Received illegal packet type {}", type);
	return false;
}

bool NetworkCoordinatorSocketHandler::Receive_GC_ERROR(Packet *) { return this->ReceiveInvalidPacket(PACKET_COORDINATOR_GC_ERROR); }
bool NetworkCoordinatorSocketHandler::Receive_SERVER_REGISTER(Packet *) { return this->ReceiveInvalidPacket(PACKET_COORDINATOR_SERVER_REGISTER); }
bool NetworkCoordinatorSocketHandler::Receive_GC_REGISTER_ACK(Packet *) { return this->ReceiveInvalidPacket(PACKET_COORDINATOR_GC_REGISTER_ACK); }
bool NetworkCoordinatorSocketHandler::Receive_SERVER_UPDATE(Packet *) { return this->ReceiveInvalidPacket(PACKET_COORDINATOR_SERVER_UPDATE); }
bool NetworkCoordinatorSocketHandler::Receive_CLIENT_LISTING(Packet *) { return this->ReceiveInvalidPacket(PACKET_COORDINATOR_CLIENT_LISTING); }
bool NetworkCoordinatorSocketHandler::Receive_GC_LISTING(Packet *) { return this->ReceiveInvalidPacket(PACKET_COORDINATOR_GC_LISTING); }
bool NetworkCoordinatorSocketHandler::Receive_CLIENT_CONNECT(Packet *) { return this->ReceiveInvalidPacket(PACKET_COORDINATOR_CLIENT_CONNECT); }
bool NetworkCoordinatorSocketHandler::Receive_GC_CONNECTING(Packet *) { return this->ReceiveInvalidPacket(PACKET_COORDINATOR_GC_CONNECTING); }
bool NetworkCoordinatorSocketHandler::Receive_SERCLI_CONNECT_FAILED(Packet *) { return this->ReceiveInvalidPacket(PACKET_COORDINATOR_SERCLI_CONNECT_FAILED); }
bool NetworkCoordinatorSocketHandler::Receive_GC_CONNECT_FAILED(Packet *) { return this->ReceiveInvalidPacket(PACKET_COORDINATOR_GC_CONNECT_FAILED); }
bool NetworkCoordinatorSocketHandler::Receive_CLIENT_CONNECTED(Packet *) { return this->ReceiveInvalidPacket(PACKET_COORDINATOR_CLIENT_CONNECTED); }
bool NetworkCoordinatorSocketHandler::Receive_GC_DIRECT_CONNECT(Packet *) { return this->ReceiveInvalidPacket(PACKET_COORDINATOR_GC_DIRECT_CONNECT); }
bool NetworkCoordinatorSocketHandler::Receive_GC_STUN_REQUEST(Packet *) { return this->ReceiveInvalidPacket(PACKET_COORDINATOR_GC_STUN_REQUEST); }
bool NetworkCoordinatorSocketHandler::Receive_SERCLI_STUN_RESULT(Packet *) { return this->ReceiveInvalidPacket(PACKET_COORDINATOR_SERCLI_STUN_RESULT); }
bool NetworkCoordinatorSocketHandler::Receive_GC_STUN_CONNECT(Packet *) { return this->ReceiveInvalidPacket(PACKET_COORDINATOR_GC_STUN_CONNECT); }
bool NetworkCoordinatorSocketHandler::Receive_GC_NEWGRF_LOOKUP(Packet *) { return this->ReceiveInvalidPacket(PACKET_COORDINATOR_GC_NEWGRF_LOOKUP); }
bool NetworkCoordinatorSocketHandler::Receive_GC_TURN_CONNECT(Packet *) { return this->ReceiveInvalidPacket(PACKET_COORDINATOR_GC_TURN_CONNECT); }
