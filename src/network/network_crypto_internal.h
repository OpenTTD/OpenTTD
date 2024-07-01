/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file network_crypto_internal.h Internal bits to the crypto of the network handling. */

#ifndef NETWORK_CRYPTO_INTERNAL_H
#define NETWORK_CRYPTO_INTERNAL_H

#include "network_crypto.h"

/** The number of bytes the public and secret keys are in X25519. */
constexpr size_t X25519_KEY_SIZE = 32;
/** The number of bytes the nonces are in X25519. */
constexpr size_t X25519_NONCE_SIZE = 24;
/** The number of bytes the message authentication codes are in X25519. */
constexpr size_t X25519_MAC_SIZE = 16;
/** The number of bytes the (random) payload of the authentication message has. */
constexpr size_t X25519_KEY_EXCHANGE_MESSAGE_SIZE = 8;

/** Container for a X25519 key that is automatically crypto-wiped when destructed. */
struct X25519Key : std::array<uint8_t, X25519_KEY_SIZE> {
	~X25519Key();
};

/** Container for a X25519 public key. */
struct X25519PublicKey : X25519Key {
};

/** Container for a X25519 secret key. */
struct X25519SecretKey : X25519Key {
	static X25519SecretKey CreateRandom();
	X25519PublicKey CreatePublicKey() const;
};

/** Container for a X25519 nonce that is automatically crypto-wiped when destructed. */
struct X25519Nonce : std::array<uint8_t, X25519_NONCE_SIZE> {
	static X25519Nonce CreateRandom();
	~X25519Nonce();
};

/** Container for a X25519 message authentication code. */
using X25519Mac = std::array<uint8_t, X25519_MAC_SIZE>;

/** Container for a X25519 key exchange message. */
using X25519KeyExchangeMessage = std::array<uint8_t, X25519_KEY_EXCHANGE_MESSAGE_SIZE>;

/** The side of the key exchange. */
enum class X25519KeyExchangeSide {
	CLIENT, ///< We are the client.
	SERVER, ///< We are the server.
};

/**
 * Container for the keys that derived from the X25519 key exchange mechanism. This mechanism derives
 * a key to encrypt both the client-to-server and a key to encrypt server-to-client communication.
 */
class X25519DerivedKeys {
private:
	/** Single contiguous buffer to store the derived keys in, as they are generated as a single hash. */
	std::array<uint8_t, X25519_KEY_SIZE + X25519_KEY_SIZE> keys;
public:
	~X25519DerivedKeys();
	std::span<const uint8_t> ClientToServer() const;
	std::span<const uint8_t> ServerToClient() const;
	bool Exchange(const X25519PublicKey &peer_public_key, X25519KeyExchangeSide side,
			const X25519SecretKey &our_secret_key, const X25519PublicKey &our_public_key, std::string_view extra_payload);
};

/**
 * Base for handlers using a X25519 key exchange to perform authentication.
 *
 * In general this works as follows:
 * 1) the client and server have or generate a secret and public X25519 key.
 * 2) the X25519 key exchange is performed at both the client and server, with their own secret key and their peer's public key.
 * 3) a pair of derived keys is created by BLAKE2b-hashing the following into 64 bytes, in this particular order:
 *    - the shared secret from the key exchange;
 *    - the public key of the server;
 *    - the public key of the client;
 *    - optional extra payload, e.g. a password in the case of PAKE.
 *    The first of the pair of derived keys is usually used to encrypt client-to-server communication, and the second of the pair
 *    is usually used to encrypt server-to-client communication.
 * 4) a XChaCha20-Poly1305 (authenticated) encryption is performed using:
 *    - the first of the pair of derived keys as encryption key;
 *    - a 24 byte nonce;
 *    - the public key of the client as additional authenticated data.
 *    - a 8 byte random number as content/message.
 *
 * The server initiates the request by sending its public key and a 24 byte nonce that is randomly generated. Normally the side
 * that sends the encrypted data sends the nonce in their packet, which would be the client on our case. However, there are
 * many implementations of clients due to the admin-protocol where this is used, and we cannot guarantee that they generate a
 * good enough nonce. As such the server sends one instead. The server will create a new set of keys for each session.
 *
 * The client receives the request, performs the key exchange, generates the derived keys and then encrypts the message. This
 * message must contain some content, so it has to be filled with 8 random bytes. Once the message has been encrypted, the
 * client sends their public key, the encrypted message and the message authentication code (MAC) to the server in a response.
 *
 * The server receives the response, performs the key exchange, generates the derived keys, decrypts the message and validates the
 * message authentication code, and finally the message. It is up to the sub class to perform the final authentication checks.
 */
