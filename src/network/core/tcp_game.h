/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tcp_game.h Basic functions to receive and send TCP packets for game purposes. */

#ifndef NETWORK_CORE_TCP_GAME_H
#define NETWORK_CORE_TCP_GAME_H

#include "os_abstraction.h"
#include "tcp.h"
#include "../network_type.h"
#include <chrono>

/**
 * Enum with all types of TCP packets.
 * For the exact meaning, look at #NetworkGameSocketHandler.
 */
enum class PacketGameType : uint8_t {
	/*
	 * These first ten packets must remain in this order for backward and forward compatibility
	 * between clients that are trying to join directly. These packets can be received and/or sent
	 * by the server before the server has processed the 'join' packet from the client.
	 */

	/* Packets sent by socket accepting code without ever constructing a client socket instance. */
	ServerFull, ///< The server is full and has no place for you.
	ServerBanned, ///< The server has banned you.

	/* Packets used by the client to join and an error message when the revision is wrong. */
	ClientJoin, ///< The client telling the server it wants to join.
	ServerError, ///< Server sending an error message to the client.

	/* Unused packet types, formerly used for the pre-game lobby. */
	ClientUnused, ///< Unused.
	ServerUnused, ///< Unused.

	/* Packets used to get the game info. */
	ServerGameInfo, ///< Information about the server.
	ClientGameInfo, ///< Request information about the server.

	/* A server quitting this game. */
	ServerNewGame, ///< The server is preparing to start a new game.
	ServerShutdown, ///< The server is shutting down.

	/*
	 * Packets after here assume that the client
	 * and server are running the same version. As
	 * such ordering is unimportant from here on.
	 *
	 * The following is the remainder of the packets
	 * sent as part of authenticating and getting
	 * the map and other important data.
	 */

	/* After the join step, the first perform game authentication and enabling encryption. */
	ServerAuthenticationRequest, ///< The server requests the client to authenticate using a number of methods.
	ClientAuthenticationResponse, ///< The client responds to the authentication request.
	ServerEnableEncryption, ///< The server tells that authentication has completed and requests to enable encryption with the keys of the last \c PacketGameType::ClientAuthenticationResponse.

	/* After the authentication is done, the next step is identification. */
	ClientIdentify, ///< Client telling the server the client's name and requested company.

	/* After the identify step, the next is checking NewGRFs. */
	ServerCheckNewGRFs, ///< Server sends NewGRF IDs and MD5 checksums for the client to check.
	ClientNewGRFsChecked, ///< Client acknowledges that it has all required NewGRFs.

	/* The server welcomes the authenticated client and sends information of other clients. */
	ServerWelcome, ///< Server welcomes you and gives you your #ClientID.
	ServerClientInfo, ///< Server sends you information about a client.

	/* Getting the savegame/map. */
	ClientGetMap, ///< Client requests the actual map.
	ServerWaitForMap, ///< Server tells the client there are some people waiting for the map as well.
	ServerMapBegin, ///< Server tells the client that it is beginning to send the map.
	ServerMapSize, ///< Server tells the client what the (compressed) size of the map is.
	ServerMapData, ///< Server sends bits of the map to the client.
	ServerMapDone, ///< Server tells it has just sent the last bits of the map to the client.
	ClientMapOk, ///< Client tells the server that it received the whole map.

	ServerClientJoined, ///< Tells clients that a new client has joined.

	/*
	 * At this moment the client has the map and
	 * the client is fully authenticated. Now the
	 * normal communication starts.
	 */

	/* Game progress monitoring. */
	ServerFrame, ///< Server tells the client what frame it is in, and thus to where the client may progress.
	ClientAck, ///< The client tells the server which frame it has executed.
	ServerSync, ///< Server tells the client what the random state should be.

	/* Sending commands around. */
	ClientCommand, ///< Client executed a command and sends it to the server.
	ServerCommand, ///< Server distributes a command to (all) the clients.

	/* Human communication! */
	ClientChat, ///< Client said something that should be distributed.
	ServerChat, ///< Server distributing the message of a client (or itself).
	ServerExternalChat, ///< Server distributing the message from external source.

