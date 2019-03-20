/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_client.cpp Implementation of ScriptClient. */

#include "../../stdafx.h"
#include "script_client.hpp"
#include "../../network/network.h"
#include "../../network/network_base.h"

#include "../../safeguards.h"

/**
 * Finds NetworkClientInfo given client-identifier,
 *  is used by other methods to resolve client-identifier.
 * @param client The client to get info structure for
 * @return A pointer to corresponding CI struct or NULL when not found.
 */
static NetworkClientInfo *FindClientInfo(ScriptClient::ClientID client)
{
	if (client == ScriptClient::CLIENT_INVALID) return NULL;
	if (!_networking) return NULL;
	return NetworkClientInfo::GetByClientID((::ClientID)client);
}

/* static */ ScriptClient::ClientID ScriptClient::ResolveClientID(ScriptClient::ClientID client)
{
	return (FindClientInfo(client) == NULL ? ScriptClient::CLIENT_INVALID : client);
}

/* static */ char *ScriptClient::GetName(ScriptClient::ClientID client)
{
	NetworkClientInfo *ci = FindClientInfo(client);
	if (ci == NULL) return NULL;
	return stredup(ci->client_name);
}

/* static */ ScriptCompany::CompanyID ScriptClient::GetCompany(ScriptClient::ClientID client)
{
	NetworkClientInfo *ci = FindClientInfo(client);
	if (ci == NULL) return ScriptCompany::COMPANY_INVALID;
	return (ScriptCompany::CompanyID)ci->client_playas;
}

/* static */ ScriptDate::Date ScriptClient::GetJoinDate(ScriptClient::ClientID client)
{
	NetworkClientInfo *ci = FindClientInfo(client);
	if (ci == NULL) return ScriptDate::DATE_INVALID;
	return (ScriptDate::Date)ci->join_date;
}
