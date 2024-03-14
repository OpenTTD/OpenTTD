/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file test_network_crypto.cpp Tests for network related crypto functions. */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

#include "../core/format.hpp"
#include "../network/network_crypto_internal.h"
#include "../network/core/packet.h"
#include "../string_func.h"

/* The length of the hexadecimal representation of a X25519 key must fit in the key length. */
static_assert(NETWORK_SECRET_KEY_LENGTH >= X25519_KEY_SIZE * 2 + 1);

class MockNetworkSocketHandler : public NetworkSocketHandler {
public:
	MockNetworkSocketHandler(std::unique_ptr<NetworkEncryptionHandler> &&receive = {}, std::unique_ptr<NetworkEncryptionHandler> &&send = {})
	{
		this->receive_encryption_handler = std::move(receive);
		this->send_encryption_handler = std::move(send);
	}
};

static MockNetworkSocketHandler mock_socket_handler;

static std::tuple<Packet, bool> CreatePacketForReading(Packet &source, MockNetworkSocketHandler *socket_handler)
{
	source.PrepareToSend();

	Packet dest(socket_handler, COMPAT_MTU, source.Size());

	auto transfer_in = [](Packet &source, char *dest_data, size_t length) {
		auto transfer_out = [](char *dest_data, const char *source_data, size_t length) {
			std::copy(source_data, source_data + length, dest_data);
			return length;
		};
		return source.TransferOutWithLimit(transfer_out, length, dest_data);
	};
	dest.TransferIn(transfer_in, source);

	bool valid = dest.PrepareToRead();
	dest.Recv_uint8(); // Ignore the type
	return { dest, valid };
}

class TestPasswordRequestHandler : public NetworkAuthenticationPasswordRequestHandler {
private:
	std::string password;
public:
	TestPasswordRequestHandler(std::string &password) : password(password) {}
	void SendResponse() override {}
	void AskUserForPassword(std::shared_ptr<NetworkAuthenticationPasswordRequest> request) override { request->Reply(this->password); }
};

static void TestAuthentication(NetworkAuthenticationServerHandler &server, NetworkAuthenticationClientHandler &client,
		NetworkAuthenticationServerHandler::ResponseResult expected_response_result,
		NetworkAuthenticationClientHandler::RequestResult expected_request_result)
{
	Packet request(&mock_socket_handler, PacketType{});
	server.SendRequest(request);

	bool valid;
	std::tie(request, valid) = CreatePacketForReading(request, &mock_socket_handler);
	CHECK(valid);
	CHECK(client.ReceiveRequest(request) == expected_request_result);

	Packet response(&mock_socket_handler, PacketType{});
	client.SendResponse(response);

	std::tie(response, valid) = CreatePacketForReading(response, &mock_socket_handler);
	CHECK(valid);
	CHECK(server.ReceiveResponse(response) == expected_response_result);
}


TEST_CASE("Authentication_KeyExchangeOnly")
{
	X25519KeyExchangeOnlyServerHandler server(X25519SecretKey::CreateRandom());
	X25519KeyExchangeOnlyClientHandler client(X25519SecretKey::CreateRandom());

	TestAuthentication(server, client, NetworkAuthenticationServerHandler::AUTHENTICATED, NetworkAuthenticationClientHandler::READY_FOR_RESPONSE);
}


static void TestAuthenticationPAKE(std::string server_password, std::string client_password,
		NetworkAuthenticationServerHandler::ResponseResult expected_response_result)
{
	NetworkAuthenticationDefaultPasswordProvider server_password_provider(server_password);
	X25519PAKEServerHandler server(X25519SecretKey::CreateRandom(), &server_password_provider);
	X25519PAKEClientHandler client(X25519SecretKey::CreateRandom(), std::make_shared<TestPasswordRequestHandler>(client_password));

	TestAuthentication(server, client, expected_response_result, NetworkAuthenticationClientHandler::AWAIT_USER_INPUT);
}

TEST_CASE("Authentication_PAKE")
{
	SECTION("Correct password") {
		TestAuthenticationPAKE("sikrit", "sikrit", NetworkAuthenticationServerHandler::AUTHENTICATED);
	}

	SECTION("Empty password") {
		TestAuthenticationPAKE("", "", NetworkAuthenticationServerHandler::AUTHENTICATED);
	}

	SECTION("Wrong password") {
		TestAuthenticationPAKE("sikrit", "secret", NetworkAuthenticationServerHandler::NOT_AUTHENTICATED);
	}
}


