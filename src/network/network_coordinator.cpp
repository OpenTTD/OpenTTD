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
#include "network_gui.h"
#include "network_internal.h"
#include "network_server.h"
#include "network_stun.h"
#include "table/strings.h"

#include "../safeguards.h"

static const auto NETWORK_COORDINATOR_DELAY_BETWEEN_UPDATES = std::chrono::seconds(30); ///< How many time between updates the server sends to the Game Coordinator.
ClientNetworkCoordinatorSocketHandler _network_coordinator_client; ///< The connection to the Game Coordinator.
ConnectionType _network_server_connection_type = CONNECTION_TYPE_UNKNOWN; ///< What type of connection the Game Coordinator detected we are on.
std::string _network_server_invite_code = ""; ///< Our invite code as indicated by the Game Coordinator.

/** Connect to a game server by IP:port. */
class NetworkDirectConnecter : public TCPConnecter {
private:
	std::string token;     ///< Token of this connection.
	uint8_t tracking_number; ///< Tracking number of this connection.

public:
	/**
	 * Try to establish a direct (hostname:port based) connection.
	 * @param hostname The hostname of the server.
	 * @param port The port of the server.
	 * @param token The token as given by the Game Coordinator to track this connection attempt.
	 * @param tracking_number The tracking number as given by the Game Coordinator to track this connection attempt.
	 */
	NetworkDirectConnecter(const std::string &hostname, uint16_t port, const std::string &token, uint8_t tracking_number) : TCPConnecter(hostname, port), token(token), tracking_number(tracking_number) {}

	void OnFailure() override
	{
		_network_coordinator_client.ConnectFailure(this->token, this->tracking_number);
	}

	void OnConnect(SOCKET s) override
	{
		NetworkAddress address = NetworkAddress::GetPeerAddress(s);
		_network_coordinator_client.ConnectSuccess(this->token, s, address);
	}
};

/** Connecter used after STUN exchange to connect from both sides to each other. */
class NetworkReuseStunConnecter : public TCPConnecter {
private:
	std::string token;     ///< Token of this connection.
	uint8_t tracking_number; ///< Tracking number of this connection.
	uint8_t family;          ///< Family of this connection.

public:
	/**
	 * Try to establish a STUN-based connection.
	 * @param hostname The hostname of the peer.
	 * @param port The port of the peer.
	 * @param bind_address The local bind address used for this connection.
	 * @param token The connection token.
	 * @param tracking_number The tracking number of the connection.
	 * @param family The family this connection is using.
	 */
	NetworkReuseStunConnecter(const std::string &hostname, uint16_t port, const NetworkAddress &bind_address, std::string token, uint8_t tracking_number, uint8_t family) :
		TCPConnecter(hostname, port, bind_address),
		token(token),
		tracking_number(tracking_number),
		family(family)
	{
	}

	void OnFailure() override
	{
		/* Close the STUN connection too, as it is no longer of use. */
		_network_coordinator_client.CloseStunHandler(this->token, this->family);

		_network_coordinator_client.ConnectFailure(this->token, this->tracking_number);
	}

	void OnConnect(SOCKET s) override
	{
		NetworkAddress address = NetworkAddress::GetPeerAddress(s);
		_network_coordinator_client.ConnectSuccess(this->token, s, address);
	}
};

/** Connect to the Game Coordinator server. */
class NetworkCoordinatorConnecter : TCPConnecter {
public:
	/**
	 * Initiate the connecting.
	 * @param connection_string The address of the Game Coordinator server.
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
			ShowErrorMessage(STR_NETWORK_ERROR_COORDINATOR_REGISTRATION_FAILED, INVALID_STRING_ID, WL_ERROR);

			/* To prevent that we constantly try to reconnect, switch to local game. */
			_settings_client.network.server_game_type = SERVER_GAME_TYPE_LOCAL;

			this->CloseConnection();
			return false;

