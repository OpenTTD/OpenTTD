/* $Id$ */

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
	enum ClientID {
		CLIENT_INVALID = 0,  ///< c1
		CLIENT_SERVER  = 1,  ///< c2
		CLIENT_FIRST   = 2,  ///< c3
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
	static char *GetName(ClientID client);

	/**
	 * Get the company in which the given client is playing.
	 * @param client The client to get company for.
	 * @pre ResolveClientID(client) != CLIENT_INVALID.
	 * @return The company in which client is playing.
	 */
	static ScriptCompany::CompanyID GetCompany(ClientID client);

	/**
	* Get the game date when the given client has joined.
	* @param client The client to get joining date for.
	* @pre ResolveClientID(client) != CLIENT_INVALID.
	* @return The date when client has joined.
	 */
	static ScriptDate::Date GetJoinedDate(ClientID client);

protected:
	/**
	* Finds NetworkClientInfo given client-identifier,
	   is used by other methods to resolve client-identifier.
	* @param client The client to get info structure for.
	* @return A pointer to corresponding CI struct or NULL when not found.
	 */
	 static NetworkClientInfo *FindClientInfo(ClientID client);
};


#endif /* SCRIPT_CLIENT_HPP */
