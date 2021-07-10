
/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file network_coordinator.cpp Game Coordinator sending/receiving part of the network protocol. */

#include "../stdafx.h"
#include "../debug.h"
#include "../error.h"
#include "../rev.h"
#include "../settings_type.h"
#include "../strings_func.h"
#include "../window_func.h"
#include "../window_type.h"
#include "network.h"
#include "network_coordinator.h"
#include "network_gamelist.h"
#include "network_internal.h"
#include "table/strings.h"

#include "../safeguards.h"

static const auto NETWORK_COORDINATOR_DELAY_BETWEEN_UPDATES = std::chrono::seconds(30); ///< How many time between updates the server sends to the Game Coordinator.
ClientNetworkCoordinatorSocketHandler _network_coordinator_client; ///< The connection to the Game Coordinator.
ConnectionType _network_server_connection_type = CONNECTION_TYPE_UNKNOWN; ///< What type of connection the Game Coordinator detected we are on.

/** Connect to the Game Coordinator server. */
class NetworkCoordinatorConnecter : TCPConnecter {
public:
	/**
	 * Initiate the connecting.
	 * @param address The address of the Game Coordinator server.
	 */
	NetworkCoordinatorConnecter(const std::string &connection_string) : TCPConnecter(connection_string, NETWORK_COORDINATOR_SERVER_PORT) {}

	void OnFailure() override
	{
		_network_coordinator_client.connecting = false;
		_network_coordinator_client.CloseConnection(true);
	}

	void OnConnect(SOCKET s) override
	{
		assert(_network_coordinator_client.sock == INVALID_SOCKET);

		_network_coordinator_client.sock = s;
		_network_coordinator_client.last_activity = std::chrono::steady_clock::now();
		_network_coordinator_client.connecting = false;
	}
};

bool ClientNetworkCoordinatorSocketHandler::Receive_GC_ERROR(Packet *p)
{
	NetworkCoordinatorErrorType error = (NetworkCoordinatorErrorType)p->Recv_uint8();
	std::string detail = p->Recv_string(NETWORK_ERROR_DETAIL_LENGTH);

	switch (error) {
		case NETWORK_COORDINATOR_ERROR_UNKNOWN:
			this->CloseConnection();
			return false;

		case NETWORK_COORDINATOR_ERROR_REGISTRATION_FAILED:
			SetDParamStr(0, detail);
			ShowErrorMessage(STR_NETWORK_ERROR_COORDINATOR_REGISTRATION_FAILED, STR_JUST_RAW_STRING, WL_ERROR);

			/* To prevent that we constantly try to reconnect, switch to private game. */
			_settings_client.network.server_advertise = false;

			this->CloseConnection();
			return false;

		default:
			Debug(net, 0, "Invalid error type {} received from Game Coordinator", error);
			this->CloseConnection();
			return false;
	}
}

bool ClientNetworkCoordinatorSocketHandler::Receive_GC_REGISTER_ACK(Packet *p)
{
	/* Schedule sending an update. */
	this->next_update = std::chrono::steady_clock::now();

	_network_server_connection_type = (ConnectionType)p->Recv_uint8();

	if (_network_server_connection_type == CONNECTION_TYPE_ISOLATED) {
		ShowErrorMessage(STR_NETWORK_ERROR_COORDINATOR_ISOLATED, STR_NETWORK_ERROR_COORDINATOR_ISOLATED_DETAIL, WL_ERROR);
	}

	SetWindowDirty(WC_CLIENT_LIST, 0);

	if (_network_dedicated) {
		std::string connection_type;
		switch (_network_server_connection_type) {
			case CONNECTION_TYPE_ISOLATED: connection_type = "Remote players can't connect"; break;
			case CONNECTION_TYPE_DIRECT:   connection_type = "Public"; break;

			case CONNECTION_TYPE_UNKNOWN: // Never returned from Game Coordinator.
			default: connection_type = "Unknown"; break; // Should never happen, but don't fail if it does.
		}

		Debug(net, 3, "----------------------------------------");
		Debug(net, 3, "Your server is now registered with the Game Coordinator:");
		Debug(net, 3, "  Game type:       Public");
		Debug(net, 3, "  Connection type: {}", connection_type);
		Debug(net, 3, "----------------------------------------");
	}

	return true;
}

bool ClientNetworkCoordinatorSocketHandler::Receive_GC_LISTING(Packet *p)
{
	uint8 servers = p->Recv_uint16();

	/* End of list; we can now remove all expired items from the list. */
	if (servers == 0) {
		NetworkGameListRemoveExpired();
		return true;
	}

	for (; servers > 0; servers--) {
		std::string connection_string = p->Recv_string(NETWORK_HOSTNAME_PORT_LENGTH);

		/* Read the NetworkGameInfo from the packet. */
		NetworkGameInfo ngi = {};
		DeserializeNetworkGameInfo(p, &ngi);

		/* Now we know the join-key, we can add it to our list. */
		NetworkGameList *item = NetworkGameListAddItem(connection_string);

		/* Clear any existing GRFConfig chain. */
		ClearGRFConfigList(&item->info.grfconfig);
		/* Copy the new NetworkGameInfo info. */
		item->info = ngi;
		/* Check for compatability with the client. */
		CheckGameCompatibility(item->info);
		/* Mark server as online. */
		item->online = true;
		/* Mark the item as up-to-date. */
		item->version = _network_game_list_version;
	}

	UpdateNetworkGameWindow();
	return true;
}