static void TestAuthenticationAuthorizedKey(const X25519SecretKey &client_secret_key, const X25519PublicKey &server_expected_public_key,
		NetworkAuthenticationServerHandler::ResponseResult expected_response_result)
{
	std::vector<std::string> authorized_keys;
	authorized_keys.emplace_back(FormatArrayAsHex(server_expected_public_key));

	NetworkAuthenticationDefaultAuthorizedKeyHandler authorized_key_handler(authorized_keys);
	X25519AuthorizedKeyServerHandler server(X25519SecretKey::CreateRandom(), &authorized_key_handler);
	X25519AuthorizedKeyClientHandler client(client_secret_key);

	TestAuthentication(server, client, expected_response_result, NetworkAuthenticationClientHandler::READY_FOR_RESPONSE);
}

TEST_CASE("Authentication_AuthorizedKey")
{
	auto client_secret_key = X25519SecretKey::CreateRandom();
	auto valid_client_public_key = client_secret_key.CreatePublicKey();
	auto invalid_client_public_key = X25519SecretKey::CreateRandom().CreatePublicKey();

	SECTION("Correct public key") {
		TestAuthenticationAuthorizedKey(client_secret_key, valid_client_public_key, NetworkAuthenticationServerHandler::AUTHENTICATED);
	}

	SECTION("Incorrect public key") {
		TestAuthenticationAuthorizedKey(client_secret_key, invalid_client_public_key, NetworkAuthenticationServerHandler::NOT_AUTHENTICATED);
	}
}


TEST_CASE("Authentication_Combined")
{
	auto client_secret_key = X25519SecretKey::CreateRandom();
	std::string client_secret_key_str = FormatArrayAsHex(client_secret_key);
	auto client_public_key = client_secret_key.CreatePublicKey();
	std::string client_public_key_str = FormatArrayAsHex(client_public_key);

	std::vector<std::string> valid_authorized_keys;
	valid_authorized_keys.emplace_back(client_public_key_str);
	NetworkAuthenticationDefaultAuthorizedKeyHandler valid_authorized_key_handler(valid_authorized_keys);

	std::vector<std::string> invalid_authorized_keys;
	invalid_authorized_keys.emplace_back("not-a-valid-authorized-key");
	NetworkAuthenticationDefaultAuthorizedKeyHandler invalid_authorized_key_handler(invalid_authorized_keys);

	std::vector<std::string> no_authorized_keys;
	NetworkAuthenticationDefaultAuthorizedKeyHandler no_authorized_key_handler(no_authorized_keys);

	std::string no_password = "";
	NetworkAuthenticationDefaultPasswordProvider no_password_provider(no_password);
	std::string valid_password = "sikrit";
	NetworkAuthenticationDefaultPasswordProvider valid_password_provider(valid_password);
	std::string invalid_password = "secret";
	NetworkAuthenticationDefaultPasswordProvider invalid_password_provider(invalid_password);

	auto client = NetworkAuthenticationClientHandler::Create(std::make_shared<TestPasswordRequestHandler>(valid_password), client_secret_key_str, client_public_key_str);

	SECTION("Invalid authorized keys, invalid password") {
		auto server = NetworkAuthenticationServerHandler::Create(&invalid_password_provider, &invalid_authorized_key_handler);

		TestAuthentication(*server, *client, NetworkAuthenticationServerHandler::RETRY_NEXT_METHOD, NetworkAuthenticationClientHandler::READY_FOR_RESPONSE);
		TestAuthentication(*server, *client, NetworkAuthenticationServerHandler::NOT_AUTHENTICATED, NetworkAuthenticationClientHandler::AWAIT_USER_INPUT);
	}

	SECTION("Invalid authorized keys, valid password") {
		auto server = NetworkAuthenticationServerHandler::Create(&valid_password_provider, &invalid_authorized_key_handler);

		TestAuthentication(*server, *client, NetworkAuthenticationServerHandler::RETRY_NEXT_METHOD, NetworkAuthenticationClientHandler::READY_FOR_RESPONSE);
		TestAuthentication(*server, *client, NetworkAuthenticationServerHandler::AUTHENTICATED, NetworkAuthenticationClientHandler::AWAIT_USER_INPUT);
	}

	SECTION("Valid authorized keys, valid password") {
		auto server = NetworkAuthenticationServerHandler::Create(&valid_password_provider, &valid_authorized_key_handler);

		TestAuthentication(*server, *client, NetworkAuthenticationServerHandler::AUTHENTICATED, NetworkAuthenticationClientHandler::READY_FOR_RESPONSE);
	}

	SECTION("No authorized keys, invalid password") {
		auto server = NetworkAuthenticationServerHandler::Create(&invalid_password_provider, &no_authorized_key_handler);

		TestAuthentication(*server, *client, NetworkAuthenticationServerHandler::NOT_AUTHENTICATED, NetworkAuthenticationClientHandler::AWAIT_USER_INPUT);
	}

	SECTION("No authorized keys, valid password") {
		auto server = NetworkAuthenticationServerHandler::Create(&valid_password_provider, &no_authorized_key_handler);

		TestAuthentication(*server, *client, NetworkAuthenticationServerHandler::AUTHENTICATED, NetworkAuthenticationClientHandler::AWAIT_USER_INPUT);
	}

	SECTION("No authorized keys, no password") {
		auto server = NetworkAuthenticationServerHandler::Create(&no_password_provider, &no_authorized_key_handler);

		TestAuthentication(*server, *client, NetworkAuthenticationServerHandler::AUTHENTICATED, NetworkAuthenticationClientHandler::READY_FOR_RESPONSE);
	}
}