class X25519AuthenticationHandler {
private:
	X25519SecretKey our_secret_key; ///< The secret key used by us.
	X25519PublicKey our_public_key; ///< The public key used by us.
	X25519Nonce key_exchange_nonce; ///< The nonce to prevent replay attacks of the key exchange.
	X25519DerivedKeys derived_keys; ///< Keys derived from the authentication process.
	X25519PublicKey peer_public_key; ///< The public key used by our peer.

	X25519Nonce encryption_nonce; ///< The nonce to prevent replay attacks the encrypted connection.

protected:
	X25519AuthenticationHandler(const X25519SecretKey &secret_key);

	void SendRequest(struct Packet &p);
	bool ReceiveRequest(struct Packet &p);
	bool SendResponse(struct Packet &p, std::string_view derived_key_extra_payload);
	NetworkAuthenticationServerHandler::ResponseResult ReceiveResponse(struct Packet &p, std::string_view derived_key_extra_payload);

	std::string GetPeerPublicKey() const;

	void SendEnableEncryption(struct Packet &p) const;
	bool ReceiveEnableEncryption(struct Packet &p);
	std::unique_ptr<NetworkEncryptionHandler> CreateClientToServerEncryptionHandler() const;
	std::unique_ptr<NetworkEncryptionHandler> CreateServerToClientEncryptionHandler() const;
};

/**
 * Client side handler for using X25519 without actual authentication.
 *
 * This follows the method described in \c X25519AuthenticationHandler, without an extra payload.
 */
class X25519KeyExchangeOnlyClientHandler : protected X25519AuthenticationHandler, public NetworkAuthenticationClientHandler {
public:
	/**
	 * Create the handler that that one does the key exchange.
	 * @param secret_key The secret key to initialize this handler with.
	 */
	X25519KeyExchangeOnlyClientHandler(const X25519SecretKey &secret_key) : X25519AuthenticationHandler(secret_key) {}

	virtual RequestResult ReceiveRequest(struct Packet &p) override { return this->X25519AuthenticationHandler::ReceiveRequest(p) ? READY_FOR_RESPONSE : INVALID; }
	virtual bool SendResponse(struct Packet &p) override { return this->X25519AuthenticationHandler::SendResponse(p, {}); }

	virtual std::string_view GetName() const override { return "X25519-KeyExchangeOnly-client"; }
	virtual NetworkAuthenticationMethod GetAuthenticationMethod() const override { return NETWORK_AUTH_METHOD_X25519_KEY_EXCHANGE_ONLY; }

	virtual bool ReceiveEnableEncryption(struct Packet &p) override { return this->X25519AuthenticationHandler::ReceiveEnableEncryption(p); }
	virtual std::unique_ptr<NetworkEncryptionHandler> CreateClientToServerEncryptionHandler() const override { return this->X25519AuthenticationHandler::CreateClientToServerEncryptionHandler(); }
	virtual std::unique_ptr<NetworkEncryptionHandler> CreateServerToClientEncryptionHandler() const override { return this->X25519AuthenticationHandler::CreateServerToClientEncryptionHandler(); }
};

/**
 * Server side handler for using X25519 without actual authentication.
 *
 * This follows the method described in \c X25519AuthenticationHandler, without an extra payload.
 */
class X25519KeyExchangeOnlyServerHandler : protected X25519AuthenticationHandler, public NetworkAuthenticationServerHandler {
public:
	/**
	 * Create the handler that that one does the key exchange.
	 * @param secret_key The secret key to initialize this handler with.
	 */
	X25519KeyExchangeOnlyServerHandler(const X25519SecretKey &secret_key) : X25519AuthenticationHandler(secret_key) {}