	/* Remote console. */
	ClientRemoteConsoleCommand, ///< Client asks the server to execute some command.
	ServerRemoteConsoleCommand, ///< Response of the executed command on the server.

	/* Moving a client.*/
	ClientMove, ///< A client would like to be moved to another company.
	ServerMove, ///< Server tells everyone that someone is moved to another company.

	/* Configuration updates. */
	ClientSetName, ///< A client changes its name.
	ServerConfigurationUpdate, ///< Some network configuration important to the client changed.

	/* A client quitting. */
	ClientQuit, ///< A client tells the server it is going to quit.
	ServerQuit, ///< A server tells that a client has quit.
	ClientError, ///< A client reports an error to the server.
	ServerErrorQuit, ///< A server tells that a client has hit an error and did quit.
};
/** Mark PacketGameType as PacketType. */
template <> struct IsEnumPacketType<PacketGameType> {
	static constexpr bool value = true; ///< This is an enumeration of a PacketType.
};

/** Packet that wraps a command */
struct CommandPacket;

/**
 * A "queue" of CommandPackets.
 * Not a std::queue because, when paused, some commands remain on the queue.
 * In other words, you do not always pop the first element from this queue.
 */
using CommandQueue = std::vector<CommandPacket>;

/** Base socket handler for all TCP sockets */
class NetworkGameSocketHandler : public NetworkTCPSocketHandler {
/* TODO: rewrite into a proper class */
private:
	NetworkClientInfo *info = nullptr; ///< Client info related to this socket
	bool is_pending_deletion = false; ///< Whether this socket is pending deletion

protected:
	NetworkRecvStatus ReceiveInvalidPacket(PacketGameType type);

