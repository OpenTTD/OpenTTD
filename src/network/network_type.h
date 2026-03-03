/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file network_type.h Types used for networking. */

#ifndef NETWORK_TYPE_H
#define NETWORK_TYPE_H

#include "../core/enum_type.hpp"
#include "../core/pool_type.hpp"

/** How many clients can we have */
static const uint MAX_CLIENTS = 255;

/**
 * Vehicletypes in the order they are send in info packets.
 */
enum class NetworkVehicleType : uint8_t {
	Train = 0, ///< A train.
	Truck, ///< A road vehicle that stops at truck stops
	Bus, ///< A road vehicle that stops at bus stops.
	Aircraft, ///< An airplane or helicopter.
	Ship, ///< A ship.

	End ///< End marker for array sizes.
};

/**
 * Game type the server can be using.
 * Used on the network protocol to communicate with Game Coordinator.
 */
enum class ServerGameType : uint8_t {
	Local = 0, ///< Do not communicate with the game coordinator.
	Public, ///< The game is publicly accessible.
	InviteOnly, ///< The game can be accessed if you know the invite code.
};

/** 'Unique' identifier to be given to clients */
enum ClientID : uint32_t {
	INVALID_CLIENT_ID = 0, ///< Client is not part of anything
	CLIENT_ID_SERVER  = 1, ///< Servers always have this ID
	CLIENT_ID_FIRST   = 2, ///< The first client ID
};

/** Indices into the client related pools */
using ClientPoolID = PoolID<uint16_t, struct ClientPoolIDTag, MAX_CLIENTS + 1 /* dedicated server. */, 0xFFFF>;

/** Indices into the admin tables. */
using AdminID = PoolID<uint8_t, struct AdminIDTag, 16, 0xFF>;

/** Simple calculated statistics of a company */
struct NetworkCompanyStats {
	/** Array indexed by NetworkVehicleType. */
	using NetworkVehicleTypeArray = EnumClassIndexContainer<std::array<uint16_t, to_underlying(NetworkVehicleType::End)>, NetworkVehicleType>;
	NetworkVehicleTypeArray num_vehicle; ///< How many vehicles are there of this type?
	NetworkVehicleTypeArray num_station; ///< How many stations are there of this type?
};

struct NetworkClientInfo;

/**
 * Destination of our chat messages.
 * @warning The values of the enum items are part of the admin network API. Only append at the end.
 */
enum DestType : uint8_t {
	DESTTYPE_BROADCAST, ///< Send message/notice to all clients (All)
	DESTTYPE_TEAM,      ///< Send message/notice to everyone playing the same company (Team)
	DESTTYPE_CLIENT,    ///< Send message/notice to only a certain client (Private)
};
DECLARE_ENUM_AS_ADDABLE(DestType)

/**
 * Actions that can be used for NetworkTextMessage.
 * @warning The values of the enum items are part of the admin network API. Only append at the end.
 */
enum NetworkAction : uint8_t {
	NETWORK_ACTION_JOIN,
	NETWORK_ACTION_LEAVE,
	NETWORK_ACTION_SERVER_MESSAGE,
	NETWORK_ACTION_CHAT,
	NETWORK_ACTION_CHAT_COMPANY,
	NETWORK_ACTION_CHAT_CLIENT,
	NETWORK_ACTION_GIVE_MONEY,
	NETWORK_ACTION_NAME_CHANGE,
	NETWORK_ACTION_COMPANY_SPECTATOR,
	NETWORK_ACTION_COMPANY_JOIN,
	NETWORK_ACTION_COMPANY_NEW,
	NETWORK_ACTION_KICKED,
	NETWORK_ACTION_EXTERNAL_CHAT,
};

/**
 * The error codes we send around in the protocols.
 * @warning The values of the enum items are part of the admin network API. Only append at the end.
 */
enum class NetworkErrorCode : uint8_t {
	General, ///< Fallback error code in case nothing matches.

	Desync, ///< Client tells that they desynced.
	SavegameFailed, ///< Client tells they could not load the savegame.
	ConnectionLost, ///< Connection to the client was lost.
	IllegalPacket, ///< A packet was received that has invalid content.
	NewGRFMismatch, ///< Client does not have the right NewGRFs.

	NotAuthorized, ///< The client tried to do something there are not authorized to.
	NotExpected, ///< The request/packet was not expected in the current state.
	WrongRevision, ///< The client is using the wrong revision.
	NameInUse, ///< The client has a duplicate name (and we couldn't make it unique).
	WrongPassword, ///< The client entered a wrong password.
	CompanyMismatch, ///< The client was impersonating another company.
	Kicked, ///< The client got kicked.
	Cheater, ///< The client is trying control companies in a way they are not supposed to.
	ServerFull, ///< The server is full.
	TooManyCommands, ///< The client has sent too many commands in a short time.
	TimeoutPassword, ///< The client has timed out providing a password.
	TimeoutComputer, ///< The client has timed out because the computer could not keep up with the server.
	TimeoutMap, ///< The client has timed out because it took too long to download the map.
	TimeoutJoin, ///< The client has timed out because getting up to speed with the server failed.
	InvalidClientName, ///< The client tried to set an invalid name.
	NotOnAllowList, ///< The client is not on the allow list.
	NoAuthenticationMethodAvailable, ///< The client and server could not find a common authentication method.

	/* When adding elements to this enumeration, update the mapping in GetLongNetworkErrorString and GetNetworkErrorMsg. */
};

/**
 * Simple helper to (more easily) manage authorized keys.
 *
 * The authorized keys are hexadecimal representations of their binary form.
 * The authorized keys are case insensitive.
 */
class NetworkAuthorizedKeys : public std::vector<std::string> {
public:
	bool Contains(std::string_view key) const;
	bool Add(std::string_view key);
	bool Remove(std::string_view key);
};

#endif /* NETWORK_TYPE_H */
