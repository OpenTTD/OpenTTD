/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file network_admin.cpp Server part of the admin network protocol. */

#ifdef ENABLE_NETWORK

#include "../stdafx.h"
#include "../debug.h"
#include "../strings_func.h"
#include "../date_func.h"
#include "network_admin.h"
#include "network.h"
#include "network_base.h"
#include "../company_base.h"
#include "../console_func.h"
#include "../core/pool_func.hpp"
#include "../map_func.h"
#include "../rev.h"

#include "table/strings.h"

/* This file handles all the admin network commands. */

/** The amount of admins connected. */
byte _network_admins_connected = 0;

NetworkAdminSocketPool _networkadminsocket_pool("NetworkAdminSocket");
INSTANTIATE_POOL_METHODS(NetworkAdminSocket)

/** The timeout for authorisation of the client. */
static const int ADMIN_AUTHORISATION_TIMEOUT = 10000;


/** Frequencies, which may be registered for a certain update type. */
static const AdminUpdateFrequency _admin_update_type_frequencies[] = {
	ADMIN_FREQUENCY_POLL | ADMIN_FREQUENCY_DAILY | ADMIN_FREQUENCY_WEEKLY | ADMIN_FREQUENCY_MONTHLY | ADMIN_FREQUENCY_QUARTERLY | ADMIN_FREQUENCY_ANUALLY, ///< ADMIN_UPDATE_DATE
	ADMIN_FREQUENCY_POLL | ADMIN_FREQUENCY_AUTOMATIC,                                                                                                      ///< ADMIN_UPDATE_CLIENT_INFO
	ADMIN_FREQUENCY_POLL | ADMIN_FREQUENCY_AUTOMATIC,                                                                                                      ///< ADMIN_UPDATE_COMPANY_INFO
	ADMIN_FREQUENCY_POLL |                         ADMIN_FREQUENCY_WEEKLY | ADMIN_FREQUENCY_MONTHLY | ADMIN_FREQUENCY_QUARTERLY | ADMIN_FREQUENCY_ANUALLY, ///< ADMIN_UPDATE_COMPANY_ECONOMY
	ADMIN_FREQUENCY_POLL |                         ADMIN_FREQUENCY_WEEKLY | ADMIN_FREQUENCY_MONTHLY | ADMIN_FREQUENCY_QUARTERLY | ADMIN_FREQUENCY_ANUALLY, ///< ADMIN_UPDATE_COMPANY_STATS
};
assert_compile(lengthof(_admin_update_type_frequencies) == ADMIN_UPDATE_END);

/**
 * Create a new socket for the server side of the admin network.
 * @param s The socket to connect with.
 */
ServerNetworkAdminSocketHandler::ServerNetworkAdminSocketHandler(SOCKET s) : NetworkAdminSocketHandler(s)
{
	_network_admins_connected++;
	this->status = ADMIN_STATUS_INACTIVE;
	this->realtime_connect = _realtime_tick;
}

/**
 * Clear everything related to this admin.
 */
ServerNetworkAdminSocketHandler::~ServerNetworkAdminSocketHandler()
{
	_network_admins_connected--;
	DEBUG(net, 1, "[admin] '%s' (%s) has disconnected", this->admin_name, this->admin_version);
}

/**
 * Whether a connection is allowed or not at this moment.
 * @return Whether the connection is allowed.
 */
/* static */ bool ServerNetworkAdminSocketHandler::AllowConnection()
{
	return !StrEmpty(_settings_client.network.admin_password) && _network_admins_connected < MAX_ADMINS;
}

/** Send the packets for the server sockets. */
/* static */ void ServerNetworkAdminSocketHandler::Send()
{
	ServerNetworkAdminSocketHandler *as;
	FOR_ALL_ADMIN_SOCKETS(as) {
		if (as->status == ADMIN_STATUS_INACTIVE && as->realtime_connect + ADMIN_AUTHORISATION_TIMEOUT < _realtime_tick) {
			DEBUG(net, 1, "[admin] Admin did not send its authorisation within %d seconds", ADMIN_AUTHORISATION_TIMEOUT / 1000);
			as->CloseConnection(true);
			continue;
		}
		if (as->writable) {
			as->Send_Packets();
		}
	}
}