		case NETWORK_COORDINATOR_ERROR_INVALID_INVITE_CODE: {
			auto connecter_pre_it = this->connecter_pre.find(detail);
			if (connecter_pre_it != this->connecter_pre.end()) {
				connecter_pre_it->second->SetFailure();
				this->connecter_pre.erase(connecter_pre_it);
			}

			/* Mark the server as offline. */
			NetworkGameList *item = NetworkGameListAddItem(detail);
			item->status = NGLS_OFFLINE;

			UpdateNetworkGameWindow();
			return true;
		}

		case NETWORK_COORDINATOR_ERROR_REUSE_OF_INVITE_CODE:
			ShowErrorMessage(STR_NETWORK_ERROR_COORDINATOR_REUSE_OF_INVITE_CODE, INVALID_STRING_ID, WL_ERROR);

			/* To prevent that we constantly battle for the same invite-code, switch to local game. */
			_settings_client.network.server_game_type = SERVER_GAME_TYPE_LOCAL;

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

	_settings_client.network.server_invite_code = p->Recv_string(NETWORK_INVITE_CODE_LENGTH);
	_settings_client.network.server_invite_code_secret = p->Recv_string(NETWORK_INVITE_CODE_SECRET_LENGTH);
	_network_server_connection_type = (ConnectionType)p->Recv_uint8();

	if (_network_server_connection_type == CONNECTION_TYPE_ISOLATED) {
		ShowErrorMessage(STR_NETWORK_ERROR_COORDINATOR_ISOLATED, STR_NETWORK_ERROR_COORDINATOR_ISOLATED_DETAIL, WL_ERROR);
	}

	/* Users can change the invite code in the settings, but this has no effect
	 * on the invite code as assigned by the server. So
	 * _network_server_invite_code contains the current invite code,
	 * and _settings_client.network.server_invite_code contains the one we will
	 * attempt to re-use when registering again. */
	_network_server_invite_code = _settings_client.network.server_invite_code;

	SetWindowDirty(WC_CLIENT_LIST, 0);

	if (_network_dedicated) {
		std::string connection_type;
		switch (_network_server_connection_type) {
			case CONNECTION_TYPE_ISOLATED: connection_type = "Remote players can't connect"; break;
			case CONNECTION_TYPE_DIRECT:   connection_type = "Public"; break;
			case CONNECTION_TYPE_STUN:     connection_type = "Behind NAT"; break;
			case CONNECTION_TYPE_TURN:     connection_type = "Via relay"; break;

			case CONNECTION_TYPE_UNKNOWN: // Never returned from Game Coordinator.
			default: connection_type = "Unknown"; break; // Should never happen, but don't fail if it does.
		}

		std::string game_type;
		switch (_settings_client.network.server_game_type) {
			case SERVER_GAME_TYPE_INVITE_ONLY: game_type = "Invite only"; break;
			case SERVER_GAME_TYPE_PUBLIC: game_type = "Public"; break;

			case SERVER_GAME_TYPE_LOCAL: // Impossible to register local servers.
			default: game_type = "Unknown"; break; // Should never happen, but don't fail if it does.
		}

		Debug(net, 3, "----------------------------------------");
		Debug(net, 3, "Your server is now registered with the Game Coordinator:");
		Debug(net, 3, "  Game type:       {}", game_type);
		Debug(net, 3, "  Connection type: {}", connection_type);
		Debug(net, 3, "  Invite code:     {}", _network_server_invite_code);
		Debug(net, 3, "----------------------------------------");
	} else {
		Debug(net, 3, "Game Coordinator registered our server with invite code '{}'", _network_server_invite_code);
	}

	return true;
}

bool ClientNetworkCoordinatorSocketHandler::Receive_GC_LISTING(Packet *p)
{
	uint8_t servers = p->Recv_uint16();

	/* End of list; we can now remove all expired items from the list. */
	if (servers == 0) {
		NetworkGameListRemoveExpired();
		return true;
	}

	for (; servers > 0; servers--) {
		std::string connection_string = p->Recv_string(NETWORK_HOSTNAME_PORT_LENGTH);

		/* Read the NetworkGameInfo from the packet. */
		NetworkGameInfo ngi = {};
		DeserializeNetworkGameInfo(p, &ngi, &this->newgrf_lookup_table);

		/* Now we know the connection string, we can add it to our list. */
		NetworkGameList *item = NetworkGameListAddItem(connection_string);

		/* Clear any existing GRFConfig chain. */
		ClearGRFConfigList(&item->info.grfconfig);
		/* Copy the new NetworkGameInfo info. */
		item->info = ngi;
		/* Check for compatability with the client. */
		CheckGameCompatibility(item->info);
		/* Mark server as online. */
		item->status = NGLS_ONLINE;
		/* Mark the item as up-to-date. */
		item->version = _network_game_list_version;
	}

	UpdateNetworkGameWindow();
	return true;
}