static void CheckEncryption(MockNetworkSocketHandler *sending_socket_handler, MockNetworkSocketHandler *receiving_socket_handler)
{
	PacketType sent_packet_type{ 1 };
	uint64_t sent_value = 0x1234567890ABCDEF;
	std::set<PacketType> encrypted_packet_types;

	for (int i = 0; i < 10; i++) {
		Packet request(sending_socket_handler, sent_packet_type);
		request.Send_uint64(sent_value);

		auto [response, valid] = CreatePacketForReading(request, receiving_socket_handler);
		CHECK(valid);
		CHECK(response.Recv_uint64() == sent_value);

		encrypted_packet_types.insert(request.GetPacketType());
	}
	/*
	 * Check whether it looks like encryption has happened. This is done by checking the value
	 * of the packet type after encryption. If after a few iterations more than one encrypted
	 * value has been seen, then we know that some type of encryption/scrambling is happening.
	 *
	 * Technically this check could fail erroneously when 16 subsequent encryptions yield the
	 * same encrypted packet type. However, with encryption that byte should have random value
	 * value, so the chance of this happening are tiny given enough iterations.
	 * Roughly in the order of 2**((iterations - 1) * 8), which with 10 iterations is in the
	 * one-in-sextillion (10**21) order of magnitude.
	 */
	CHECK(encrypted_packet_types.size() != 1);

}

TEST_CASE("Encryption handling")
{
	X25519KeyExchangeOnlyServerHandler server(X25519SecretKey::CreateRandom());
	X25519KeyExchangeOnlyClientHandler client(X25519SecretKey::CreateRandom());

	TestAuthentication(server, client, NetworkAuthenticationServerHandler::AUTHENTICATED, NetworkAuthenticationClientHandler::READY_FOR_RESPONSE);

	MockNetworkSocketHandler server_socket_handler(server.CreateClientToServerEncryptionHandler(), server.CreateServerToClientEncryptionHandler());
	MockNetworkSocketHandler client_socket_handler(client.CreateServerToClientEncryptionHandler(), client.CreateClientToServerEncryptionHandler());

	SECTION("Encyption happening client -> server") {
		CheckEncryption(&client_socket_handler, &server_socket_handler);
	}

	SECTION("Encyption happening server -> client") {
		CheckEncryption(&server_socket_handler, &client_socket_handler);
	}

	SECTION("Unencrypted packet sent causes invalid read packet") {
		Packet request(&mock_socket_handler, PacketType{});
		request.Send_uint64(0);

		auto [response, valid] = CreatePacketForReading(request, &client_socket_handler);
		CHECK(!valid);
	}
}