/* static */ void ServerNetworkAdminSocketHandler::AcceptConnection(SOCKET s, const NetworkAddress &address)
{
	ServerNetworkAdminSocketHandler *as = new ServerNetworkAdminSocketHandler(s);
	as->address = address; // Save the IP of the client
}

/***********
 * Sending functions for admin network
 ************/

NetworkRecvStatus ServerNetworkAdminSocketHandler::SendError(NetworkErrorCode error)
{
	Packet *p = new Packet(ADMIN_PACKET_SERVER_ERROR);

	p->Send_uint8(error);
	this->Send_Packet(p);

	char str[100];
	StringID strid = GetNetworkErrorMsg(error);
	GetString(str, strid, lastof(str));

	DEBUG(net, 1, "[admin] the admin '%s' (%s) made an error and has been disconnected. Reason: '%s'", this->admin_name, this->admin_version, str);

	return this->CloseConnection(true);
}

NetworkRecvStatus ServerNetworkAdminSocketHandler::SendProtocol()
{
	Packet *p = new Packet(ADMIN_PACKET_SERVER_PROTOCOL);

	/* announce the protocol version */
	p->Send_uint8(NETWORK_GAME_ADMIN_VERSION);

	for (int i = 0; i < ADMIN_UPDATE_END; i++) {
		p->Send_bool  (true);
		p->Send_uint16(i);
		p->Send_uint16(_admin_update_type_frequencies[i]);
	}

	p->Send_bool(false);
	this->Send_Packet(p);

	return this->SendWelcome();
}

NetworkRecvStatus ServerNetworkAdminSocketHandler::SendWelcome()
{
	this->status = ADMIN_STATUS_ACTIVE;

	Packet *p = new Packet(ADMIN_PACKET_SERVER_WELCOME);

	p->Send_string(_settings_client.network.server_name);
	p->Send_string(_openttd_revision);
	p->Send_bool  (_network_dedicated);

	p->Send_string(_network_game_info.map_name);
	p->Send_uint32(_settings_game.game_creation.generation_seed);
	p->Send_uint8 (_settings_game.game_creation.landscape);
	p->Send_uint32(ConvertYMDToDate(_settings_game.game_creation.starting_year, 0, 1));
	p->Send_uint16(MapSizeX());
	p->Send_uint16(MapSizeY());

	this->Send_Packet(p);

	return NETWORK_RECV_STATUS_OKAY;
}

NetworkRecvStatus ServerNetworkAdminSocketHandler::SendNewGame()
{
	Packet *p = new Packet(ADMIN_PACKET_SERVER_NEWGAME);
	this->Send_Packet(p);
	return NETWORK_RECV_STATUS_OKAY;
}

NetworkRecvStatus ServerNetworkAdminSocketHandler::SendShutdown()
{
	Packet *p = new Packet(ADMIN_PACKET_SERVER_SHUTDOWN);
	this->Send_Packet(p);
	return NETWORK_RECV_STATUS_OKAY;
}

NetworkRecvStatus ServerNetworkAdminSocketHandler::SendDate()
{
	Packet *p = new Packet(ADMIN_PACKET_SERVER_DATE);

	p->Send_uint32(_date);
	this->Send_Packet(p);

	return NETWORK_RECV_STATUS_OKAY;
}

NetworkRecvStatus ServerNetworkAdminSocketHandler::SendClientJoin(ClientID client_id)
{
	Packet *p = new Packet(ADMIN_PACKET_SERVER_CLIENT_JOIN);

	p->Send_uint32(client_id);
	this->Send_Packet(p);

	return NETWORK_RECV_STATUS_OKAY;
}

