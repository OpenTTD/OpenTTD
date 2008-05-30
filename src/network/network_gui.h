/* $Id$ */

/** @file network_gui.h GUIs related to networking. */

#ifndef NETWORK_GUI_H
#define NETWORK_GUI_H

#ifdef ENABLE_NETWORK

#include "../window_type.h"
#include "network_type.h"

void ShowNetworkNeedPassword(NetworkPasswordType npt);
void ShowNetworkGiveMoneyWindow(PlayerID player); // PlayerID
void ShowNetworkChatQueryWindow(DestType type, int dest);
void ShowJoinStatusWindow();
void ShowNetworkGameWindow();
void ShowClientList();
void ShowNetworkCompanyPasswordWindow(Window *parent);

#else /* ENABLE_NETWORK */
/* Network function stubs when networking is disabled */

static inline void ShowNetworkChatQueryWindow(byte desttype, int dest) {}
static inline void ShowClientList() {}
static inline void ShowNetworkGameWindow() {}
static inline void ShowNetworkCompanyPasswordWindow(Window *parent) {}

#endif /* ENABLE_NETWORK */

#endif /* NETWORK_GUI_H */
