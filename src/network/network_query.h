/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file network_query.h Query part of the network protocol. */

#ifndef NETWORK_QUERY_H
#define NETWORK_QUERY_H

#include "network_internal.h"

/** Class for handling the client side of quering a game server. */
class QueryNetworkGameSocketHandler : public ZeroedMemoryAllocator, public NetworkGameSocketHandler {
private:
	static std::vector<std::unique_ptr<QueryNetworkGameSocketHandler>> queries; ///< Pending queries.
	std::string connection_string; ///< Address we are connected to.

protected:
	NetworkRecvStatus Receive_SERVER_FULL(Packet &p) override;
	NetworkRecvStatus Receive_SERVER_BANNED(Packet &p) override;
	NetworkRecvStatus Receive_SERVER_ERROR(Packet &p) override;
	NetworkRecvStatus Receive_SERVER_GAME_INFO(Packet &p) override;

	NetworkRecvStatus SendGameInfo();

	bool CheckConnection();
	void Send();
	bool Receive();

public:
	/**
	 * Create a new socket for the client side of quering game server.
	 * @param s The socket to connect with.
	 * @param connection_string The connection string of the server.
	 */
	QueryNetworkGameSocketHandler(SOCKET s, const std::string &connection_string) : NetworkGameSocketHandler(s), connection_string(connection_string) {}

	/**
	 * Start to query a server based on an open socket.
	 * @param s The socket to connect with.
	 * @param connection_string The connection string of the server.
	 */
	static void QueryServer(SOCKET s, const std::string &connection_string)
	{
		auto query = std::make_unique<QueryNetworkGameSocketHandler>(s, connection_string);
		query->SendGameInfo();

		QueryNetworkGameSocketHandler::queries.push_back(std::move(query));
	}

	static void SendReceive();

	NetworkRecvStatus CloseConnection(NetworkRecvStatus status) override;
};

#endif /* NETWORK_QUERY_H */
