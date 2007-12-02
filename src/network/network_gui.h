/* $Id$ */

#ifndef NETWORK_GUI_H
#define NETWORK_GUI_H

#ifdef ENABLE_NETWORK

#include "network_data.h"

void ShowNetworkNeedPassword(NetworkPasswordType npt);
void ShowNetworkGiveMoneyWindow(PlayerID player); // PlayerID
void ShowNetworkChatQueryWindow(DestType type, int dest);
void ShowJoinStatusWindow();
void ShowNetworkGameWindow();
void ShowClientList();
void ShowNetworkCompanyPasswordWindow();

#else /* ENABLE_NETWORK */
/* Network function stubs when networking is disabled */

static inline void ShowNetworkChatQueryWindow(byte desttype, int dest) {}
static inline void ShowClientList() {}
static inline void ShowNetworkGameWindow() {}
static inline void ShowNetworkCompanyPasswordWindow() {}

#endif /* ENABLE_NETWORK */

#endif /* NETWORK_GUI_H */
