/* $Id$ */

/** @file network_internal.h Variables and function used internally. */

#ifndef NETWORK_TYPE_H
#define NETWORK_TYPE_H

#ifdef ENABLE_NETWORK

#include "../player_type.h"
#include "../economy_type.h"
#include "core/config.h"
#include "core/game.h"

enum {
	/**
	 * How many clients can we have? Like.. MAX_PLAYERS - 1 is the amount of
	 *  players that can really play.. so.. a max of 4 spectators.. gives us..
	 *  MAX_PLAYERS + 3
	 */
	MAX_CLIENTS = MAX_PLAYERS + 3,

	/** Do not change this next line. It should _ALWAYS_ be MAX_CLIENTS + 1 */
	MAX_CLIENT_INFO = MAX_CLIENTS + 1,

	/** Maximum number of internet interfaces supported. */
	MAX_INTERFACES = 9,

	/** How many vehicle/station types we put over the network */
	NETWORK_VEHICLE_TYPES = 5,
	NETWORK_STATION_TYPES = 5,

	NETWORK_SERVER_INDEX = 1,
	NETWORK_EMPTY_INDEX  = 0,
};

struct NetworkPlayerInfo {
	char company_name[NETWORK_NAME_LENGTH];         ///< Company name
	char password[NETWORK_PASSWORD_LENGTH];         ///< The password for the player
	Year inaugurated_year;                          ///< What year the company started in
	Money company_value;                            ///< The company value
	Money money;                                    ///< The amount of money the company has
	Money income;                                   ///< How much did the company earned last year
	uint16 performance;                             ///< What was his performance last month?
	bool use_password;                              ///< Is there a password
	uint16 num_vehicle[NETWORK_VEHICLE_TYPES];      ///< How many vehicles are there of this type?
	uint16 num_station[NETWORK_STATION_TYPES];      ///< How many stations are there of this type?
	char players[NETWORK_PLAYERS_LENGTH];           ///< The players that control this company (Name1, name2, ..)
	uint16 months_empty;                            ///< How many months the company is empty
};

struct NetworkClientInfo {
	uint16 client_index;                            ///< Index of the client (same as ClientState->index)
	char client_name[NETWORK_CLIENT_NAME_LENGTH];   ///< Name of the client
	byte client_lang;                               ///< The language of the client
	PlayerID client_playas;                         ///< As which player is this client playing (PlayerID)
	uint32 client_ip;                               ///< IP-address of the client (so he can be banned)
	Date join_date;                                 ///< Gamedate the player has joined
	char unique_id[NETWORK_UNIQUE_ID_LENGTH];       ///< Every play sends an unique id so we can indentify him
};

enum NetworkPasswordType {
	NETWORK_GAME_PASSWORD,
	NETWORK_COMPANY_PASSWORD,
};

enum DestType {
	DESTTYPE_BROADCAST, ///< Send message/notice to all players (All)
	DESTTYPE_TEAM,      ///< Send message/notice to everyone playing the same company (Team)
	DESTTYPE_CLIENT,    ///< Send message/notice to only a certain player (Private)
};

/** Actions that can be used for NetworkTextMessage */
enum NetworkAction {
	NETWORK_ACTION_JOIN,
	NETWORK_ACTION_LEAVE,
	NETWORK_ACTION_SERVER_MESSAGE,
	NETWORK_ACTION_CHAT,
	NETWORK_ACTION_CHAT_COMPANY,
	NETWORK_ACTION_CHAT_CLIENT,
	NETWORK_ACTION_GIVE_MONEY,
	NETWORK_ACTION_NAME_CHANGE,
};

enum NetworkErrorCode {
	NETWORK_ERROR_GENERAL, // Try to use this one like never

	/* Signals from clients */
	NETWORK_ERROR_DESYNC,
	NETWORK_ERROR_SAVEGAME_FAILED,
	NETWORK_ERROR_CONNECTION_LOST,
	NETWORK_ERROR_ILLEGAL_PACKET,
	NETWORK_ERROR_NEWGRF_MISMATCH,

	/* Signals from servers */
	NETWORK_ERROR_NOT_AUTHORIZED,
	NETWORK_ERROR_NOT_EXPECTED,
	NETWORK_ERROR_WRONG_REVISION,
	NETWORK_ERROR_NAME_IN_USE,
	NETWORK_ERROR_WRONG_PASSWORD,
	NETWORK_ERROR_PLAYER_MISMATCH, // Happens in CLIENT_COMMAND
	NETWORK_ERROR_KICKED,
	NETWORK_ERROR_CHEATER,
	NETWORK_ERROR_FULL,
};

#endif /* ENABLE_NETWORK */
#endif /* NETWORK_TYPE_H */
