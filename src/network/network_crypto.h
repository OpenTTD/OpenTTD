/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file network_crypto.h Crypto specific bits of the network handling.
 *
 * This provides a set of functionality to perform authentication combined with a key exchange,
 * to create a shared secret as well as encryption using those shared secrets.
 *
 * For the authentication/key exchange, the server determines the available methods and creates
 * the appropriate \c NetworkAuthenticationServerHandler. This will be used to create a request
 * for the client, which instantiates a \c NetworkAuthenticationClientHandler to handle that
 * request.
 * At the moment there are three types of request: key exchange only, password-authenticated key
 * exchange (PAKE) and authorized keys. When the request is for a password, the user is asked
 * for the password via an essentially asynchronous callback from the client handler. For the
 * other requests no input from the user is needed, and these are immediately ready to generate
 * the response for the server.
 *
 * The server will validate the response resulting in either the user being authenticated or not.
 * When the user failed authentication, there might be a possibility to retry. For example when
 * the server has configured authorized keys and passwords; when the client fails with the
 * authorized keys, it will retry with the password.
 *
 * Once the key exchange/authentication has been done, the server can signal the client to
 * upgrade the network connection to use encryption using the shared secret of the key exchange.
 */

#ifndef NETWORK_CRYPTO_H
#define NETWORK_CRYPTO_H

#include "network_type.h"

/**
 * Base class for handling the encryption (or decryption) of a network connection.
 */
class NetworkEncryptionHandler {
public:
	virtual ~NetworkEncryptionHandler() {}

	/**
	 * Get the size of the MAC (Message Authentication Code) used by the underlying encryption protocol.
	 * @return The size, in bytes, of the MACs.
	 */
	virtual size_t MACSize() const = 0;

	/**
	 * Decrypt the given message in-place, validating against the given MAC.
	 * @param mac The message authentication code (MAC).
	 * @param message The location of the message to decrypt.
	 * @return Whether decryption and authentication/validation of the message succeeded.
	 */
	virtual bool Decrypt(std::span<std::uint8_t> mac, std::span<std::uint8_t> message) = 0;

	/**
	 * Encrypt the given message in-place, and write the associated MAC.
	 * @param mac The location to write the message authentication code (MAC) to.
	 * @param message The location of the message to encrypt.
	 */
	virtual void Encrypt(std::span<std::uint8_t> mac, std::span<std::uint8_t> message) = 0;
};


/**
 * Callback interface for requests for passwords in the context of network authentication.
 */
class NetworkAuthenticationPasswordRequest {
public:
	virtual ~NetworkAuthenticationPasswordRequest() {}

	/**
	 * Reply to the request with the given password.
	 */
	virtual void Reply(const std::string &password) = 0;
};

/**
 * Callback interface for client implementations to provide the handling of the password requests.
 */
class NetworkAuthenticationPasswordRequestHandler : public NetworkAuthenticationPasswordRequest {
protected:
	friend class X25519PAKEClientHandler;

	std::string password; ///< The entered password.
public:

	virtual void Reply(const std::string &password) override;

	/**
	 * Callback to trigger sending the response for the password request.
	 */
	virtual void SendResponse() = 0;

	/**
	 * Callback to trigger asking the user for the password.
	 * @param request The request to the user, to which it can reply with the password.
	 */
	virtual void AskUserForPassword(std::shared_ptr<NetworkAuthenticationPasswordRequest> request) = 0;
};


/**
 * Callback interface for server implementations to provide the current password.
 */
class NetworkAuthenticationPasswordProvider {
public:
	virtual ~NetworkAuthenticationPasswordProvider() {}

	/**
	 * Callback to return the password where to validate against.
	 * @return \c std::string_view of the current password; an empty view means no password check will be performed.
	 */
	virtual std::string_view GetPassword() const = 0;
};

/**
 * Default implementation of the password provider.
 */
class NetworkAuthenticationDefaultPasswordProvider : public NetworkAuthenticationPasswordProvider {
private:
	const std::string *password; ///< The password to check against.
public:
	/**
	 * Create the provider with the pointer to the password that is to be used. A pointer, so this can handle
	 * situations where the password gets changed over time.
	 * @param password The reference to the configured password.
	 */
	NetworkAuthenticationDefaultPasswordProvider(const std::string &password) : password(&password) {}

	std::string_view GetPassword() const override { return *this->password; };
};

/**
 * Callback interface for server implementations to provide the authorized key validation.
 */
class NetworkAuthenticationAuthorizedKeyHandler {
public:
	virtual ~NetworkAuthenticationAuthorizedKeyHandler() {}

	/**
	 * Check whether the key handler can be used, i.e. whether there are authorized keys to check against.
	 * @return \c true when it can be used, otherwise \c false.
	 */
	virtual bool CanBeUsed() const = 0;

	/**
	 * Check whether the given public key of the peer is allowed in.
	 * @param peer_public_key The public key of the peer to check against.
	 * @return \c true when the key is allowed, otherwise \c false.
	 */
	virtual bool IsAllowed(std::string_view peer_public_key) const = 0;
};

/**
 * Default implementation for the authorized key handler.
 */
