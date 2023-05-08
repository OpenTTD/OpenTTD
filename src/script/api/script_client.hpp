/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_client.hpp Everything to query a network client's information */

#ifndef SCRIPT_CLIENT_HPP
#define SCRIPT_CLIENT_HPP

#include "script_text.hpp"
#include "script_date.hpp"
#include "script_company.hpp"
#include "../../network/network_type.h"

/**
 * Class that handles all client related functions.
 *
 * @api game
 */
class ScriptClient : public ScriptObject {
public:

	/** Different constants related to ClientID. */
	enum ClientID : uint32_t {
		CLIENT_INVALID = 0,  ///< Client is not part of anything
		CLIENT_SERVER  = 1,  ///< Servers always have this ID
		CLIENT_FIRST   = 2,  ///< The first client ID
	};

	/**
	 * Resolves the given client id to the correct index for the client.
	 *  If the client with the given id does not exist it will
	 *  return CLIENT_INVALID.
	 * @param client The client id to resolve.
	 * @return The resolved client id.
	 */
	static ClientID ResolveClientID(ClientID client);

	/**
	 * Get the name of the given client.
	 * @param client The client to get the name for.
	 * @pre ResolveClientID(client) != CLIENT_INVALID.
	 * @return The name of the given client.
	 */
	static std::optional<std::string> GetName(ClientID client);

	/**
	 * Get the company in which the given client is playing.
	 * @param client The client to get company for.
	 * @pre ResolveClientID(client) != CLIENT_INVALID.
	 * @return The company in which client is playing or COMPANY_SPECTATOR.
	 */
	static ScriptCompany::CompanyID GetCompany(ClientID client);

	/**
	 * Get the game date when the given client has joined.
	 * @param client The client to get joining date for.
	 * @pre ResolveClientID(client) != CLIENT_INVALID.
	 * @return The date when client has joined.
	 */
	static ScriptDate::Date GetJoinDate(ClientID client);
};


#endif /* SCRIPT_CLIENT_HPP */
