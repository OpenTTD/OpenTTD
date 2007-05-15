/* $Id$ */

#ifndef NETWORK_GUI_H
#define NETWORK_GUI_H

#ifdef ENABLE_NETWORK

#include "network_data.h"

void ShowNetworkNeedPassword(NetworkPasswordType npt);
void ShowNetworkGiveMoneyWindow(byte player); // PlayerID
void ShowNetworkChatQueryWindow(DestType type, int dest);
void ShowJoinStatusWindowAfterJoin(void);
void ShowNetworkGameWindow(void);
void ShowClientList(void);

#else /* ENABLE_NETWORK */
/* Network function stubs when networking is disabled */

static inline void ShowNetworkChatQueryWindow(byte desttype, int dest) {}
static inline void ShowClientList(void) {}
static inline void ShowJoinStatusWindowAfterJoin(void) {}
static inline void ShowNetworkGameWindow(void) {}

#endif /* ENABLE_NETWORK */

#endif /* NETWORK_GUI_H */
