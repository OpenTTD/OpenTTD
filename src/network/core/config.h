/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file config.h Configuration options of the network stuff. It is used even when compiling without network support.
 */

#ifndef NETWORK_CORE_CONFIG_H
#define NETWORK_CORE_CONFIG_H

/** DNS hostname of the masterserver */
static const char * const NETWORK_MASTER_SERVER_HOST            = "master.openttd.org";
/** DNS hostname of the content server */
static const char * const NETWORK_CONTENT_SERVER_HOST           = "content.openttd.org";
/** DNS hostname of the HTTP-content mirror server */
static const char * const NETWORK_CONTENT_MIRROR_HOST           = "binaries.openttd.org";
/** URL of the HTTP mirror system */
static const char * const NETWORK_CONTENT_MIRROR_URL            = "/bananas";
/** Message sent to the masterserver to 'identify' this client as OpenTTD */
static const char * const NETWORK_MASTER_SERVER_WELCOME_MESSAGE = "OpenTTDRegister";

static const uint16 NETWORK_MASTER_SERVER_PORT    = 3978;         ///< The default port of the master server (UDP)
static const uint16 NETWORK_CONTENT_SERVER_PORT   = 3978;         ///< The default port of the content server (TCP)
static const uint16 NETWORK_CONTENT_MIRROR_PORT   =   80;         ///< The default port of the content mirror (TCP)
static const uint16 NETWORK_DEFAULT_PORT          = 3979;         ///< The default port of the game server (TCP & UDP)
static const uint16 NETWORK_ADMIN_PORT            = 3977;         ///< The default port for admin network
static const uint16 NETWORK_DEFAULT_DEBUGLOG_PORT = 3982;         ///< The default port debug-log is sent to (TCP)

static const uint16 SEND_MTU                      = 1460;         ///< Number of bytes we can pack in a single packet

static const byte NETWORK_GAME_ADMIN_VERSION      =    1;         ///< What version of the admin network do we use?
static const byte NETWORK_GAME_INFO_VERSION       =    4;         ///< What version of game-info do we use?
static const byte NETWORK_COMPANY_INFO_VERSION    =    6;         ///< What version of company info is this?
static const byte NETWORK_MASTER_SERVER_VERSION   =    2;         ///< What version of master-server-protocol do we use?

static const uint NETWORK_NAME_LENGTH             =   80;         ///< The maximum length of the server name and map name, in bytes including '\0'
static const uint NETWORK_COMPANY_NAME_LENGTH     =  128;         ///< The maximum length of the company name, in bytes including '\0'
static const uint NETWORK_HOSTNAME_LENGTH         =   80;         ///< The maximum length of the host name, in bytes including '\0'
static const uint NETWORK_SERVER_ID_LENGTH        =   33;         ///< The maximum length of the network id of the servers, in bytes including '\0'
static const uint NETWORK_REVISION_LENGTH         =   15;         ///< The maximum length of the revision, in bytes including '\0'
static const uint NETWORK_PASSWORD_LENGTH         =   33;         ///< The maximum length of the password, in bytes including '\0' (must be >= NETWORK_SERVER_ID_LENGTH)
static const uint NETWORK_CLIENTS_LENGTH          =  200;         ///< The maximum length for the list of clients that controls a company, in bytes including '\0'
static const uint NETWORK_CLIENT_NAME_LENGTH      =   25;         ///< The maximum length of a client's name, in bytes including '\0'
static const uint NETWORK_RCONCOMMAND_LENGTH      =  500;         ///< The maximum length of a rconsole command, in bytes including '\0'
static const uint NETWORK_GAMESCRIPT_JSON_LENGTH  = SEND_MTU - 3; ///< The maximum length of a gamescript json string, in bytes including '\0'. Must not be longer than SEND_MTU including header (3 bytes)
static const uint NETWORK_CHAT_LENGTH             =  900;         ///< The maximum length of a chat message, in bytes including '\0'

static const uint NETWORK_GRF_NAME_LENGTH         =   80;         ///< Maximum length of the name of a GRF

/**
 * Maximum number of GRFs that can be sent.
 * This limit is reached when PACKET_UDP_SERVER_RESPONSE reaches the maximum size of SEND_MTU bytes.
 */
static const uint NETWORK_MAX_GRF_COUNT           =   62;

static const uint NETWORK_NUM_LANGUAGES           =   36; ///< Number of known languages (to the network protocol) + 1 for 'any'.

/**
 * The number of landscapes in OpenTTD.
 * This number must be equal to NUM_LANDSCAPE, but as this number is used
 * within the network code and that the network code is shared with the
 * masterserver/updater, it has to be declared in here too. In network.cpp
 * there is a compile assertion to check that this NUM_LANDSCAPE is equal
 * to NETWORK_NUM_LANDSCAPES.
 */
static const uint NETWORK_NUM_LANDSCAPES          =    4;

#endif /* NETWORK_CORE_CONFIG_H */