	virtual void SendRequest(struct Packet &p) override { this->X25519AuthenticationHandler::SendRequest(p); }
	virtual ResponseResult ReceiveResponse(struct Packet &p) override { return this->X25519AuthenticationHandler::ReceiveResponse(p, {}); }

	virtual std::string_view GetName() const override { return "X25519-KeyExchangeOnly-server"; }
	virtual NetworkAuthenticationMethod GetAuthenticationMethod() const override { return NETWORK_AUTH_METHOD_X25519_KEY_EXCHANGE_ONLY; }
	virtual bool CanBeUsed() const override { return true; }

	virtual std::string GetPeerPublicKey() const override { return this->X25519AuthenticationHandler::GetPeerPublicKey(); }
	virtual void SendEnableEncryption(struct Packet &p) override { this->X25519AuthenticationHandler::SendEnableEncryption(p); }
	virtual std::unique_ptr<NetworkEncryptionHandler> CreateClientToServerEncryptionHandler() const override { return this->X25519AuthenticationHandler::CreateClientToServerEncryptionHandler(); }
	virtual std::unique_ptr<NetworkEncryptionHandler> CreateServerToClientEncryptionHandler() const override { return this->X25519AuthenticationHandler::CreateServerToClientEncryptionHandler(); }
};

/**
 * Client side handler for using X25519 with a password-authenticated key exchange.
 *
 * This follows the method described in \c X25519AuthenticationHandler, were the password is the extra payload.
 */
class X25519PAKEClientHandler : protected X25519AuthenticationHandler, public NetworkAuthenticationClientHandler {
private:
	std::shared_ptr<NetworkAuthenticationPasswordRequestHandler> handler;

public:
	/**
	 * Create the handler with the given password handler.
	 * @param secret_key The secret key to initialize this handler with.
	 * @param handler The handler requesting the password from the user, if required.
	 */
	X25519PAKEClientHandler(const X25519SecretKey &secret_key, std::shared_ptr<NetworkAuthenticationPasswordRequestHandler> handler) : X25519AuthenticationHandler(secret_key), handler(handler) {}

	virtual RequestResult ReceiveRequest(struct Packet &p) override;
	virtual bool SendResponse(struct Packet &p) override { return this->X25519AuthenticationHandler::SendResponse(p, this->handler->password); }

	virtual std::string_view GetName() const override { return "X25519-PAKE-client"; }
	virtual NetworkAuthenticationMethod GetAuthenticationMethod() const override { return NETWORK_AUTH_METHOD_X25519_PAKE; }

	virtual bool ReceiveEnableEncryption(struct Packet &p) override { return this->X25519AuthenticationHandler::ReceiveEnableEncryption(p); }
	virtual std::unique_ptr<NetworkEncryptionHandler> CreateClientToServerEncryptionHandler() const override { return this->X25519AuthenticationHandler::CreateClientToServerEncryptionHandler(); }
	virtual std::unique_ptr<NetworkEncryptionHandler> CreateServerToClientEncryptionHandler() const override { return this->X25519AuthenticationHandler::CreateServerToClientEncryptionHandler(); }
};

/**
 * Server side handler for using X25519 with a password-authenticated key exchange.
 *
 * This follows the method described in \c X25519AuthenticationHandler, were the password is the extra payload.
 */
class X25519PAKEServerHandler : protected X25519AuthenticationHandler, public NetworkAuthenticationServerHandler {
private:
	const NetworkAuthenticationPasswordProvider *password_provider; ///< The password to check against.
public:
	/**
	 * Create the handler with the given password provider.
	 * @param secret_key The secret key to initialize this handler with.
	 * @param password_provider The provider for the passwords.
	 */
	X25519PAKEServerHandler(const X25519SecretKey &secret_key, const NetworkAuthenticationPasswordProvider *password_provider) : X25519AuthenticationHandler(secret_key), password_provider(password_provider) {}

	virtual void SendRequest(struct Packet &p) override { this->X25519AuthenticationHandler::SendRequest(p); }
	virtual ResponseResult ReceiveResponse(struct Packet &p) override { return this->X25519AuthenticationHandler::ReceiveResponse(p, this->password_provider->GetPassword()); }