NetworkRecvStatus ServerNetworkAdminSocketHandler::SendClientInfo(const NetworkClientInfo *ci)
{
	Packet *p = new Packet(ADMIN_PACKET_SERVER_CLIENT_INFO);

	p->Send_uint32(ci->client_id);
	p->Send_string(const_cast<NetworkAddress &>(ci->client_address).GetHostname());
	p->Send_string(ci->client_name);
	p->Send_uint8 (ci->client_lang);
	p->Send_uint32(ci->join_date);
	p->Send_uint8 (ci->client_playas);

	this->Send_Packet(p);

	return NETWORK_RECV_STATUS_OKAY;
}

NetworkRecvStatus ServerNetworkAdminSocketHandler::SendClientUpdate(const NetworkClientInfo *ci)
{
	Packet *p = new Packet(ADMIN_PACKET_SERVER_CLIENT_UPDATE);

	p->Send_uint32(ci->client_id);
	p->Send_string(ci->client_name);
	p->Send_uint8 (ci->client_playas);

	this->Send_Packet(p);

	return NETWORK_RECV_STATUS_OKAY;
}

NetworkRecvStatus ServerNetworkAdminSocketHandler::SendClientQuit(ClientID client_id)
{
	Packet *p = new Packet(ADMIN_PACKET_SERVER_CLIENT_QUIT);

	p->Send_uint32(client_id);
	this->Send_Packet(p);

	return NETWORK_RECV_STATUS_OKAY;
}

NetworkRecvStatus ServerNetworkAdminSocketHandler::SendClientError(ClientID client_id, NetworkErrorCode error)
{
	Packet *p = new Packet(ADMIN_PACKET_SERVER_CLIENT_ERROR);

	p->Send_uint32(client_id);
	p->Send_uint8 (error);
	this->Send_Packet(p);

	return NETWORK_RECV_STATUS_OKAY;
}

NetworkRecvStatus ServerNetworkAdminSocketHandler::SendCompanyNew(CompanyID company_id)
{
	Packet *p = new Packet(ADMIN_PACKET_SERVER_COMPANY_NEW);
	p->Send_uint8(company_id);

	this->Send_Packet(p);

	return NETWORK_RECV_STATUS_OKAY;
}

NetworkRecvStatus ServerNetworkAdminSocketHandler::SendCompanyInfo(const Company *c)
{
	char company_name[NETWORK_COMPANY_NAME_LENGTH];
	char manager_name[NETWORK_COMPANY_NAME_LENGTH];

	SetDParam(0, c->index);
	GetString(company_name, STR_COMPANY_NAME, lastof(company_name));

	SetDParam(0, c->index);
	GetString(manager_name, STR_PRESIDENT_NAME, lastof(manager_name));

	Packet *p = new Packet(ADMIN_PACKET_SERVER_COMPANY_INFO);

	p->Send_uint8 (c->index);
	p->Send_string(company_name);
	p->Send_string(manager_name);
	p->Send_uint8 (c->colour);
	p->Send_bool  (NetworkCompanyIsPassworded(c->index));
	p->Send_uint32(c->inaugurated_year);
	p->Send_bool  (c->is_ai);

	this->Send_Packet(p);

	return NETWORK_RECV_STATUS_OKAY;
}

NetworkRecvStatus ServerNetworkAdminSocketHandler::SendCompanyUpdate(const Company *c)
{
	char company_name[NETWORK_COMPANY_NAME_LENGTH];
	char manager_name[NETWORK_COMPANY_NAME_LENGTH];

	SetDParam(0, c->index);
	GetString(company_name, STR_COMPANY_NAME, lastof(company_name));

	SetDParam(0, c->index);
	GetString(manager_name, STR_PRESIDENT_NAME, lastof(manager_name));

	Packet *p = new Packet(ADMIN_PACKET_SERVER_COMPANY_UPDATE);

	p->Send_uint8 (c->index);
	p->Send_string(company_name);
	p->Send_string(manager_name);
	p->Send_uint8 (c->colour);
	p->Send_bool  (NetworkCompanyIsPassworded(c->index));
	p->Send_uint8 (c->quarters_of_bankruptcy);

	for (size_t i = 0; i < lengthof(c->share_owners); i++) {
		p->Send_uint8(c->share_owners[i]);
	}

	this->Send_Packet(p);

	return NETWORK_RECV_STATUS_OKAY;
}

