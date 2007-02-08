/* $Id$ */

#ifndef NETWORK_GAMELIST_H
#define NETWORK_GAMELIST_H

void NetworkGameListClear(void);
NetworkGameList *NetworkGameListAddItem(uint32 ip, uint16 port);
void NetworkGameListRemoveItem(NetworkGameList *remove);
void NetworkGameListAddQueriedItem(const NetworkGameInfo *info, bool server_online);
void NetworkGameListRequery(void);

#endif /* NETWORK_GAMELIST_H */