bool ClientNetworkCoordinatorSocketHandler::Receive_GC_CONNECTING(Packet *p)
{
	std::string token = p->Recv_string(NETWORK_TOKEN_LENGTH);
	std::string invite_code = p->Recv_string(NETWORK_INVITE_CODE_LENGTH);

	/* Find the connecter based on the invite code. */
	auto connecter_pre_it = this->connecter_pre.find(invite_code);
	if (connecter_pre_it == this->connecter_pre.end()) {
		this->CloseConnection();
		return false;
	}

	/* Now store it based on the token. */
	this->connecter[token] = {invite_code, connecter_pre_it->second};
	this->connecter_pre.erase(connecter_pre_it);

	return true;
}

bool ClientNetworkCoordinatorSocketHandler::Receive_GC_CONNECT_FAILED(Packet *p)
{
	std::string token = p->Recv_string(NETWORK_TOKEN_LENGTH);
	this->CloseToken(token);

	return true;
}

bool ClientNetworkCoordinatorSocketHandler::Receive_GC_DIRECT_CONNECT(Packet *p)
{
	std::string token = p->Recv_string(NETWORK_TOKEN_LENGTH);
	uint8_t tracking_number = p->Recv_uint8();
	std::string hostname = p->Recv_string(NETWORK_HOSTNAME_LENGTH);
	uint16_t port = p->Recv_uint16();

	/* Ensure all other pending connection attempts are killed. */
	if (this->game_connecter != nullptr) {
		this->game_connecter->Kill();
		this->game_connecter = nullptr;
	}

	this->game_connecter = new NetworkDirectConnecter(hostname, port, token, tracking_number);
	return true;
}

bool ClientNetworkCoordinatorSocketHandler::Receive_GC_STUN_REQUEST(Packet *p)
{
	std::string token = p->Recv_string(NETWORK_TOKEN_LENGTH);

	this->stun_handlers[token][AF_INET6] = ClientNetworkStunSocketHandler::Stun(token, AF_INET6);
	this->stun_handlers[token][AF_INET] = ClientNetworkStunSocketHandler::Stun(token, AF_INET);
	return true;
}

bool ClientNetworkCoordinatorSocketHandler::Receive_GC_STUN_CONNECT(Packet *p)
{
	std::string token = p->Recv_string(NETWORK_TOKEN_LENGTH);
	uint8_t tracking_number = p->Recv_uint8();
	uint8_t family = p->Recv_uint8();
	std::string host = p->Recv_string(NETWORK_HOSTNAME_PORT_LENGTH);
	uint16_t port = p->Recv_uint16();

	/* Check if we know this token. */
	auto stun_it = this->stun_handlers.find(token);
	if (stun_it == this->stun_handlers.end()) return true;
	auto family_it = stun_it->second.find(family);
	if (family_it == stun_it->second.end()) return true;

	/* Ensure all other pending connection attempts are killed. */
	if (this->game_connecter != nullptr) {
		this->game_connecter->Kill();
		this->game_connecter = nullptr;
	}

	/* We now mark the connection as closed, but we do not really close the
	 * socket yet. We do this when the NetworkReuseStunConnecter is connected.
	 * This prevents any NAT to already remove the route while we create the
	 * second connection on top of the first. */
	family_it->second->CloseConnection(false);

	/* Connect to our peer from the same local address as we use for the
	 * STUN server. This means that if there is any NAT in the local network,
	 * the public ip:port is still pointing to the local address, and as such
	 * a connection can be established. */
	this->game_connecter = new NetworkReuseStunConnecter(host, port, family_it->second->local_addr, token, tracking_number, family);
	return true;
}