NetworkRecvStatus ServerNetworkAdminSocketHandler::SendCompanyRemove(CompanyID company_id, AdminCompanyRemoveReason acrr)
{
	Packet *p = new Packet(ADMIN_PACKET_SERVER_COMPANY_REMOVE);

	p->Send_uint8(company_id);
	p->Send_uint8(acrr);

	this->Send_Packet(p);

	return NETWORK_RECV_STATUS_OKAY;
}

NetworkRecvStatus ServerNetworkAdminSocketHandler::SendCompanyEconomy()
{
	const Company *company;
	FOR_ALL_COMPANIES(company) {
		/* Get the income. */
		Money income = 0;
		for (uint i = 0; i < lengthof(company->yearly_expenses[0]); i++) {
			income -= company->yearly_expenses[0][i];
		}

		Packet *p = new Packet(ADMIN_PACKET_SERVER_COMPANY_ECONOMY);

		p->Send_uint8(company->index);

		/* Current information. */
		p->Send_uint64(company->money);
		p->Send_uint64(company->current_loan);
		p->Send_uint64(income);
		p->Send_uint16(company->cur_economy.delivered_cargo);

		/* Send stats for the last 2 quarters. */
		for (uint i = 0; i < 2; i++) {
			p->Send_uint64(company->old_economy[i].company_value);
			p->Send_uint16(company->old_economy[i].performance_history);
			p->Send_uint16(company->old_economy[i].delivered_cargo);
		}

		this->Send_Packet(p);
	}


	return NETWORK_RECV_STATUS_OKAY;
}

NetworkRecvStatus ServerNetworkAdminSocketHandler::SendCompanyStats()
{
	/* Fetch the latest version of the stats. */
	NetworkCompanyStats company_stats[MAX_COMPANIES];
	NetworkPopulateCompanyStats(company_stats);

	const Company *company;

	/* Go through all the companies. */
	FOR_ALL_COMPANIES(company) {
		Packet *p = new Packet(ADMIN_PACKET_SERVER_COMPANY_STATS);

		/* Send the information. */
		p->Send_uint8(company->index);

		for (uint i = 0; i < NETWORK_VEH_END; i++) {
			p->Send_uint16(company_stats->num_vehicle[i]);
		}

		for (uint i = 0; i < NETWORK_VEH_END; i++) {
			p->Send_uint16(company_stats->num_station[i]);
		}

		this->Send_Packet(p);
	}

	return NETWORK_RECV_STATUS_OKAY;
}

/***********
 * Receiving functions
 ************/

DEF_ADMIN_RECEIVE_COMMAND(Server, ADMIN_PACKET_ADMIN_JOIN)
{
	if (this->status != ADMIN_STATUS_INACTIVE) return this->SendError(NETWORK_ERROR_NOT_EXPECTED);

	char password[NETWORK_PASSWORD_LENGTH];
	p->Recv_string(password, sizeof(password));

	if (StrEmpty(_settings_client.network.admin_password) ||
			strcmp(password, _settings_client.network.admin_password) != 0) {
		/* Password is invalid */
		return this->SendError(NETWORK_ERROR_WRONG_PASSWORD);
	}

	p->Recv_string(this->admin_name, sizeof(this->admin_name));
	p->Recv_string(this->admin_version, sizeof(this->admin_version));

	if (StrEmpty(this->admin_name) || StrEmpty(this->admin_version)) {
		/* no name or version supplied */
		return this->SendError(NETWORK_ERROR_ILLEGAL_PACKET);
	}

	DEBUG(net, 1, "[admin] '%s' (%s) has connected", this->admin_name, this->admin_version);

	return this->SendProtocol();
}

DEF_ADMIN_RECEIVE_COMMAND(Server, ADMIN_PACKET_ADMIN_QUIT)
{
	/* The admin is leaving nothing else to do */
	return this->CloseConnection();
}

