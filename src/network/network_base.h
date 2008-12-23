/* $Id$ */

/** @file network_base.h Base core network types and some helper functions to access them. */

#ifndef NETWORK_BASE_H
#define NETWORK_BASE_H

#ifdef ENABLE_NETWORK

#include "network_type.h"
#include "../oldpool.h"

DECLARE_OLD_POOL(NetworkClientInfo, NetworkClientInfo, NCI_BITS_PER_POOL_BLOCK, MAX_CLIENT_SLOTS >> NCI_BITS_PER_POOL_BLOCK);

struct NetworkClientInfo : PoolItem<NetworkClientInfo, ClientIndex, &_NetworkClientInfo_pool> {
	ClientID client_id;                             ///< Client identifier (same as ClientState->client_id)
	char client_name[NETWORK_CLIENT_NAME_LENGTH];   ///< Name of the client
	byte client_lang;                               ///< The language of the client
	CompanyID client_playas;                        ///< As which company is this client playing (CompanyID)
	uint32 client_ip;                               ///< IP-address of the client (so he can be banned)
	Date join_date;                                 ///< Gamedate the client has joined
	char unique_id[NETWORK_UNIQUE_ID_LENGTH];       ///< Every play sends an unique id so we can indentify him

	NetworkClientInfo(ClientID client_id = INVALID_CLIENT_ID) : client_id(client_id) {}
	~NetworkClientInfo() { client_id = INVALID_CLIENT_ID; }

	inline bool IsValid() const { return client_id != INVALID_CLIENT_ID; }
};

static inline bool IsValidNetworkClientInfoIndex(ClientIndex index)
{
	return (uint)index < GetNetworkClientInfoPoolSize() && GetNetworkClientInfo(index)->IsValid();
}

#define FOR_ALL_CLIENT_INFOS_FROM(d, start) for (d = GetNetworkClientInfo(start); d != NULL; d = (d->index + 1U < GetNetworkClientInfoPoolSize()) ? GetNetworkClientInfo(d->index + 1U) : NULL) if (d->IsValid())
#define FOR_ALL_CLIENT_INFOS(d) FOR_ALL_CLIENT_INFOS_FROM(d, 0)

#endif /* ENABLE_NETWORK */
#endif /* NETWORK_BASE_H */