bool ClientNetworkCoordinatorSocketHandler::Receive_GC_NEWGRF_LOOKUP(Packet *p)
{
	this->newgrf_lookup_table_cursor = p->Recv_uint32();

	uint16_t newgrfs = p->Recv_uint16();
	for (; newgrfs> 0; newgrfs--) {
		uint32_t index = p->Recv_uint32();
		DeserializeGRFIdentifierWithName(p, &this->newgrf_lookup_table[index]);
	}
	return true;
}

bool ClientNetworkCoordinatorSocketHandler::Receive_GC_TURN_CONNECT(Packet *p)
{
	std::string token = p->Recv_string(NETWORK_TOKEN_LENGTH);
	uint8_t tracking_number = p->Recv_uint8();
	std::string ticket = p->Recv_string(NETWORK_TOKEN_LENGTH);
	std::string connection_string = p->Recv_string(NETWORK_HOSTNAME_PORT_LENGTH);

	/* Ensure all other pending connection attempts are killed. */
	if (this->game_connecter != nullptr) {
		this->game_connecter->Kill();
		this->game_connecter = nullptr;
	}

	this->turn_handlers[token] = ClientNetworkTurnSocketHandler::Turn(token, tracking_number, ticket, connection_string);

	if (!_network_server) {
		auto connecter_it = this->connecter.find(token);
		if (connecter_it == this->connecter.end()) {
			/* Make sure we are still interested in connecting to this server. */
			this->ConnectFailure(token, 0);
			return true;
		}

		switch (_settings_client.network.use_relay_service) {
			case URS_NEVER:
				this->ConnectFailure(token, 0);
				break;

			case URS_ASK:
				ShowNetworkAskRelay(connecter_it->second.first, connection_string, token);
				break;

			case URS_ALLOW:
				this->StartTurnConnection(token);
				break;
		}
	} else {
		this->StartTurnConnection(token);
	}

	return true;
}

void ClientNetworkCoordinatorSocketHandler::StartTurnConnection(std::string &token)
{
	auto turn_it = this->turn_handlers.find(token);
	if (turn_it == this->turn_handlers.end()) return;

	turn_it->second->Connect();
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

	this->CloseAllConnections();

	SetWindowDirty(WC_CLIENT_LIST, 0);

	return NETWORK_RECV_STATUS_OKAY;
}

/**
 * Register our server to receive our invite code.
 */
void ClientNetworkCoordinatorSocketHandler::Register()
{
	_network_server_connection_type = CONNECTION_TYPE_UNKNOWN;
	this->next_update = {};

	SetWindowDirty(WC_CLIENT_LIST, 0);

	this->Connect();

	Packet *p = new Packet(PACKET_COORDINATOR_SERVER_REGISTER);
	p->Send_uint8(NETWORK_COORDINATOR_VERSION);
	p->Send_uint8(_settings_client.network.server_game_type);
	p->Send_uint16(_settings_client.network.server_port);
	if (_settings_client.network.server_invite_code.empty() || _settings_client.network.server_invite_code_secret.empty()) {
		p->Send_string("");
		p->Send_string("");
	} else {
		p->Send_string(_settings_client.network.server_invite_code);
		p->Send_string(_settings_client.network.server_invite_code_secret);
	}

	this->SendPacket(p);
}

/**
 * Send an update of our server status to the Game Coordinator.
 */
void ClientNetworkCoordinatorSocketHandler::SendServerUpdate()
{
	Debug(net, 6, "Sending server update to Game Coordinator");

	Packet *p = new Packet(PACKET_COORDINATOR_SERVER_UPDATE, TCP_MTU);
	p->Send_uint8(NETWORK_COORDINATOR_VERSION);
	SerializeNetworkGameInfo(p, GetCurrentNetworkServerGameInfo(), this->next_update.time_since_epoch() != std::chrono::nanoseconds::zero());

	this->SendPacket(p);

	this->next_update = std::chrono::steady_clock::now() + NETWORK_COORDINATOR_DELAY_BETWEEN_UPDATES;
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
	p->Send_uint32(this->newgrf_lookup_table_cursor);

	this->SendPacket(p);
}

