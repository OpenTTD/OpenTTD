/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file network_coordinator.h Part of the network protocol handling Game Coordinator requests. */

#ifndef NETWORK_COORDINATOR_H
#define NETWORK_COORDINATOR_H

#include "core/tcp_coordinator.h"
#include "network_stun.h"
#include "network_turn.h"

/**
 * Game Coordinator communication.
 * For more detail about what the Game Coordinator does, please see
 * docs/game_coordinator.md.
 *
 * For servers:
 *  - Server sends SERVER_REGISTER.
 *  - Game Coordinator probes server to check if it can directly connect.
 *  - Game Coordinator sends GC_REGISTER_ACK with type of connection.
 *  - Server sends every 30 seconds SERVER_UPDATE.
 *
 * For clients (listing):
 *  - Client sends CLIENT_LISTING.
 *  - Game Coordinator returns the full list of public servers via GC_LISTING (multiple packets).
 *
 * For clients (connecting):
 *  - Client sends CLIENT_CONNECT.
 *  - Game Coordinator checks what type of connections the servers supports:
 *    1) Direct connect?
 *        - Send the client a GC_CONNECT with the peer address.
 *        - a) Client connects, client sends CLIENT_CONNECTED to Game Coordinator.
 *        - b) Client connect fails, client sends CLIENT_CONNECT_FAILED to Game Coordinator.
 *    2) STUN?
 *        - Game Coordinator sends GC_STUN_REQUEST to server/client (asking for both IPv4 and IPv6 STUN requests).
 *        - Game Coordinator collects what combination works and sends GC_STUN_CONNECT to server/client.
 *        - a) Server/client connect, client sends CLIENT_CONNECTED to Game Coordinator.
 *        - b) Server/client connect fails, both send SERCLI_CONNECT_FAILED to Game Coordinator.
 *        - Game Coordinator tries other combination if available.
 *    3) TURN?
 *        - Game Coordinator sends GC_TURN_CONNECT to server/client.
 *        - a) Server/client connect, client sends CLIENT_CONNECTED to Game Coordinator.
 *        - b) Server/client connect fails, both send SERCLI_CONNECT_FAILED to Game Coordinator.
 *  - If all fails, Game Coordinator sends GC_CONNECT_FAILED to indicate no connection is possible.
 */

/** Class for handling the client side of the Game Coordinator connection. */
class ClientNetworkCoordinatorSocketHandler : public NetworkCoordinatorSocketHandler {
private:
	std::chrono::steady_clock::time_point next_update; ///< When to send the next update (if server and public).
	std::map<std::string, std::pair<std::string, TCPServerConnecter *>> connecter; ///< Based on tokens, the current (invite-code, connecter) that are pending.
	std::map<std::string, TCPServerConnecter *> connecter_pre; ///< Based on invite codes, the current connecters that are pending.
	std::map<std::string, std::map<int, std::unique_ptr<ClientNetworkStunSocketHandler>>> stun_handlers; ///< All pending STUN handlers, stored by token:family.
	std::map<std::string, std::unique_ptr<ClientNetworkTurnSocketHandler>> turn_handlers; ///< Pending TURN handler (if any), stored by token.
	TCPConnecter *game_connecter = nullptr; ///< Pending connecter to the game server.

	uint32_t newgrf_lookup_table_cursor = 0; ///< Last received cursor for the #GameInfoNewGRFLookupTable updates.
	GameInfoNewGRFLookupTable newgrf_lookup_table; ///< Table to look up NewGRFs in the GC_LISTING packets.

protected:
	bool Receive_GC_ERROR(Packet *p) override;
	bool Receive_GC_REGISTER_ACK(Packet *p) override;
	bool Receive_GC_LISTING(Packet *p) override;
	bool Receive_GC_CONNECTING(Packet *p) override;
	bool Receive_GC_CONNECT_FAILED(Packet *p) override;
	bool Receive_GC_DIRECT_CONNECT(Packet *p) override;
	bool Receive_GC_STUN_REQUEST(Packet *p) override;
	bool Receive_GC_STUN_CONNECT(Packet *p) override;
	bool Receive_GC_NEWGRF_LOOKUP(Packet *p) override;
	bool Receive_GC_TURN_CONNECT(Packet *p) override;

public:
	/** The idle timeout; when to close the connection because it's idle. */
	static constexpr std::chrono::seconds IDLE_TIMEOUT = std::chrono::seconds(60);

	std::chrono::steady_clock::time_point last_activity;  ///< The last time there was network activity.
	bool connecting; ///< Are we connecting to the Game Coordinator?

	ClientNetworkCoordinatorSocketHandler() : connecting(false) {}

	NetworkRecvStatus CloseConnection(bool error = true) override;
	void SendReceive();

	void ConnectFailure(const std::string &token, uint8_t tracking_number);
	void ConnectSuccess(const std::string &token, SOCKET sock, NetworkAddress &address);
	void StunResult(const std::string &token, uint8_t family, bool result);

	void Connect();
	void CloseToken(const std::string &token);
	void CloseAllConnections();
	void CloseStunHandler(const std::string &token, uint8_t family = AF_UNSPEC);
	void CloseTurnHandler(const std::string &token);

	void Register();
	void SendServerUpdate();
	void GetListing();

	void ConnectToServer(const std::string &invite_code, TCPServerConnecter *connecter);
	void StartTurnConnection(std::string &token);
};

extern ClientNetworkCoordinatorSocketHandler _network_coordinator_client;

#endif /* NETWORK_COORDINATOR_H */
