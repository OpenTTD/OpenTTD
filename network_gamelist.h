#ifndef NETWORK_GAMELIST_H
#define NETWORK_GAMELIST_H

void NetworkGameListClear(void);
NetworkGameList *NetworkGameListAddItem(uint32 ip, uint16 port);
void NetworkGameListAddQueriedItem(NetworkGameInfo *info, bool server_online);

#endif /* NETWORK_GAMELIST_H */