/**
 * Join a server based on an invite code.
 * @param invite_code The invite code of the server to connect to.
 * @param connecter The connecter of the request.
 */
void ClientNetworkCoordinatorSocketHandler::ConnectToServer(const std::string &invite_code, TCPServerConnecter *connecter)
{
	assert(StrStartsWith(invite_code, "+"));

	if (this->connecter_pre.find(invite_code) != this->connecter_pre.end()) {
		/* If someone is hammering the refresh key, one can sent out two
		 * requests for the same invite code. There isn't really a great way
		 * of handling this, so just ignore this request. */
		connecter->SetFailure();
		return;
	}

	/* Initially we store based on invite code; on first reply we know the
	 * token, and will start using that key instead. */
	this->connecter_pre[invite_code] = connecter;

	this->Connect();

	Packet *p = new Packet(PACKET_COORDINATOR_CLIENT_CONNECT);
	p->Send_uint8(NETWORK_COORDINATOR_VERSION);
	p->Send_string(invite_code);

	this->SendPacket(p);
}

/**
 * Callback from a Connecter to let the Game Coordinator know the connection failed.
 * @param token Token of the connecter that failed.
 * @param tracking_number Tracking number of the connecter that failed.
 */
void ClientNetworkCoordinatorSocketHandler::ConnectFailure(const std::string &token, uint8_t tracking_number)
{
	/* Connecter will destroy itself. */
	this->game_connecter = nullptr;

	Packet *p = new Packet(PACKET_COORDINATOR_SERCLI_CONNECT_FAILED);
	p->Send_uint8(NETWORK_COORDINATOR_VERSION);
	p->Send_string(token);
	p->Send_uint8(tracking_number);

	this->SendPacket(p);

	/* We do not close the associated connecter here yet, as the
	 * Game Coordinator might have other methods of connecting available. */
}

/**
 * Callback from a Connecter to let the Game Coordinator know the connection
 * to the game server is established.
 * @param token Token of the connecter that succeeded.
 * @param sock The socket that the connecter can now use.
 */
void ClientNetworkCoordinatorSocketHandler::ConnectSuccess(const std::string &token, SOCKET sock, NetworkAddress &address)
{
	assert(sock != INVALID_SOCKET);

	/* Connecter will destroy itself. */
	this->game_connecter = nullptr;

	if (_network_server) {
		if (!ServerNetworkGameSocketHandler::ValidateClient(sock, address)) return;
		Debug(net, 3, "[{}] Client connected from {} on frame {}", ServerNetworkGameSocketHandler::GetName(), address.GetHostname(), _frame_counter);
		ServerNetworkGameSocketHandler::AcceptConnection(sock, address);
	} else {
		/* The client informs the Game Coordinator about the success. The server
		 * doesn't have to, as it is implied by the client telling. */
		Packet *p = new Packet(PACKET_COORDINATOR_CLIENT_CONNECTED);
		p->Send_uint8(NETWORK_COORDINATOR_VERSION);
		p->Send_string(token);
		this->SendPacket(p);

		/* Find the connecter; it can happen it no longer exist, in cases where
		 * we aborted the connect but the Game Coordinator was already in the
		 * processes of connecting us. */
		auto connecter_it = this->connecter.find(token);
		if (connecter_it != this->connecter.end()) {
			connecter_it->second.second->SetConnected(sock);
			this->connecter.erase(connecter_it);
		}
	}

	/* Close all remaining connections. */
	this->CloseToken(token);
}

/**
 * Callback from the STUN connecter to inform the Game Coordinator about the
 * result of the STUN.
 *
 * This helps the Game Coordinator not to wait for a timeout on its end, but
 * rather react as soon as the client/server knows the result.
 */
void ClientNetworkCoordinatorSocketHandler::StunResult(const std::string &token, uint8_t family, bool result)
{
	Packet *p = new Packet(PACKET_COORDINATOR_SERCLI_STUN_RESULT);
	p->Send_uint8(NETWORK_COORDINATOR_VERSION);
	p->Send_string(token);
	p->Send_uint8(family);
	p->Send_bool(result);
	this->SendPacket(p);
}

