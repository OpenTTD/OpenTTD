/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tcp_coordinator.cpp Basic functions to receive and send Game Coordinator packets. */

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
bool NetworkCoordinatorSocketHandler::HandlePacket(Packet &p)
{
	PacketCoordinatorType type = static_cast<PacketCoordinatorType>(p.Recv_uint8());

	switch (type) {
		case PacketCoordinatorType::GameCoordinatorError: return this->ReceiveGameCoordinatorError(p);
		case PacketCoordinatorType::ServerRegister: return this->ReceiveServerRegister(p);
		case PacketCoordinatorType::GameCoordinatorRegisterAck: return this->ReceiveGameCoordinatorRegisterAck(p);
		case PacketCoordinatorType::ServerUpdate: return this->ReceiveServerUpdate(p);
		case PacketCoordinatorType::ClientListing: return this->ReceiveClientListing(p);
		case PacketCoordinatorType::GameCoordinatorListing: return this->ReceiveGameCoordinatorListing(p);
		case PacketCoordinatorType::ClientConnect: return this->ReceiveClientConnect(p);
		case PacketCoordinatorType::GameCoordinatorConnecting: return this->ReceiveGameCoordinatorConnecting(p);
		case PacketCoordinatorType::ServerOrClientConnectFailed: return this->ReceiveServerOrClientConnectFailed(p);
		case PacketCoordinatorType::GameCoordinatorConnectFailed: return this->ReceiveGameCoordinatorConnectFailed(p);
		case PacketCoordinatorType::ClientConnected: return this->ReceiveClientConnected(p);
		case PacketCoordinatorType::GameCoordinatorDirectConnect: return this->ReceiveGameCoordinatorDirectConnect(p);
		case PacketCoordinatorType::GameCoordinatorStunRequest: return this->ReceiveGameCoordinatorStunRequest(p);
		case PacketCoordinatorType::ServerOrClientStunResult: return this->ReceiveServerOrClientStunResult(p);
		case PacketCoordinatorType::GameCoordinatorStunConnect: return this->ReceiveGameCoordinatorStunConnect(p);
		case PacketCoordinatorType::GameCoordinatorNewGRFLookup: return this->ReceiveGameCoordinatorNewGRFLookup(p);
		case PacketCoordinatorType::GameCoordinatorTurnConnect: return this->ReceiveGameCoordinatorTurnConnect(p);

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
	std::unique_ptr<Packet> p;
	static const int MAX_PACKETS_TO_RECEIVE = 42;
	int i = MAX_PACKETS_TO_RECEIVE;
	while (--i != 0 && (p = this->ReceivePacket()) != nullptr) {
		bool cont = this->HandlePacket(*p);
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

bool NetworkCoordinatorSocketHandler::ReceiveGameCoordinatorError(Packet &) { return this->ReceiveInvalidPacket(PacketCoordinatorType::GameCoordinatorError); }
bool NetworkCoordinatorSocketHandler::ReceiveServerRegister(Packet &) { return this->ReceiveInvalidPacket(PacketCoordinatorType::ServerRegister); }
bool NetworkCoordinatorSocketHandler::ReceiveGameCoordinatorRegisterAck(Packet &) { return this->ReceiveInvalidPacket(PacketCoordinatorType::GameCoordinatorRegisterAck); }
bool NetworkCoordinatorSocketHandler::ReceiveServerUpdate(Packet &) { return this->ReceiveInvalidPacket(PacketCoordinatorType::ServerUpdate); }
bool NetworkCoordinatorSocketHandler::ReceiveClientListing(Packet &) { return this->ReceiveInvalidPacket(PacketCoordinatorType::ClientListing); }
bool NetworkCoordinatorSocketHandler::ReceiveGameCoordinatorListing(Packet &) { return this->ReceiveInvalidPacket(PacketCoordinatorType::GameCoordinatorListing); }
bool NetworkCoordinatorSocketHandler::ReceiveClientConnect(Packet &) { return this->ReceiveInvalidPacket(PacketCoordinatorType::ClientConnect); }
bool NetworkCoordinatorSocketHandler::ReceiveGameCoordinatorConnecting(Packet &) { return this->ReceiveInvalidPacket(PacketCoordinatorType::GameCoordinatorConnecting); }
bool NetworkCoordinatorSocketHandler::ReceiveServerOrClientConnectFailed(Packet &) { return this->ReceiveInvalidPacket(PacketCoordinatorType::ServerOrClientConnectFailed); }
bool NetworkCoordinatorSocketHandler::ReceiveGameCoordinatorConnectFailed(Packet &) { return this->ReceiveInvalidPacket(PacketCoordinatorType::GameCoordinatorConnectFailed); }
bool NetworkCoordinatorSocketHandler::ReceiveClientConnected(Packet &) { return this->ReceiveInvalidPacket(PacketCoordinatorType::ClientConnected); }
bool NetworkCoordinatorSocketHandler::ReceiveGameCoordinatorDirectConnect(Packet &) { return this->ReceiveInvalidPacket(PacketCoordinatorType::GameCoordinatorDirectConnect); }
bool NetworkCoordinatorSocketHandler::ReceiveGameCoordinatorStunRequest(Packet &) { return this->ReceiveInvalidPacket(PacketCoordinatorType::GameCoordinatorStunRequest); }
bool NetworkCoordinatorSocketHandler::ReceiveServerOrClientStunResult(Packet &) { return this->ReceiveInvalidPacket(PacketCoordinatorType::ServerOrClientStunResult); }
bool NetworkCoordinatorSocketHandler::ReceiveGameCoordinatorStunConnect(Packet &) { return this->ReceiveInvalidPacket(PacketCoordinatorType::GameCoordinatorStunConnect); }
bool NetworkCoordinatorSocketHandler::ReceiveGameCoordinatorNewGRFLookup(Packet &) { return this->ReceiveInvalidPacket(PacketCoordinatorType::GameCoordinatorNewGRFLookup); }
bool NetworkCoordinatorSocketHandler::ReceiveGameCoordinatorTurnConnect(Packet &) { return this->ReceiveInvalidPacket(PacketCoordinatorType::GameCoordinatorTurnConnect); }