	/**
	 * Notification that the server is full.
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus ReceiveServerFull(Packet &p);

	/**
	 * Notification that the client trying to join is banned.
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus ReceiveServerBanned(Packet &p);

	/**
	 * Try to join the server:
	 * string   OpenTTD revision (norev0000 if no revision).
	 * uint32_t NewGRF version (added in 1.2).
	 * string   Name of the client (max NETWORK_NAME_LENGTH) (removed in 15).
	 * uint8_t  ID of the company to play as (1..MAX_COMPANIES) (removed in 15).
	 * uint8_t  ID of the clients Language (removed in 15).
	 * string   Client's unique identifier (removed in 1.0).
	 *
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus ReceiveClientJoin(Packet &p);

	/**
	 * The client made an error:
	 * uint8_t   Error code caused (see NetworkErrorCode).
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus ReceiveServerError(Packet &p);

	/**
	 * Request game information.
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus ReceiveClientGameInfo(Packet &p);

	/**
	 * Sends information about the game.
	 * Serialized NetworkGameInfo. See game_info.h for details.
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus ReceiveServerGameInfo(Packet &p);

	/**
	 * Send information about a client:
	 * uint32_t  ID of the client (always unique on a server. 1 = server, 0 is invalid).
	 * uint8_t   ID of the company the client is playing as (255 for spectators).
	 * string Name of the client.
	 * string Public key of the client.
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus ReceiveServerClientInfo(Packet &p);

	/**
	 * The client tells the server about the identity of the client:
	 * string  Name of the client (max NETWORK_NAME_LENGTH).
	 * uint8_t ID of the company to play as (1..MAX_COMPANIES, or COMPANY_SPECTATOR).
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus ReceiveClientIdentify(Packet &p);

	/**
	 * Indication to the client that it needs to authenticate:
	 * uint8_t The \c NetworkAuthenticationMethod to use.
	 * 32 * uint8_t Public key of the server.
	 * 24 * uint8_t Nonce for the key exchange.
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus ReceiveServerAuthenticationRequest(Packet &p);

	/**
	 * Send the response to the authentication request:
	 * 32 * uint8_t Public key of the client.
	 * 16 * uint8_t Message authentication code.
	 *  8 * uint8_t Random message that got encoded and signed.
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus ReceiveClientAuthenticationResponse(Packet &p);

	/**
	 * Indication to the client that authentication is complete and encryption has to be used from here on forward.
	 * The encryption uses the shared keys generated by the last AUTH_REQUEST key exchange.
	 * 24 * uint8_t Nonce for encrypted connection.
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus ReceiveServerEnableEncryption(Packet &p);

	/**
	 * The client is joined and ready to receive their map:
	 * uint32_t  Own client ID.
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus ReceiveServerWelcome(Packet &p);

	/**
	 * Request the map from the server.
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus ReceiveClientGetMap(Packet &p);

	/**
	 * Notification that another client is currently receiving the map:
	 * uint8_t   Number of clients waiting in front of you.
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus ReceiveServerWaitForMap(Packet &p);

	/**
	 * Sends that the server will begin with sending the map to the client:
	 * uint32_t  Current frame.
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus ReceiveServerMapBegin(Packet &p);

	/**
	 * Sends the size of the map to the client.
	 * uint32_t  Size of the (compressed) map (in bytes).
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus ReceiveServerMapSize(Packet &p);

	/**
	 * Sends the data of the map to the client:
	 * Contains a part of the map (until max size of packet).
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus ReceiveServerMapData(Packet &p);

	/**
	 * Sends that all data of the map are sent to the client:
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus ReceiveServerMapDone(Packet &p);

	/**
	 * Tell the server that we are done receiving/loading the map.
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus ReceiveClientMapOk(Packet &p);

	/**
	 * A client joined (PacketGameType::ClientMapOk), what usually directly follows is a PacketGameType::ServerClientInfo:
	 * uint32_t  ID of the client that just joined the game.
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus ReceiveServerClientJoined(Packet &p);

	/**
	 * Sends the current frame counter to the client:
	 * uint32_t  Frame counter
	 * uint32_t  Frame counter max (how far may the client walk before the server?)
	 * uint32_t  General seed 1 (dependent on compile settings, not default).
	 * uint32_t  General seed 2 (dependent on compile settings, not default).
	 * uint8_t   Random token to validate the client is actually listening (only occasionally present).
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus ReceiveServerFrame(Packet &p);

	/**
	 * Sends a sync-check to the client:
	 * uint32_t  Frame counter.
	 * uint32_t  General seed 1.
	 * uint32_t  General seed 2 (dependent on compile settings, not default).
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus ReceiveServerSync(Packet &p);

	/**
	 * Tell the server we are done with this frame:
	 * uint32_t  Current frame counter of the client.
	 * uint8_t   The random token that the server sent in the PacketGameType::ServerFrame packet.
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus ReceiveClientAck(Packet &p);

	/**
	 * Send a DoCommand to the Server:
	 * uint8_t   ID of the company (0..MAX_COMPANIES-1).
	 * uint32_t  ID of the command (see command.h).
	 * `<var>` Command specific buffer with encoded parameters of variable length.
	 *         The content differs per command and can change without notification.
	 * uint8_t   ID of the callback.
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus ReceiveClientCommand(Packet &p);

	/**
	 * Sends a DoCommand to the client:
	 * uint8_t   ID of the company (0..MAX_COMPANIES-1).
	 * uint32_t  ID of the command (see command.h).
	 * `<var>` Command specific buffer with encoded parameters of variable length.
	 *         The content differs per command and can change without notification.
	 * uint8_t   ID of the callback.
	 * uint32_t  Frame of execution.
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus ReceiveServerCommand(Packet &p);

	/**
	 * Sends a chat-packet to the server:
	 * uint8_t   ID of the action (see NetworkAction).
	 * uint8_t   ID of the destination type (see #NetworkChatDestinationType).
	 * uint32_t  ID of the client or company (destination of the chat).
	 * string  Message (max NETWORK_CHAT_LENGTH).
	 * uint64_t  data (used e.g. for 'give money' actions).
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus ReceiveClientChat(Packet &p);

	/**
	 * Sends a chat-packet to the client:
	 * uint8_t   ID of the action (see NetworkAction).
	 * uint32_t  ID of the client (origin of the chat).
	 * string  Message (max NETWORK_CHAT_LENGTH).
	 * uint64_t  data (used e.g. for 'give money' actions).
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus ReceiveServerChat(Packet &p);

	/**
	 * Sends a chat-packet for external source to the client:
	 * string  Name of the source this message came from.
	 * uint16_t  TextColour to use for the message.
	 * string  Name of the user who sent the message.
	 * string  Message (max NETWORK_CHAT_LENGTH).
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus ReceiveServerExternalChat(Packet &p);

	/**
	 * Gives the client a new name:
	 * string  New name of the client.
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus ReceiveClientSetName(Packet &p);

	/**
	 * The client is quitting the game.
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus ReceiveClientQuit(Packet &p);

	/**
	 * The client made an error and is quitting the game.
	 * uint8_t   Error of the code caused (see NetworkErrorCode).
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus ReceiveClientError(Packet &p);

	/**
	 * Notification that a client left the game:
	 * uint32_t  ID of the client.
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus ReceiveServerQuit(Packet &p);

	/**
	 * Inform all clients that one client made an error and thus has quit/been disconnected:
	 * uint32_t  ID of the client that caused the error.
	 * uint8_t   Code of the error caused (see NetworkErrorCode).
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus ReceiveServerErrorQuit(Packet &p);

	/**
	 * Let the clients know that the server is closing.
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus ReceiveServerShutdown(Packet &p);

	/**
	 * Let the clients know that the server is loading a new map.
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus ReceiveServerNewGame(Packet &p);

	/**
	 * Send the result of an issues RCon command back to the client:
	 * uint16_t  Colour code.
	 * string  Output of the RCon command
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus ReceiveServerRemoteConsoleCommand(Packet &p);

	/**
	 * Send an RCon command to the server:
	 * string  RCon password.
	 * string  Command to be executed.
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus ReceiveClientRemoteConsoleCommand(Packet &p);

	/**
	 * Sends information about all used GRFs to the client:
	 * uint8_t   Amount of GRFs (the following data is repeated this many times, i.e. per GRF data).
	 * uint32_t  GRF ID
	 * 16 * uint8_t   MD5 checksum of the GRF
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus ReceiveServerCheckNewGRFs(Packet &p);

	/**
	 * Tell the server that we have the required GRFs
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus ReceiveClientNewGRFsChecked(Packet &p);

	/**
	 * Move a client from one company into another:
	 * uint32_t  ID of the client.
	 * uint8_t   ID of the new company.
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus ReceiveServerMove(Packet &p);

	/**
	 * Request the server to move this client into another company:
	 * uint8_t   ID of the company the client wants to join.
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus ReceiveClientMove(Packet &p);

	/**
	 * Update the clients knowledge of the max settings:
	 * uint8_t   Maximum number of companies allowed.
	 * uint8_t   Maximum number of spectators allowed.
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus ReceiveServerConfigurationUpdate(Packet &p);

	NetworkRecvStatus HandlePacket(Packet &p);

	NetworkGameSocketHandler(SOCKET s);
public:
	ClientID client_id = INVALID_CLIENT_ID; ///< Client identifier
	uint32_t last_frame = 0; ///< Last frame we have executed
	uint32_t last_frame_server = 0; ///< Last frame the server has executed
	CommandQueue incoming_queue; ///< The command-queue awaiting handling
	std::chrono::steady_clock::time_point last_packet{}; ///< Time we received the last frame.

	NetworkRecvStatus CloseConnection(bool error = true) override;

	/**
	 * Close the network connection due to the given status.
	 * @param status The reason the connection got closed.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus CloseConnection(NetworkRecvStatus status) = 0;
	~NetworkGameSocketHandler() override = default;

	/**
	 * Sets the client info for this socket handler.
	 * @param info The new client info.
	 */
	inline void SetInfo(NetworkClientInfo *info)
	{
		assert(info != nullptr && this->info == nullptr);
		this->info = info;
	}

	/**
	 * Gets the client info of this socket handler.
	 * @return The client info.
	 */
	inline NetworkClientInfo *GetInfo() const
	{
		return this->info;
	}

	NetworkRecvStatus ReceivePackets();

	std::optional<std::string_view> ReceiveCommand(Packet &p, CommandPacket &cp);
	void SendCommand(Packet &p, const CommandPacket &cp);

	/**
	 * Is this pending for deletion and as such should not be accessed anymore.
	 * @return \c true iff this is being deleted.
	 */
	bool IsPendingDeletion() const { return this->is_pending_deletion; }

	void DeferDeletion();
	static void ProcessDeferredDeletions();
};

#endif /* NETWORK_CORE_TCP_GAME_H */
