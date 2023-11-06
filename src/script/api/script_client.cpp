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
 * @return A pointer to corresponding CI struct or nullptr when not found.
 */
static NetworkClientInfo *FindClientInfo(ScriptClient::ClientID client)
{
	if (client == ScriptClient::CLIENT_INVALID) return nullptr;
	if (!_networking) return nullptr;
	return NetworkClientInfo::GetByClientID((::ClientID)client);
}

/* static */ ScriptClient::ClientID ScriptClient::ResolveClientID(ScriptClient::ClientID client)
{
	return (FindClientInfo(client) == nullptr ? ScriptClient::CLIENT_INVALID : client);
}

/* static */ std::optional<std::string> ScriptClient::GetName(ScriptClient::ClientID client)
{
	NetworkClientInfo *ci = FindClientInfo(client);
	if (ci == nullptr) return std::nullopt;
	return ci->client_name;
}

/* static */ ScriptCompany::CompanyID ScriptClient::GetCompany(ScriptClient::ClientID client)
{
	NetworkClientInfo *ci = FindClientInfo(client);
	if (ci == nullptr) return ScriptCompany::COMPANY_INVALID;
	return (ScriptCompany::CompanyID)ci->client_playas;
}

/* static */ ScriptDate::Date ScriptClient::GetJoinDate(ScriptClient::ClientID client)
{
	NetworkClientInfo *ci = FindClientInfo(client);
	if (ci == nullptr) return ScriptDate::DATE_INVALID;
	return (ScriptDate::Date)ci->join_date.base();
}