void ClientNetworkCoordinatorSocketHandler::Connect()
{
	/* We are either already connected or are trying to connect. */
	if (this->sock != INVALID_SOCKET || this->connecting) return;

	this->Reopen();

	this->connecting = true;
	this->last_activity = std::chrono::steady_clock::now();

	new NetworkCoordinatorConnecter(NetworkCoordinatorConnectionString());
}

NetworkRecvStatus ClientNetworkCoordinatorSocketHandler::CloseConnection(bool error)
{
	NetworkCoordinatorSocketHandler::CloseConnection(error);

	this->CloseSocket();
	this->connecting = false;

	_network_server_connection_type = CONNECTION_TYPE_UNKNOWN;
	this->next_update = {};

	SetWindowDirty(WC_CLIENT_LIST, 0);

	return NETWORK_RECV_STATUS_OKAY;
}

/**
 * Register our server to receive our join-key.
 */
void ClientNetworkCoordinatorSocketHandler::Register()
{
	_network_server_connection_type = CONNECTION_TYPE_UNKNOWN;
	this->next_update = {};

	SetWindowDirty(WC_CLIENT_LIST, 0);

	this->Connect();

	Packet *p = new Packet(PACKET_COORDINATOR_SERVER_REGISTER);
	p->Send_uint8(NETWORK_COORDINATOR_VERSION);
	p->Send_uint8(SERVER_GAME_TYPE_PUBLIC);
	p->Send_uint16(_settings_client.network.server_port);

	this->SendPacket(p);
}

/**
 * Send an update of our server status to the Game Coordinator.
 */
void ClientNetworkCoordinatorSocketHandler::SendServerUpdate()
{
	Debug(net, 6, "Sending server update to Game Coordinator");
	this->next_update = std::chrono::steady_clock::now() + NETWORK_COORDINATOR_DELAY_BETWEEN_UPDATES;

	Packet *p = new Packet(PACKET_COORDINATOR_SERVER_UPDATE);
	p->Send_uint8(NETWORK_COORDINATOR_VERSION);
	SerializeNetworkGameInfo(p, GetCurrentNetworkServerGameInfo());

	this->SendPacket(p);
}

/**
 * Request a listing of all public servers.
 */
void ClientNetworkCoordinatorSocketHandler::GetListing()
{
	this->Connect();

	_network_game_list_version++;

	Packet *p = new Packet(PACKET_COORDINATOR_CLIENT_LISTING);
	p->Send_uint8(NETWORK_COORDINATOR_VERSION);
	p->Send_uint8(NETWORK_GAME_INFO_VERSION);
	p->Send_string(_openttd_revision);

	this->SendPacket(p);
}

/**
 * Check whether we received/can send some data from/to the Game Coordinator server and
 * when that's the case handle it appropriately.
 */
void ClientNetworkCoordinatorSocketHandler::SendReceive()
{
	/* Private games are not listed via the Game Coordinator. */
	if (_network_server && !_settings_client.network.server_advertise) {
		if (this->sock != INVALID_SOCKET) {
			this->CloseConnection();
		}
		return;
	}

	static int last_attempt_backoff = 1;
	static bool first_reconnect = true;

	if (this->sock == INVALID_SOCKET) {
		static std::chrono::steady_clock::time_point last_attempt = {};

		/* Don't auto-reconnect when we are not a server. */
		if (!_network_server) return;
		/* Don't reconnect if we are connecting. */
		if (this->connecting) return;
		/* Throttle how often we try to reconnect. */
		if (std::chrono::steady_clock::now() < last_attempt + std::chrono::seconds(1) * last_attempt_backoff) return;

		last_attempt = std::chrono::steady_clock::now();
		/* Delay reconnecting with up to 32 seconds. */
		if (last_attempt_backoff < 32) {
			last_attempt_backoff *= 2;
		}

		/* Do not reconnect on the first attempt, but only initialize the
		 * last_attempt variables.  Otherwise after an outage all servers
		 * reconnect at the same time, potentially overwhelming the
		 * Game Coordinator. */
		if (first_reconnect) {
			first_reconnect = false;
			return;
		}

		Debug(net, 1, "Connection with Game Coordinator lost; reconnecting...");
		this->Register();
		return;
	}

	last_attempt_backoff = 1;
	first_reconnect = true;

	if (_network_server && _network_server_connection_type != CONNECTION_TYPE_UNKNOWN && std::chrono::steady_clock::now() > this->next_update) {
		this->SendServerUpdate();
	}

	if (!_network_server && std::chrono::steady_clock::now() > this->last_activity + IDLE_TIMEOUT) {
		this->CloseConnection();
		return;
	}

	if (this->CanSendReceive()) {
		if (this->ReceivePackets()) {
			this->last_activity = std::chrono::steady_clock::now();
		}
	}

	this->SendPackets();
}