/**
 * Close the STUN handler.
 * @param token The token used for the STUN handlers.
 * @param family The family of STUN handlers to close. AF_UNSPEC to close all STUN handlers for this token.
 */
void ClientNetworkCoordinatorSocketHandler::CloseStunHandler(const std::string &token, uint8_t family)
{
	auto stun_it = this->stun_handlers.find(token);
	if (stun_it == this->stun_handlers.end()) return;

	if (family == AF_UNSPEC) {
		for (auto &[family, stun_handler] : stun_it->second) {
			stun_handler->CloseConnection();
			stun_handler->CloseSocket();
		}

		this->stun_handlers.erase(stun_it);
	} else {
		auto family_it = stun_it->second.find(family);
		if (family_it == stun_it->second.end()) return;

		family_it->second->CloseConnection();
		family_it->second->CloseSocket();

		stun_it->second.erase(family_it);
	}
}

/**
 * Close the TURN handler.
 * @param token The token used for the TURN handler.
 */
void ClientNetworkCoordinatorSocketHandler::CloseTurnHandler(const std::string &token)
{
	CloseWindowByClass(WC_NETWORK_ASK_RELAY, NRWCD_HANDLED);

	auto turn_it = this->turn_handlers.find(token);
	if (turn_it == this->turn_handlers.end()) return;

	turn_it->second->CloseConnection();
	turn_it->second->CloseSocket();

	/* We don't remove turn_handler here, as we can be called from within that
	 * turn_handler instance, so our object cannot be free'd yet. Instead, we
	 * check later if the connection is closed, and free the object then. */
}

/**
 * Close everything related to this connection token.
 * @param token The connection token to close.
 */
void ClientNetworkCoordinatorSocketHandler::CloseToken(const std::string &token)
{
	/* Close all remaining STUN / TURN connections. */
	this->CloseStunHandler(token);
	this->CloseTurnHandler(token);

	/* Close the caller of the connection attempt. */
	auto connecter_it = this->connecter.find(token);
	if (connecter_it != this->connecter.end()) {
		connecter_it->second.second->SetFailure();
		this->connecter.erase(connecter_it);
	}
}

/**
 * Close all pending connection tokens.
 */
void ClientNetworkCoordinatorSocketHandler::CloseAllConnections()
{
	/* Ensure all other pending connection attempts are also killed. */
	if (this->game_connecter != nullptr) {
		this->game_connecter->Kill();
		this->game_connecter = nullptr;
	}

	/* Mark any pending connecters as failed. */
	for (auto &[token, it] : this->connecter) {
		this->CloseStunHandler(token);
		this->CloseTurnHandler(token);
		it.second->SetFailure();

		/* Inform the Game Coordinator he can stop trying to connect us to the server. */
		this->ConnectFailure(token, 0);
	}
	this->stun_handlers.clear();
	this->turn_handlers.clear();
	this->connecter.clear();

	/* Also close any pending invite-code requests. */
	for (auto &[invite_code, it] : this->connecter_pre) {
		it->SetFailure();
	}
	this->connecter_pre.clear();
}

/**
 * Check whether we received/can send some data from/to the Game Coordinator server and
 * when that's the case handle it appropriately.
 */
void ClientNetworkCoordinatorSocketHandler::SendReceive()
{
	/* Private games are not listed via the Game Coordinator. */
	if (_network_server && _settings_client.network.server_game_type == SERVER_GAME_TYPE_LOCAL) {
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

	for (const auto &[token, families] : this->stun_handlers) {
		for (const auto &[family, stun_handler] : families) {
			stun_handler->SendReceive();
		}
	}

	/* Check for handlers that are not connecting nor connected. Destroy those objects. */
	for (auto turn_it = this->turn_handlers.begin(); turn_it != this->turn_handlers.end(); /* nothing */) {
		if (turn_it->second->connect_started && turn_it->second->connecter == nullptr && !turn_it->second->IsConnected()) {
			turn_it = this->turn_handlers.erase(turn_it);
		} else {
			turn_it++;
		}
	}

	for (const auto &[token, turn_handler] : this->turn_handlers) {
		turn_handler->SendReceive();
	}
}
