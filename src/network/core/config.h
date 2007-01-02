/* $Id$ */

#ifndef NETWORK_CORE_CONFIG_H
#define NETWORK_CORE_CONFIG_H

#ifdef ENABLE_NETWORK

/** DNS hostname of the masterserver */
#define NETWORK_MASTER_SERVER_HOST "master.openttd.org"
/** Message sent to the masterserver to 'identify' this client as OpenTTD */
#define NETWORK_MASTER_SERVER_WELCOME_MESSAGE "OpenTTDRegister"

enum {
	NETWORK_MASTER_SERVER_PORT    = 3978, ///< The default port of the master server (UDP)
	NETWORK_DEFAULT_PORT          = 3979, ///< The default port of the game server (TCP & UDP)

	SEND_MTU                      = 1460, ///< Number of bytes we can pack in a single packet

	NETWORK_GAME_INFO_VERSION     =    4, ///< What version of game-info do we use?
	NETWORK_COMPANY_INFO_VERSION  =    4, ///< What version of company info is this?
	NETWORK_MASTER_SERVER_VERSION =    1, ///< What version of master-server-protocol do we use?

	NETWORK_NAME_LENGTH           =   80, ///< The maximum length of the server name and map name, in bytes including '\0'
	NETWORK_HOSTNAME_LENGTH       =   80, ///< The maximum length of the host name, in bytes including '\0'
	NETWORK_REVISION_LENGTH       =   15, ///< The maximum length of the revision, in bytes including '\0'
	NETWORK_PASSWORD_LENGTH       =   20, ///< The maximum length of the password, in bytes including '\0'
	NETWORK_PLAYERS_LENGTH        =  200, ///< The maximum length for the list of players that controls a company, in bytes including '\0'
	NETWORK_CLIENT_NAME_LENGTH    =   25, ///< The maximum length of a player, in bytes including '\0'
	NETWORK_RCONCOMMAND_LENGTH    =  500, ///< The maximum length of a rconsole command, in bytes including '\0'

	NETWORK_GRF_NAME_LENGTH       =   80, ///< Maximum length of the name of a GRF
	/**
	 * Maximum number of GRFs that can be sent.
	 * This value is related to number of handles (files) OpenTTD can open.
	 * This is currently 64 and about 10 are currently used when OpenTTD loads
	 * without any NewGRFs. Therefore one can only load about 55 NewGRFs, so
	 * this is not a limit, but rather a way to easily check whether the limit
	 * imposed by the handle count is reached. Secondly it isn't possible to
	 * send much more GRF IDs + MD5sums in the PACKET_UDP_SERVER_RESPONSE, due
	 * to the limited size of UDP packets.
	 */
	NETWORK_MAX_GRF_COUNT         =   55,

	NETWORK_NUM_LANGUAGES         =    4, ///< Number of known languages (to the network protocol) + 1 for 'any'.
};

#endif /* ENABLE_NETWORK */

#endif /* NETWORK_CORE_CONFIG_H */