DEF_ADMIN_RECEIVE_COMMAND(Server, ADMIN_PACKET_ADMIN_UPDATE_FREQUENCY)
{
	if (this->status == ADMIN_STATUS_INACTIVE) return this->SendError(NETWORK_ERROR_NOT_EXPECTED);

	AdminUpdateType type = (AdminUpdateType)p->Recv_uint16();
	AdminUpdateFrequency freq = (AdminUpdateFrequency)p->Recv_uint16();

	if (type >= ADMIN_UPDATE_END || (_admin_update_type_frequencies[type] & freq) != freq) {
		/* The server does not know of this UpdateType. */
		DEBUG(net, 3, "[admin] Not supported update frequency %d (%d) from '%s' (%s).", type, freq, this->admin_name, this->admin_version);
		return this->SendError(NETWORK_ERROR_ILLEGAL_PACKET);
	}

	this->update_frequency[type] = freq;

	return NETWORK_RECV_STATUS_OKAY;
}

DEF_ADMIN_RECEIVE_COMMAND(Server, ADMIN_PACKET_ADMIN_POLL)
{
	if (this->status == ADMIN_STATUS_INACTIVE) return this->SendError(NETWORK_ERROR_NOT_EXPECTED);

	AdminUpdateType type = (AdminUpdateType)p->Recv_uint8();
	uint32 d1 = p->Recv_uint32();

	switch (type) {
		case ADMIN_UPDATE_DATE:
			/* The admin is requesting the current date. */
			this->SendDate();
			break;

		case ADMIN_UPDATE_CLIENT_INFO:
			/* The admin is requesting client info. */
			const NetworkClientInfo *ci;
			if (d1 == UINT32_MAX) {
				FOR_ALL_CLIENT_INFOS(ci) {
					this->SendClientInfo(ci);
				}
			} else {
				ci = NetworkFindClientInfoFromClientID((ClientID)d1);
				if (ci != NULL) this->SendClientInfo(ci);
			}
			break;

		case ADMIN_UPDATE_COMPANY_INFO:
			/* The admin is asking for company info. */
			const Company *company;
			if (d1 == UINT32_MAX) {
				FOR_ALL_COMPANIES(company) {
					this->SendCompanyInfo(company);
				}
			} else {
				company = Company::GetIfValid(d1);
				if (company != NULL) this->SendCompanyInfo(company);
			}
			break;

		case ADMIN_UPDATE_COMPANY_ECONOMY:
			/* The admin is requesting economy info. */
			this->SendCompanyEconomy();
			break;

		case ADMIN_UPDATE_COMPANY_STATS:
			/* the admin is requesting company stats. */
			this->SendCompanyStats();
			break;

		default:
			/* An unsupported "poll" update type. */
			DEBUG(net, 3, "[admin] Not supported poll %d (%d) from '%s' (%s).", type, d1, this->admin_name, this->admin_version);
			return this->SendError(NETWORK_ERROR_ILLEGAL_PACKET);
	}

	return NETWORK_RECV_STATUS_OKAY;
}

/*
 * Useful wrapper functions
 */

/**
 * Notify the admin network of a new client (if they did opt in for the respective update).
 * @param ci the client info.
 * @param new_client if this is a new client, send the respective packet too.
 */
void NetworkAdminClientInfo(const NetworkClientInfo *ci, bool new_client)
{
	ServerNetworkAdminSocketHandler *as;
	FOR_ALL_ADMIN_SOCKETS(as) {
		if (as->update_frequency[ADMIN_UPDATE_CLIENT_INFO] & ADMIN_FREQUENCY_AUTOMATIC) {
			as->SendClientInfo(ci);
			if (new_client) {
				as->SendClientJoin(ci->client_id);
			}
		}
	}
}

/**
 * Notify the admin network of a client update (if they did opt in for the respective update).
 * @param ci the client info.
 */
void NetworkAdminClientUpdate(const NetworkClientInfo *ci)
{
	ServerNetworkAdminSocketHandler *as;
	FOR_ALL_ADMIN_SOCKETS(as) {
		if (as->update_frequency[ADMIN_UPDATE_CLIENT_INFO] & ADMIN_FREQUENCY_AUTOMATIC) {
			as->SendClientUpdate(ci);
		}
	}
}

