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
		_network_coordinator_client.connecting = false;
	}
};

bool ClientNetworkCoordinatorSocketHandler::Receive_SERVER_ERROR(Packet *p)
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

bool ClientNetworkCoordinatorSocketHandler::Receive_SERVER_REGISTER_ACK(Packet *p)
{
	/* Schedule sending an update. */
	this->next_update = std::chrono::steady_clock::now();

	_settings_client.network.server_join_key = "+" + p->Recv_string(NETWORK_JOIN_KEY_LENGTH);
	_settings_client.network.server_join_key_secret = p->Recv_string(NETWORK_JOIN_KEY_SECRET_LENGTH);
	_network_server_connection_type = (ConnectionType)p->Recv_uint8();

	if (_network_server_connection_type == CONNECTION_TYPE_ISOLATED) {
		ShowErrorMessage(STR_NETWORK_ERROR_COORDINATOR_ISOLATED, STR_NETWORK_ERROR_COORDINATOR_ISOLATED_DETAIL, WL_ERROR);
	}

	/* Users can change the join-key in the settings, but this has no effect
	 * on the join-key as assigned by the server. So _network_game_info.join_key
	 * contains the current join-key, and _settings_client.network.server_join_key
	 * contains the one we will attempt to re-use when registering again. */
	_network_game_info.join_key = _settings_client.network.server_join_key;

	SetWindowDirty(WC_CLIENT_LIST, 0);

	if (_network_dedicated) {
		std::string connection_type;
		switch (_network_server_connection_type) {
			case CONNECTION_TYPE_ISOLATED: connection_type = "Inaccessible (blocked by firewall/network configuration)"; break;
			case CONNECTION_TYPE_DIRECT:   connection_type = "Public"; break;

			case CONNECTION_TYPE_UNKNOWN: // Never returned from Game Coordinator.
			default: connection_type = "Unknown"; break; // Should never happen, but don't fail if it does.
		}

		Debug(net, 3, " ");
		Debug(net, 3, "Your server is now registered with the Game Coordinator:");
		Debug(net, 3, "----------------------------------------");
		Debug(net, 3, "  Game type:       Public");
		Debug(net, 3, "  Connection type: {}", connection_type);
		Debug(net, 3, "  Invite code:     {}", _network_game_info.join_key);
		Debug(net, 3, "----------------------------------------");
		Debug(net, 3, " ");
	} else {
		Debug(net, 3, "Game Coordinator registered our server with invite code '{}'", _network_game_info.join_key);
	}

	return true;
}

void ClientNetworkCoordinatorSocketHandler::Connect()
{
	/* We are either already connected or are trying to connect. */
	if (this->sock != INVALID_SOCKET || this->connecting) return;

	this->Reopen();

	this->connecting = true;
	new NetworkCoordinatorConnecter(NETWORK_COORDINATOR_SERVER_HOST);
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

	Packet *p = new Packet(PACKET_COORDINATOR_CLIENT_REGISTER);
	p->Send_uint8(NETWORK_COORDINATOR_VERSION);
	p->Send_uint8(SERVER_GAME_TYPE_PUBLIC);
	p->Send_uint16(_settings_client.network.server_port);
	if (_settings_client.network.server_join_key.empty() || _settings_client.network.server_join_key_secret.empty() || _settings_client.network.server_join_key[0] != '+') {
		p->Send_string("");
		p->Send_string("");
	} else {
		assert(_settings_client.network.server_join_key[0] == '+');
		p->Send_string(_settings_client.network.server_join_key.substr(1));
		p->Send_string(_settings_client.network.server_join_key_secret);
	}

	this->SendPacket(p);
}

/**
 * Send an update of our server status to the Game Coordinator.
 */
void ClientNetworkCoordinatorSocketHandler::SendServerUpdate()
{
	Debug(net, 6, "Sending server update to Game Coordinator");
	this->next_update = std::chrono::steady_clock::now() + NETWORK_COORDINATOR_DELAY_BETWEEN_UPDATES;

	Packet *p = new Packet(PACKET_COORDINATOR_CLIENT_UPDATE);
	p->Send_uint8(NETWORK_COORDINATOR_VERSION);
	SerializeNetworkGameInfo(p, GetCurrentNetworkServerGameInfo());

	this->SendPacket(p);
}

/**
 * Check whether we received/can send some data from/to the Game Coordinator server and
 * when that's the case handle it appropriately
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

	if (this->sock == INVALID_SOCKET) {
		if (!this->connecting && _network_server) {
			static std::chrono::steady_clock::time_point last_attempt = {};

			if (std::chrono::steady_clock::now() > last_attempt + std::chrono::seconds(1) * last_attempt_backoff) {
				last_attempt = std::chrono::steady_clock::now();
				/* Delay reconnecting with up to 32 seconds. */
				if (last_attempt_backoff < 32) {
					last_attempt_backoff *= 2;
				}

				Debug(net, 1, "Connection with Game Coordinator lost; reconnecting ...");
				this->Register();
			}
		}
		return;
	} else if (last_attempt_backoff != 1) {
		last_attempt_backoff = 1;
	}

	if (_network_server && _network_server_connection_type != CONNECTION_TYPE_UNKNOWN && std::chrono::steady_clock::now() > this->next_update) {
		this->SendServerUpdate();
	}

	if (this->CanSendReceive()) {
		this->ReceivePackets();
	}

	this->SendPackets();
}