	virtual std::string_view GetName() const override { return "X25519-PAKE-server"; }
	virtual NetworkAuthenticationMethod GetAuthenticationMethod() const override { return NETWORK_AUTH_METHOD_X25519_PAKE; }
	virtual bool CanBeUsed() const override { return !this->password_provider->GetPassword().empty(); }

	virtual std::string GetPeerPublicKey() const override { return this->X25519AuthenticationHandler::GetPeerPublicKey(); }
	virtual void SendEnableEncryption(struct Packet &p) override { this->X25519AuthenticationHandler::SendEnableEncryption(p); }
	virtual std::unique_ptr<NetworkEncryptionHandler> CreateClientToServerEncryptionHandler() const override { return this->X25519AuthenticationHandler::CreateClientToServerEncryptionHandler(); }
	virtual std::unique_ptr<NetworkEncryptionHandler> CreateServerToClientEncryptionHandler() const override { return this->X25519AuthenticationHandler::CreateServerToClientEncryptionHandler(); }
};


/**
 * Handler for clients using a X25519 key exchange to perform authentication via a set of authorized (public) keys of clients.
 *
 * This follows the method described in \c X25519AuthenticationHandler. Once all these checks have succeeded, it will
 * check whether the public key of the client is in the list of authorized keys to login.
 */
class X25519AuthorizedKeyClientHandler : protected X25519AuthenticationHandler, public NetworkAuthenticationClientHandler {
public:
	/**
	 * Create the handler that uses the given password to check against.
	 * @param secret_key The secret key to initialize this handler with.
	 */
	X25519AuthorizedKeyClientHandler(const X25519SecretKey &secret_key) : X25519AuthenticationHandler(secret_key) {}

	virtual RequestResult ReceiveRequest(struct Packet &p) override { return this->X25519AuthenticationHandler::ReceiveRequest(p) ? READY_FOR_RESPONSE : INVALID; }
	virtual bool SendResponse(struct Packet &p) override { return this->X25519AuthenticationHandler::SendResponse(p, {}); }

	virtual std::string_view GetName() const override { return "X25519-AuthorizedKey-client"; }
	virtual NetworkAuthenticationMethod GetAuthenticationMethod() const override { return NETWORK_AUTH_METHOD_X25519_AUTHORIZED_KEY; }

	virtual bool ReceiveEnableEncryption(struct Packet &p) override { return this->X25519AuthenticationHandler::ReceiveEnableEncryption(p); }
	virtual std::unique_ptr<NetworkEncryptionHandler> CreateClientToServerEncryptionHandler() const override { return this->X25519AuthenticationHandler::CreateClientToServerEncryptionHandler(); }
	virtual std::unique_ptr<NetworkEncryptionHandler> CreateServerToClientEncryptionHandler() const override { return this->X25519AuthenticationHandler::CreateServerToClientEncryptionHandler(); }

	static X25519SecretKey GetValidSecretKeyAndUpdatePublicKey(std::string &secret_key, std::string &public_key);
};

/**
 * Handler for servers using a X25519 key exchange to perform authentication via a set of authorized (public) keys of clients.
 *
 * This follows the method described in \c X25519AuthenticationHandler. Once all these checks have succeeded, it will
 * check whether the public key of the client is in the list of authorized keys to login.
 */
class X25519AuthorizedKeyServerHandler : protected X25519AuthenticationHandler, public NetworkAuthenticationServerHandler {
private:
	const NetworkAuthenticationAuthorizedKeyHandler *authorized_key_handler; ///< The handler of the authorized keys.
public:
	/**
	 * Create the handler that uses the given authorized keys to check against.
	 * @param secret_key The secret key to initialize this handler with.
	 * @param authorized_key_handler The handler of the authorized keys.
	 */
	X25519AuthorizedKeyServerHandler(const X25519SecretKey &secret_key, const NetworkAuthenticationAuthorizedKeyHandler *authorized_key_handler) : X25519AuthenticationHandler(secret_key), authorized_key_handler(authorized_key_handler) {}

	virtual void SendRequest(struct Packet &p) override { this->X25519AuthenticationHandler::SendRequest(p); }
	virtual ResponseResult ReceiveResponse(struct Packet &p) override;

