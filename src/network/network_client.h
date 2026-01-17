/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file network_client.h Client part of the network protocol. */

#ifndef NETWORK_CLIENT_H
#define NETWORK_CLIENT_H

#include "network_internal.h"

/** Class for handling the client side of the game connection. */
class ClientNetworkGameSocketHandler : public NetworkGameSocketHandler {
private:
	std::unique_ptr<class NetworkAuthenticationClientHandler> authentication_handler = nullptr; ///< The handler for the authentication.
	std::string connection_string; ///< Address we are connected to.
	std::shared_ptr<struct PacketReader> savegame = nullptr; ///< Packet reader for reading the savegame.
	uint8_t token = 0; ///< The token we need to send back to the server to prove we're the right client.

	/** Status of the connection with the server. */
	enum ServerStatus : uint8_t {
		STATUS_INACTIVE,      ///< The client is not connected nor active.
		STATUS_JOIN,          ///< We are trying to join a server.
		STATUS_AUTH_GAME,     ///< Last action was requesting game (server) password.
		STATUS_ENCRYPTED,     ///< The game authentication has completed and from here on the connection to the server is encrypted.
		STATUS_NEWGRFS_CHECK, ///< Last action was checking NewGRFs.
		STATUS_AUTHORIZED,    ///< The client is authorized at the server.
		STATUS_MAP_WAIT,      ///< The client is waiting as someone else is downloading the map.
		STATUS_MAP,           ///< The client is downloading the map.
		STATUS_ACTIVE,        ///< The client is active within in the game.
		STATUS_END,           ///< Must ALWAYS be on the end of this list!! (period)
	};

	ServerStatus status = STATUS_INACTIVE; ///< Status of the connection with the server.

protected:
	friend void NetworkExecuteLocalCommandQueue();
	friend void NetworkClose(bool close_admins);
	static ClientNetworkGameSocketHandler *my_client; ///< This is us!

	NetworkRecvStatus ReceiveServerFull(Packet &p) override;
	NetworkRecvStatus ReceiveServerBanned(Packet &p) override;
	NetworkRecvStatus ReceiveServerError(Packet &p) override;
	NetworkRecvStatus ReceiveServerClientInfo(Packet &p) override;
	NetworkRecvStatus ReceiveServerAuthenticationRequest(Packet &p) override;
	NetworkRecvStatus ReceiveServerEnableEncryption(Packet &p) override;
	NetworkRecvStatus ReceiveServerWelcome(Packet &p) override;
	NetworkRecvStatus ReceiveServerWaitForMap(Packet &p) override;
	NetworkRecvStatus ReceiveServerMapBegin(Packet &p) override;
	NetworkRecvStatus ReceiveServerMapSize(Packet &p) override;
	NetworkRecvStatus ReceiveServerMapData(Packet &p) override;
	NetworkRecvStatus ReceiveServerMapDone(Packet &p) override;
	NetworkRecvStatus ReceiveServerClientJoined(Packet &p) override;
	NetworkRecvStatus ReceiveServerFrame(Packet &p) override;
	NetworkRecvStatus ReceiveServerSync(Packet &p) override;
	NetworkRecvStatus ReceiveServerCommand(Packet &p) override;
	NetworkRecvStatus ReceiveServerChat(Packet &p) override;
	NetworkRecvStatus ReceiveServerExternalChat(Packet &p) override;
	NetworkRecvStatus ReceiveServerQuit(Packet &p) override;
	NetworkRecvStatus ReceiveServerErrorQuit(Packet &p) override;
	NetworkRecvStatus ReceiveServerShutdown(Packet &p) override;
	NetworkRecvStatus ReceiveServerNewGame(Packet &p) override;
	NetworkRecvStatus ReceiveServerRemoteConsoleCommand(Packet &p) override;
	NetworkRecvStatus ReceiveServerCheckNewGRFs(Packet &p) override;
	NetworkRecvStatus ReceiveServerMove(Packet &p) override;
	NetworkRecvStatus ReceiveServerConfigurationUpdate(Packet &p) override;

	static NetworkRecvStatus SendNewGRFsOk();
	static NetworkRecvStatus SendGetMap();
	static NetworkRecvStatus SendMapOk();
	static NetworkRecvStatus SendIdentify();
	void CheckConnection();
public:
	ClientNetworkGameSocketHandler(SOCKET s, std::string_view connection_string);
	~ClientNetworkGameSocketHandler() override;

	NetworkRecvStatus CloseConnection(NetworkRecvStatus status) override;
	void ClientError(NetworkRecvStatus res);

	static NetworkRecvStatus SendJoin();
	static NetworkRecvStatus SendCommand(const CommandPacket &cp);
	static NetworkRecvStatus SendError(NetworkErrorCode errorno);
	static NetworkRecvStatus SendQuit();
	static NetworkRecvStatus SendAck();

	static NetworkRecvStatus SendAuthResponse();

	static NetworkRecvStatus SendChat(NetworkAction action, DestType type, int dest, std::string_view msg, int64_t data);
	static NetworkRecvStatus SendSetName(const std::string &name);
	static NetworkRecvStatus SendRCon(std::string_view password, std::string_view command);
	static NetworkRecvStatus SendMove(CompanyID company);

	static bool IsConnected();

	static void Send();
	static bool Receive();
	static bool GameLoop();
};

/** Helper to make the code look somewhat nicer. */
typedef ClientNetworkGameSocketHandler MyClient;

void NetworkClient_Connected();

/** Information required to join a server. */
struct NetworkJoinInfo {
	NetworkJoinInfo() : company(COMPANY_SPECTATOR) {}
	std::string connection_string; ///< The address of the server to join.
	CompanyID company;             ///< The company to join.
	std::string server_password;   ///< The password of the server to join.
};

extern NetworkJoinInfo _network_join;

#endif /* NETWORK_CLIENT_H */
