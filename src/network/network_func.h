/* $Id$ */

/** @file network_func.h Network functions used by other parts of OpenTTD. */

#ifndef NETWORK_FUNC_H
#define NETWORK_FUNC_H

#ifdef ENABLE_NETWORK

#include "core/address.h"
#include "network_type.h"
#include "../console_type.h"
#include "../gfx_type.h"

extern NetworkServerGameInfo _network_game_info;
extern NetworkCompanyState *_network_company_states;

extern ClientID _network_own_client_id;
extern ClientID _redirect_console_to_client;
extern bool _network_need_advertise;
extern uint32 _network_last_advertise_frame;
extern uint8 _network_reconnect;
extern char *_network_host_list[10];
extern char *_network_ban_list[25];

byte NetworkSpectatorCount();
void NetworkUpdateClientName();
bool NetworkCompanyHasClients(CompanyID company);
bool NetworkChangeCompanyPassword(byte argc, char *argv[]);
void NetworkReboot();
void NetworkDisconnect();
void NetworkGameLoop();
void NetworkUDPGameLoop();
void NetworkUDPCloseAll();
void ParseConnectionString(const char **company, const char **port, char *connection_string);
void NetworkStartDebugLog(NetworkAddress address);
void NetworkPopulateCompanyStats(NetworkCompanyStats *stats);

void NetworkUpdateClientInfo(ClientID client_id);
void NetworkClientConnectGame(NetworkAddress address);
void NetworkClientRequestMove(CompanyID company, const char *pass = "");
void NetworkClientSendRcon(const char *password, const char *command);
void NetworkClientSendChat(NetworkAction action, DestType type, int dest, const char *msg, int64 data = 0);
void NetworkClientSetPassword(const char *password);
bool NetworkClientPreferTeamChat(const NetworkClientInfo *cio);
bool NetworkCompanyIsPassworded(CompanyID company_id);
bool NetworkMaxCompaniesReached();
bool NetworkMaxSpectatorsReached();
void NetworkPrintClients();

/*** Commands ran by the server ***/
void NetworkServerMonthlyLoop();
void NetworkServerYearlyLoop();
void NetworkServerChangeOwner(Owner current_owner, Owner new_owner);
void NetworkServerSendConfigUpdate();
void NetworkServerShowStatusToConsole();
bool NetworkServerStart();
void NetworkServerUpdateCompanyPassworded(CompanyID company_id, bool passworded);
bool NetworkServerChangeClientName(ClientID client_id, const char *new_name);

NetworkClientInfo *NetworkFindClientInfoFromIndex(ClientIndex index);
NetworkClientInfo *NetworkFindClientInfoFromClientID(ClientID client_id);
NetworkClientInfo *NetworkFindClientInfoFromIP(const char *ip);
const char *GetClientIP(const NetworkClientInfo *ci);

void NetworkServerDoMove(ClientID client_id, CompanyID company_id);
void NetworkServerSendRcon(ClientID client_id, ConsoleColour colour_code, const char *string);
void NetworkServerSendError(ClientID client_id, NetworkErrorCode error);
void NetworkServerSendChat(NetworkAction action, DestType type, int dest, const char *msg, ClientID from_id, int64 data = 0);

void NetworkServerKickClient(ClientID client_id);
void NetworkServerBanIP(const char *banip);

void NetworkInitChatMessage();
void CDECL NetworkAddChatMessage(TextColour colour, uint8 duration, const char *message, ...);
void NetworkUndrawChatMessage();
void NetworkChatMessageDailyLoop();

#endif /* ENABLE_NETWORK */
#endif /* NETWORK_FUNC_H */