/**
 * Notify the admin network that a client quit (if they have opt in for the respective update).
 * @param client_id of the client that quit.
 */
void NetworkAdminClientQuit(ClientID client_id)
{
	ServerNetworkAdminSocketHandler *as;
	FOR_ALL_ADMIN_SOCKETS(as) {
		if (as->update_frequency[ADMIN_UPDATE_CLIENT_INFO] & ADMIN_FREQUENCY_AUTOMATIC) {
			as->SendClientQuit(client_id);
		}
	}
}

/**
 * Notify the admin network of a client error (if they have opt in for the respective update).
 * @param client_id the client that made the error.
 * @param error_code the error that was caused.
 */
void NetworkAdminClientError(ClientID client_id, NetworkErrorCode error_code)
{
	ServerNetworkAdminSocketHandler *as;
	FOR_ALL_ADMIN_SOCKETS(as) {
		if (as->update_frequency[ADMIN_UPDATE_CLIENT_INFO] & ADMIN_FREQUENCY_AUTOMATIC) {
			as->SendClientError(client_id, error_code);
		}
	}
}

/**
 * Notify the admin network of company details.
 * @param company the company of which details will be sent into the admin network.
 * @param new_company whether this is a new company or not.
 */
void NetworkAdminCompanyInfo(const Company *company, bool new_company)
{
	if (company == NULL) {
		DEBUG(net, 1, "[admin] Empty company given for update");
		return;
	}

	ServerNetworkAdminSocketHandler *as;
	FOR_ALL_ADMIN_SOCKETS(as) {
		if (as->update_frequency[ADMIN_UPDATE_COMPANY_INFO] != ADMIN_FREQUENCY_AUTOMATIC) continue;

		as->SendCompanyInfo(company);
		if (new_company) {
			as->SendCompanyNew(company->index);
		}
	}
}

/**
 * Notify the admin network of company updates.
 * @param company company of which updates are going to be sent into the admin network.
 */
void NetworkAdminCompanyUpdate(const Company *company)
{
	if (company == NULL) return;

	ServerNetworkAdminSocketHandler *as;
	FOR_ALL_ADMIN_SOCKETS(as) {
		if (as->update_frequency[ADMIN_UPDATE_COMPANY_INFO] != ADMIN_FREQUENCY_AUTOMATIC) continue;

		as->SendCompanyUpdate(company);
	}
}

/**
 * Notify the admin network of a company to be removed (including the reason why).
 * @param company_id ID of the company that got removed.
 * @param bcrr the reason why the company got removed (e.g. bankruptcy).
 */
void NetworkAdminCompanyRemove(CompanyID company_id, AdminCompanyRemoveReason bcrr)
{
	ServerNetworkAdminSocketHandler *as;
	FOR_ALL_ADMIN_SOCKETS(as) {
		as->SendCompanyRemove(company_id, bcrr);
	}
}

/**
 * Send a Welcome packet to all connected admins
 */
void ServerNetworkAdminSocketHandler::WelcomeAll()
{
	ServerNetworkAdminSocketHandler *as;
	FOR_ALL_ADMIN_SOCKETS(as) {
		as->SendWelcome();
	}
}

/**
 * Send (push) updates to the admin network as they have registered for these updates.
 * @param freq the frequency to be processd.
 */
void NetworkAdminUpdate(AdminUpdateFrequency freq)
{
	ServerNetworkAdminSocketHandler *as;
	FOR_ALL_ADMIN_SOCKETS(as) {
		for (int i = 0; i < ADMIN_UPDATE_END; i++) {
			if (as->update_frequency[i] & freq) {
				/* Update the admin for the required details */
				switch (i) {
					case ADMIN_UPDATE_DATE:
						as->SendDate();
						break;

					case ADMIN_UPDATE_COMPANY_ECONOMY:
						as->SendCompanyEconomy();
						break;

					case ADMIN_UPDATE_COMPANY_STATS:
						as->SendCompanyStats();
						break;

					default: NOT_REACHED();
				}
			}
		}
	}
}

#endif /* ENABLE_NETWORK */