class NetworkAuthenticationDefaultAuthorizedKeyHandler : public NetworkAuthenticationAuthorizedKeyHandler {
private:
	const NetworkAuthorizedKeys *authorized_keys; ///< The authorized keys to check against.
public:
	/**
	 * Create the handler that uses the given authorized keys to check against.
	 * @param authorized_keys The reference to the authorized keys to check against.
	 */
	NetworkAuthenticationDefaultAuthorizedKeyHandler(const NetworkAuthorizedKeys &authorized_keys) : authorized_keys(&authorized_keys) {}

	bool CanBeUsed() const override { return !this->authorized_keys->empty(); }
	bool IsAllowed(std::string_view peer_public_key) const override { return authorized_keys->Contains(peer_public_key); }
};


/** The authentication method that can be used. */
enum NetworkAuthenticationMethod : uint8_t {
	NETWORK_AUTH_METHOD_X25519_KEY_EXCHANGE_ONLY, ///< No actual authentication is taking place, just perform a x25519 key exchange.
	NETWORK_AUTH_METHOD_X25519_PAKE, ///< Authentication using x25519 password-authenticated key agreement.
	NETWORK_AUTH_METHOD_X25519_AUTHORIZED_KEY, ///< Authentication using x22519 key exchange and authorized keys.
	NETWORK_AUTH_METHOD_END, ///< Must ALWAYS be on the end of this list!! (period)
};

/** The mask of authentication methods that can be used. */
using NetworkAuthenticationMethodMask = uint16_t;

/**
 * Base class for cryptographic authentication handlers.
 */
class NetworkAuthenticationHandler {
public:
	virtual ~NetworkAuthenticationHandler() {}

	/**
	 * Get the name of the handler for debug messages.
	 * @return The name of the handler.
	 */
	virtual std::string_view GetName() const = 0;

	/**
	 * Get the method this handler is providing functionality for.
	 * @return The \c NetworkAuthenticationMethod.
	 */
	virtual NetworkAuthenticationMethod GetAuthenticationMethod() const = 0;

	/**
	 * Create a \a NetworkEncryptionHandler to encrypt or decrypt messages from the client to the server.
	 * @return The handler for the client to server encryption.
	 */
	virtual std::unique_ptr<NetworkEncryptionHandler> CreateClientToServerEncryptionHandler() const = 0;

	/**
	 * Create a \a NetworkEncryptionHandler to encrypt or decrypt messages from the server to the client.
	 * @return The handler for the server to client encryption.
	 */
	virtual std::unique_ptr<NetworkEncryptionHandler> CreateServerToClientEncryptionHandler() const = 0;
};

/**
 * Base class for client side cryptographic authentication handlers.
 */
class NetworkAuthenticationClientHandler : public NetworkAuthenticationHandler {
public:
	/** The processing result of receiving a request. */
	enum RequestResult {
		AWAIT_USER_INPUT, ///< We have requested some user input, but must wait on that.
		READY_FOR_RESPONSE, ///< We do not have to wait for user input, and can immediately respond to the server.
		INVALID, ///< We have received an invalid request.
	};

	/**
	 * Read a request from the server.
	 * @param p The packet to read the request from.
	 * @return True when valid, otherwise false.
	 */
	virtual RequestResult ReceiveRequest(struct Packet &p) = 0;

	/**
	 * Create the response to send to the server.
	 * @param p The packet to write the response from.
	 * @return True when a valid packet was made, otherwise false.
	 */
	virtual bool SendResponse(struct Packet &p) = 0;

	static std::unique_ptr<NetworkAuthenticationClientHandler> Create(std::shared_ptr<NetworkAuthenticationPasswordRequestHandler> password_handler, std::string &secret_key, std::string &public_key);
};

/**
 * Base class for server side cryptographic authentication handlers.
 */
class NetworkAuthenticationServerHandler : public NetworkAuthenticationHandler {
public:
	/** The processing result of receiving a response. */
	enum ResponseResult {
		AUTHENTICATED, ///< The client was authenticated successfully.
		NOT_AUTHENTICATED, ///< All authentications for this handler have been exhausted.
		RETRY_NEXT_METHOD, ///< The client failed to authenticate, but there is another method to try.
	};

	/**
	 * Create the request to send to the client.
	 * @param p The packet to write the request to.
	 */
	virtual void SendRequest(struct Packet &p) = 0;

	/**
	 * Read the response from the client.
	 * @param p The packet to read the response from.
	 * @return The \c ResponseResult describing the result.
	 */
	virtual ResponseResult ReceiveResponse(struct Packet &p) = 0;

	/**
	 * Checks whether this handler can be used with the current configuration.
	 * For example when there is no password, the handler cannot be used.
	 * @return True when this handler can be used.
	 */
	virtual bool CanBeUsed() const = 0;

	/**
	 * Get the public key the peer provided during the authentication.
	 * @return The hexadecimal string representation of the peer's public key.
	 */
	virtual std::string GetPeerPublicKey() const = 0;

	static std::unique_ptr<NetworkAuthenticationServerHandler> Create(const NetworkAuthenticationPasswordProvider *password_provider, const NetworkAuthenticationAuthorizedKeyHandler *authorized_key_handler, NetworkAuthenticationMethodMask client_supported_method_mask = ~static_cast<NetworkAuthenticationMethodMask>(0));
};

#endif /* NETWORK_CRYPTO_H */
