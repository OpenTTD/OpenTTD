/* $Id$ */

/** @file network_base.h Base core network types and some helper functions to access them. */

#ifndef NETWORK_BASE_H
#define NETWORK_BASE_H

#ifdef ENABLE_NETWORK

#include "network_type.h"
#include "../core/pool.hpp"

typedef Pool<NetworkClientInfo, ClientIndex, 8, MAX_CLIENT_SLOTS> NetworkClientInfoPool;
extern NetworkClientInfoPool _networkclientinfo_pool;

struct NetworkClientInfo : NetworkClientInfoPool::PoolItem<&_networkclientinfo_pool> {
	ClientID client_id;                             ///< Client identifier (same as ClientState->client_id)
	char client_name[NETWORK_CLIENT_NAME_LENGTH];   ///< Name of the client
	byte client_lang;                               ///< The language of the client
	CompanyID client_playas;                        ///< As which company is this client playing (CompanyID)
	NetworkAddress client_address;                  ///< IP-address of the client (so he can be banned)
	Date join_date;                                 ///< Gamedate the client has joined
	char unique_id[NETWORK_UNIQUE_ID_LENGTH];       ///< Every play sends an unique id so we can indentify him

	NetworkClientInfo(ClientID client_id = INVALID_CLIENT_ID) : client_id(client_id) {}
	~NetworkClientInfo() { client_id = INVALID_CLIENT_ID; }
};

#define FOR_ALL_CLIENT_INFOS_FROM(var, start) FOR_ALL_ITEMS_FROM(NetworkClientInfo, clientinfo_index, var, start)
#define FOR_ALL_CLIENT_INFOS(var) FOR_ALL_CLIENT_INFOS_FROM(var, 0)

#endif /* ENABLE_NETWORK */
#endif /* NETWORK_BASE_H */
