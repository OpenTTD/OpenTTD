/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file network_turn.cpp TURN sending/receiving part of the network protocol. */

#include "../stdafx.h"
#include "../debug.h"
#include "../error.h"
#include "../strings_func.h"
#include "network_coordinator.h"
#include "network_turn.h"

#include "table/strings.h"

#include "../safeguards.h"

/** Connect to the TURN server. */
class NetworkTurnConnecter : public TCPConnecter {
private:
	ClientNetworkTurnSocketHandler *handler;

public:
	/**
	 * Initiate the connecting.
	 * @param connection_string The address of the TURN server.
	 */
	NetworkTurnConnecter(ClientNetworkTurnSocketHandler *handler, const std::string &connection_string) : TCPConnecter(connection_string, NETWORK_TURN_SERVER_PORT), handler(handler) {}

	void OnFailure() override
	{
		this->handler->connecter = nullptr;

		this->handler->ConnectFailure();
	}

	void OnConnect(SOCKET s) override
	{
		this->handler->connecter = nullptr;

		this->handler->sock = s;
	}
};

bool ClientNetworkTurnSocketHandler::Receive_TURN_ERROR(Packet *)
{
	this->ConnectFailure();

	return false;
}

bool ClientNetworkTurnSocketHandler::Receive_TURN_CONNECTED(Packet *p)
{
	std::string hostname = p->Recv_string(NETWORK_HOSTNAME_LENGTH);

	/* Act like we no longer have a socket, as we are handing it over to the
	 * game handler. */
	SOCKET game_sock = this->sock;
	this->sock = INVALID_SOCKET;

	NetworkAddress address = NetworkAddress(hostname, NETWORK_DEFAULT_PORT);
	_network_coordinator_client.ConnectSuccess(this->token, game_sock, address);

	return false;
}

/**
 * Connect to the TURN server.
 */
void ClientNetworkTurnSocketHandler::Connect()
{
	this->connect_started = true;
	this->connecter = new NetworkTurnConnecter(this, this->connection_string);
}

/**
 * Prepare a TURN connection.
 * Not until you run Connect() on the resulting instance will it start setting
 * up the TURN connection.
 * @param token The token as received from the Game Coordinator.
 * @param tracking_number The tracking number as recieved from the Game Coordinator.
 * @param ticket The ticket as received from the Game Coordinator.
 * @param connection_string Connection string of the TURN server.
 * @return The handler for this TURN connection.
 */
/* static */ std::unique_ptr<ClientNetworkTurnSocketHandler> ClientNetworkTurnSocketHandler::Turn(const std::string &token, uint8_t tracking_number, const std::string &ticket, const std::string &connection_string)
{
	auto turn_handler = std::make_unique<ClientNetworkTurnSocketHandler>(token, tracking_number, connection_string);

	Packet *p = new Packet(PACKET_TURN_SERCLI_CONNECT);
	p->Send_uint8(NETWORK_COORDINATOR_VERSION);
	p->Send_string(ticket);

	turn_handler->SendPacket(p);

	return turn_handler;
}

void ClientNetworkTurnSocketHandler::ConnectFailure()
{
	_network_coordinator_client.ConnectFailure(this->token, this->tracking_number);
}

NetworkRecvStatus ClientNetworkTurnSocketHandler::CloseConnection(bool error)
{
	NetworkTurnSocketHandler::CloseConnection(error);

	/* Also make sure any pending connecter is killed ASAP. */
	if (this->connecter != nullptr) {
		this->connecter->Kill();
		this->connecter = nullptr;
	}

	return NETWORK_RECV_STATUS_OKAY;
}

ClientNetworkTurnSocketHandler::~ClientNetworkTurnSocketHandler()
{
	if (this->connecter != nullptr) {
		this->connecter->Kill();
		this->connecter = nullptr;
	}
}

/**
 * Check whether we received/can send some data from/to the TURN server and
 * when that's the case handle it appropriately
 */
void ClientNetworkTurnSocketHandler::SendReceive()
{
	if (this->sock == INVALID_SOCKET) return;

	if (this->CanSendReceive()) {
		this->ReceivePackets();
	}

	this->SendPackets();
}