	virtual std::string_view GetName() const override { return "X25519-AuthorizedKey-server"; }
	virtual NetworkAuthenticationMethod GetAuthenticationMethod() const override { return NETWORK_AUTH_METHOD_X25519_AUTHORIZED_KEY; }
	virtual bool CanBeUsed() const override { return this->authorized_key_handler->CanBeUsed(); }

	virtual std::string GetPeerPublicKey() const override { return this->X25519AuthenticationHandler::GetPeerPublicKey(); }
	virtual void SendEnableEncryption(struct Packet &p) override { this->X25519AuthenticationHandler::SendEnableEncryption(p); }
	virtual std::unique_ptr<NetworkEncryptionHandler> CreateClientToServerEncryptionHandler() const override { return this->X25519AuthenticationHandler::CreateClientToServerEncryptionHandler(); }
	virtual std::unique_ptr<NetworkEncryptionHandler> CreateServerToClientEncryptionHandler() const override { return this->X25519AuthenticationHandler::CreateServerToClientEncryptionHandler(); }
};


/**
 * Handler for combining a number of authentication handlers, where the failure of one of the handlers will retry with
 * another handler. For example when authorized keys fail, it can still fall back to a password.
 */
class CombinedAuthenticationClientHandler : public NetworkAuthenticationClientHandler {
public:
	using Handler = std::unique_ptr<NetworkAuthenticationClientHandler>; ///< The type of the inner handlers.

private:
	std::vector<Handler> handlers; ///< The handlers that we can authenticate with.
	NetworkAuthenticationClientHandler *current_handler = nullptr; ///< The currently active handler.

public:
	/**
	 * Add the given sub-handler to this handler.
	 * @param handler The handler to add.
	 */
	void Add(Handler &&handler) { this->handlers.push_back(std::move(handler)); }

	virtual RequestResult ReceiveRequest(struct Packet &p) override;
	virtual bool SendResponse(struct Packet &p) override;

	virtual std::string_view GetName() const override;
	virtual NetworkAuthenticationMethod GetAuthenticationMethod() const override;

	virtual bool ReceiveEnableEncryption(struct Packet &p) override { return this->current_handler->ReceiveEnableEncryption(p); }
	virtual std::unique_ptr<NetworkEncryptionHandler> CreateClientToServerEncryptionHandler() const override { return this->current_handler->CreateClientToServerEncryptionHandler(); }
	virtual std::unique_ptr<NetworkEncryptionHandler> CreateServerToClientEncryptionHandler() const override { return this->current_handler->CreateServerToClientEncryptionHandler(); }
};

/**
 * Handler for combining a number of authentication handlers, where the failure of one of the handlers will retry with
 * another handler. For example when authorized keys fail, it can still fall back to a password.
 */
class CombinedAuthenticationServerHandler : public NetworkAuthenticationServerHandler {
public:
	using Handler = std::unique_ptr<NetworkAuthenticationServerHandler>; ///< The type of the inner handlers.

private:
	std::vector<Handler> handlers; ///< The handlers that we can (still) authenticate with.

public:
	void Add(Handler &&handler);

	virtual void SendRequest(struct Packet &p) override;
	virtual ResponseResult ReceiveResponse(struct Packet &p) override;

	virtual std::string_view GetName() const override;
	virtual NetworkAuthenticationMethod GetAuthenticationMethod() const override;
	virtual bool CanBeUsed() const override;

	virtual std::string GetPeerPublicKey() const override { return this->handlers.back()->GetPeerPublicKey(); }
	virtual void SendEnableEncryption(struct Packet &p) override { this->handlers.back()->SendEnableEncryption(p); }
	virtual std::unique_ptr<NetworkEncryptionHandler> CreateClientToServerEncryptionHandler() const override { return this->handlers.back()->CreateClientToServerEncryptionHandler(); }
	virtual std::unique_ptr<NetworkEncryptionHandler> CreateServerToClientEncryptionHandler() const override { return this->handlers.back()->CreateServerToClientEncryptionHandler(); }
};

#endif /* NETWORK_CRYPTO_INTERNAL_H */
