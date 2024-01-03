/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file network_query.cpp Query part of the network protocol. */

#include "../stdafx.h"
#include "core/network_game_info.h"
#include "network_query.h"
#include "network_gamelist.h"
#include "../error.h"

#include "table/strings.h"

#include "../safeguards.h"

std::vector<std::unique_ptr<QueryNetworkGameSocketHandler>> QueryNetworkGameSocketHandler::queries = {};

NetworkRecvStatus QueryNetworkGameSocketHandler::CloseConnection(NetworkRecvStatus status)
{
	assert(status != NETWORK_RECV_STATUS_OKAY);
	assert(this->sock != INVALID_SOCKET);

	return status;
}

/**
 * Check the connection's state, i.e. is the connection still up?
 */
bool QueryNetworkGameSocketHandler::CheckConnection()
{
	std::chrono::steady_clock::duration lag = std::chrono::steady_clock::now() - this->last_packet;

	/* If there was no response in 5 seconds, terminate the query. */
	if (lag > std::chrono::seconds(5)) {
		this->CloseConnection(NETWORK_RECV_STATUS_CONNECTION_LOST);
		return false;
	}

	return true;
}

/**
 * Check whether we received/can send some data from/to the server and
 * when that's the case handle it appropriately.
 * @return true when everything went okay.
 */
bool QueryNetworkGameSocketHandler::Receive()
{
	if (this->CanSendReceive()) {
		NetworkRecvStatus res = this->ReceivePackets();
		if (res != NETWORK_RECV_STATUS_OKAY) {
			this->CloseConnection(res);
			return false;
		}
	}
	return true;
}

/** Send the packets of this socket handler. */
void QueryNetworkGameSocketHandler::Send()
{
	this->SendPackets();
}

/**
 * Query the server for server information.
 */
NetworkRecvStatus QueryNetworkGameSocketHandler::SendGameInfo()
{
	this->SendPacket(new Packet(PACKET_CLIENT_GAME_INFO));
	return NETWORK_RECV_STATUS_OKAY;
}

NetworkRecvStatus QueryNetworkGameSocketHandler::Receive_SERVER_FULL(Packet *)
{
	NetworkGameList *item = NetworkGameListAddItem(this->connection_string);
	item->status = NGLS_FULL;
	item->refreshing = false;

	UpdateNetworkGameWindow();

	return NETWORK_RECV_STATUS_CLOSE_QUERY;
}

NetworkRecvStatus QueryNetworkGameSocketHandler::Receive_SERVER_BANNED(Packet *)
{
	NetworkGameList *item = NetworkGameListAddItem(this->connection_string);
	item->status = NGLS_BANNED;
	item->refreshing = false;

	UpdateNetworkGameWindow();

	return NETWORK_RECV_STATUS_CLOSE_QUERY;
}

NetworkRecvStatus QueryNetworkGameSocketHandler::Receive_SERVER_GAME_INFO(Packet *p)
{
	NetworkGameList *item = NetworkGameListAddItem(this->connection_string);

	/* Clear any existing GRFConfig chain. */
	ClearGRFConfigList(&item->info.grfconfig);
	/* Retrieve the NetworkGameInfo from the packet. */
	DeserializeNetworkGameInfo(p, &item->info);
	/* Check for compatability with the client. */
	CheckGameCompatibility(item->info);
	/* Ensure we consider the server online. */
	item->status = NGLS_ONLINE;
	item->refreshing = false;

	UpdateNetworkGameWindow();

	return NETWORK_RECV_STATUS_CLOSE_QUERY;
}

NetworkRecvStatus QueryNetworkGameSocketHandler::Receive_SERVER_ERROR(Packet *p)
{
	NetworkErrorCode error = (NetworkErrorCode)p->Recv_uint8();

	NetworkGameList *item = NetworkGameListAddItem(this->connection_string);

	if (error == NETWORK_ERROR_NOT_EXPECTED) {
		/* If we query a server that is 1.11.1 or older, we get an
		 * NETWORK_ERROR_NOT_EXPECTED on requesting the game info. Show to the
		 * user this server is too old to query.
		 */
		item->status = NGLS_TOO_OLD;
	} else {
		item->status = NGLS_OFFLINE;
	}
	item->refreshing = false;

	UpdateNetworkGameWindow();

	return NETWORK_RECV_STATUS_CLOSE_QUERY;
}

/**
 * Check if any query needs to send or receive.
 */
/* static */ void QueryNetworkGameSocketHandler::SendReceive()
{
	for (auto it = QueryNetworkGameSocketHandler::queries.begin(); it != QueryNetworkGameSocketHandler::queries.end(); /* nothing */) {
		if (!(*it)->Receive()) {
			it = QueryNetworkGameSocketHandler::queries.erase(it);
		} else if (!(*it)->CheckConnection()) {
			it = QueryNetworkGameSocketHandler::queries.erase(it);
		} else {
			it++;
		}
	}

	for (auto &query : QueryNetworkGameSocketHandler::queries) {
		query->Send();
	}
}
