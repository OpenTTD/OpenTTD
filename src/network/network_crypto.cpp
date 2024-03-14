/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file network_crypto.cpp Implementation of the network specific cryptography helpers. */

#include "../stdafx.h"

#include "network_crypto_internal.h"
#include "core/packet.h"

#include "../3rdparty/monocypher/monocypher.h"
#include "../core/random_func.hpp"
#include "../debug.h"
#include "../string_func.h"

#include "../safeguards.h"

/**
 * Call \c crypto_wipe for all the data in the given span.
 * @param span The span to cryptographically wipe.
 */
static void crypto_wipe(std::span<uint8_t> span)
{
	crypto_wipe(span.data(), span.size());
}

/** Ensure the derived keys do not get leaked when we're done with it. */
X25519DerivedKeys::~X25519DerivedKeys()
{
	crypto_wipe(keys);
}

/**
 * Get the key to encrypt or decrypt a message sent from the client to the server.
 * @return The raw bytes of the key.
 */
std::span<const uint8_t> X25519DerivedKeys::ClientToServer() const
{
	return std::span(this->keys.data(), X25519_KEY_SIZE);
}

/**
 * Get the key to encrypt or decrypt a message sent from the server to the client.
 * @return The raw bytes of the key.
 */
std::span<const uint8_t> X25519DerivedKeys::ServerToClient() const
{
	return std::span(this->keys.data() + X25519_KEY_SIZE, X25519_KEY_SIZE);
}

/**
 * Perform the actual key exchange.
 * @param peer_public_key The public key chosen by the other participant of the key exchange.
 * @param side Whether we are the client or server; used to hash the public key of us and the peer in the right order.
 * @param our_secret_key The secret key of us.
 * @param our_public_key The public key of us.
 * @param extra_payload Extra payload to put into the hash function to create the derived keys.
 * @return True when the key exchange has succeeded, false when an illegal public key was given.
 */
bool X25519DerivedKeys::Exchange(const X25519PublicKey &peer_public_key, X25519KeyExchangeSide side,
		const X25519SecretKey &our_secret_key, const X25519PublicKey &our_public_key, std::string_view extra_payload)
{
	X25519Key shared_secret;
	crypto_x25519(shared_secret.data(), our_secret_key.data(), peer_public_key.data());
	if (std::all_of(shared_secret.begin(), shared_secret.end(), [](auto v) { return v == 0; })) {
		/* A shared secret of all zeros means that the peer tried to force the shared secret to a known constant. */
		return false;
	}

	crypto_blake2b_ctx ctx;
	crypto_blake2b_init(&ctx, this->keys.size());
	crypto_blake2b_update(&ctx, shared_secret.data(), shared_secret.size());
	switch (side) {
		case X25519KeyExchangeSide::SERVER:
			crypto_blake2b_update(&ctx, our_public_key.data(), our_public_key.size());
			crypto_blake2b_update(&ctx, peer_public_key.data(), peer_public_key.size());
			break;
		case X25519KeyExchangeSide::CLIENT:
			crypto_blake2b_update(&ctx, peer_public_key.data(), peer_public_key.size());
			crypto_blake2b_update(&ctx, our_public_key.data(), our_public_key.size());
			break;
		default:
			NOT_REACHED();
	}
	crypto_blake2b_update(&ctx, reinterpret_cast<const uint8_t *>(extra_payload.data()), extra_payload.size());
	crypto_blake2b_final(&ctx, this->keys.data());
	return true;
}

/**
 * Encryption handler implementation for monocypther encryption after a X25519 key exchange.
 */
class X25519EncryptionHandler : public NetworkEncryptionHandler {
private:
	crypto_aead_ctx context; ///< The actual encryption context.

public:
	/**
	 * Create the encryption handler.
	 * @param key The key used for the encryption.
	 * @param nonce The nonce used for the encryption.
	 */
	X25519EncryptionHandler(const std::span<const uint8_t> key, const X25519Nonce &nonce)
	{
		assert(key.size() == X25519_KEY_SIZE);
		crypto_aead_init_x(&this->context, key.data(), nonce.data());
	}

	/** Ensure the encryption context is wiped! */
	~X25519EncryptionHandler()
	{
		crypto_wipe(&this->context, sizeof(this->context));
	}

	size_t MACSize() const override
	{
		return X25519_MAC_SIZE;
	}

	bool Decrypt(std::span<std::uint8_t> mac, std::span<std::uint8_t> message) override
	{
		return crypto_aead_read(&this->context, message.data(), mac.data(), nullptr, 0, message.data(), message.size()) == 0;
	}

	void Encrypt(std::span<std::uint8_t> mac, std::span<std::uint8_t> message) override
	{
		crypto_aead_write(&this->context, message.data(), mac.data(), nullptr, 0, message.data(), message.size());
	}
};

/** Ensure the key does not get leaked when we're done with it. */
X25519Key::~X25519Key()
{
	crypto_wipe(*this);
}

/**
 * Create a new secret key that's filled with random bytes.
 * @return The randomly filled key.
 */
/* static */ X25519SecretKey X25519SecretKey::CreateRandom()
{
	X25519SecretKey secret_key;
	RandomBytesWithFallback(secret_key);
	return secret_key;
}

/**
 * Create the public key associated with this secret key.
 * @return The public key.
 */
X25519PublicKey X25519SecretKey::CreatePublicKey() const
{
	X25519PublicKey public_key;
	crypto_x25519_public_key(public_key.data(), this->data());
	return public_key;
}

/**
 * Create a new nonce that's filled with random bytes.
 * @return The randomly filled nonce.
 */
/* static */ X25519Nonce X25519Nonce::CreateRandom()
{
	X25519Nonce nonce;
	RandomBytesWithFallback(nonce);
	return nonce;
}

/** Ensure the nonce does not get leaked when we're done with it. */
X25519Nonce::~X25519Nonce()
{
	crypto_wipe(*this);
}

/**
 * Create the handler, and generate the public keys accordingly.
 * @param secret_key The secret key to use for this handler. Defaults to secure random data.
 */
X25519AuthenticationHandler::X25519AuthenticationHandler(const X25519SecretKey &secret_key) :
	our_secret_key(secret_key), our_public_key(secret_key.CreatePublicKey()), nonce(X25519Nonce::CreateRandom())
{
}

/* virtual */ void X25519AuthenticationHandler::SendRequest(Packet &p)
{
	p.Send_bytes(this->our_public_key);
	p.Send_bytes(this->nonce);
}

/**
 * Read the key exchange data from a \c Packet that came from the server,
 * @param p The packet that has been received.
 * @return True when the data seems correct.
 */
bool X25519AuthenticationHandler::ReceiveRequest(Packet &p)
{
	if (p.RemainingBytesToTransfer() != X25519_KEY_SIZE + X25519_NONCE_SIZE) {
		Debug(net, 1, "[crypto] Received auth response of illegal size; authentication aborted.");
		return false;
	}

	p.Recv_bytes(this->peer_public_key);
	p.Recv_bytes(this->nonce);
	return true;
}

/**
 * Perform the key exchange, and when that is correct fill the \c Packet with the appropriate data.
 * @param p The packet that has to be sent.
 * @param derived_key_extra_payload The extra payload to pass to the key exchange.
 * @return Whether the key exchange was successful or not.
 */
bool X25519AuthenticationHandler::SendResponse(Packet &p, std::string_view derived_key_extra_payload)
{
	if (!this->derived_keys.Exchange(this->peer_public_key, X25519KeyExchangeSide::CLIENT,
			this->our_secret_key, this->our_public_key, derived_key_extra_payload)) {
		Debug(net, 0, "[crypto] Peer sent an illegal public key; authentication aborted.");
		return false;
	}

	X25519KeyExchangeMessage message;
	RandomBytesWithFallback(message);
	X25519Mac mac;

	crypto_aead_lock(message.data(), mac.data(), this->derived_keys.ClientToServer().data(), nonce.data(),
			this->our_public_key.data(), this->our_public_key.size(), message.data(), message.size());

	p.Send_bytes(this->our_public_key);
	p.Send_bytes(mac);
	p.Send_bytes(message);
	return true;
}

/**
 * Get the public key the peer provided for the key exchange.
 * @return The hexadecimal string representation of the peer's public key.
 */
std::string X25519AuthenticationHandler::GetPeerPublicKey() const
{
	return FormatArrayAsHex(this->peer_public_key);
}

std::unique_ptr<NetworkEncryptionHandler> X25519AuthenticationHandler::CreateClientToServerEncryptionHandler() const
{
	return std::make_unique<X25519EncryptionHandler>(this->derived_keys.ClientToServer(), this->nonce);
}

std::unique_ptr<NetworkEncryptionHandler> X25519AuthenticationHandler::CreateServerToClientEncryptionHandler() const
{
	return std::make_unique<X25519EncryptionHandler>(this->derived_keys.ServerToClient(), this->nonce);
}

/**
 * Read the key exchange data from a \c Packet that came from the client, and check whether the client passes the key
 * exchange successfully.
 * @param p The packet that has been received.
 * @param derived_key_extra_payload The extra payload to pass to the key exchange.
 * @return Whether the authentication was successful or not.
 */
NetworkAuthenticationServerHandler::ResponseResult X25519AuthenticationHandler::ReceiveResponse(Packet &p, std::string_view derived_key_extra_payload)
{
	if (p.RemainingBytesToTransfer() != X25519_KEY_SIZE + X25519_MAC_SIZE + X25519_KEY_EXCHANGE_MESSAGE_SIZE) {
		Debug(net, 1, "[crypto] Received auth response of illegal size; authentication aborted.");
		return NetworkAuthenticationServerHandler::NOT_AUTHENTICATED;
	}

	X25519KeyExchangeMessage message{};
	X25519Mac mac;

	p.Recv_bytes(this->peer_public_key);
	p.Recv_bytes(mac);
	p.Recv_bytes(message);

	if (!this->derived_keys.Exchange(this->peer_public_key, X25519KeyExchangeSide::SERVER,
			this->our_secret_key, this->our_public_key, derived_key_extra_payload)) {
		Debug(net, 0, "[crypto] Peer sent an illegal public key; authentication aborted.");
		return NetworkAuthenticationServerHandler::NOT_AUTHENTICATED;
	}

	if (crypto_aead_unlock(message.data(), mac.data(), this->derived_keys.ClientToServer().data(), nonce.data(),
			this->peer_public_key.data(), this->peer_public_key.size(), message.data(), message.size()) != 0) {
		/*
		 * The ciphertext and the message authentication code do not match with the encryption key.
		 * This is most likely an invalid password, or possibly a bug in the client.
		 */
		return NetworkAuthenticationServerHandler::NOT_AUTHENTICATED;
	}

	return NetworkAuthenticationServerHandler::AUTHENTICATED;
}


/* virtual */ NetworkAuthenticationClientHandler::RequestResult X25519PAKEClientHandler::ReceiveRequest(struct Packet &p)
{
	bool success = this->X25519AuthenticationHandler::ReceiveRequest(p);
	if (!success) return NetworkAuthenticationClientHandler::INVALID;

	this->handler->AskUserForPassword(this->handler);
	return NetworkAuthenticationClientHandler::AWAIT_USER_INPUT;
}

/**
 * Get the secret key from the given string. If that is not a valid secret key, reset it with a random one.
 * Furthermore update the public key so it is always in sync with the private key.
 * @param secret_key The secret key to read/validate/fix.
 * @param public_key The public key to update.
 * @return The valid secret key.
 */
/* static */ X25519SecretKey X25519AuthorizedKeyClientHandler::GetValidSecretKeyAndUpdatePublicKey(std::string &secret_key, std::string &public_key)
{
	X25519SecretKey key{};
	if (!ConvertHexToBytes(secret_key, key)) {
		if (secret_key.empty()) {
			Debug(net, 3, "[crypto] Creating a new random key");
		} else {
			Debug(net, 0, "[crypto] Found invalid secret key, creating a new random key");
		}
		key = X25519SecretKey::CreateRandom();
		secret_key = FormatArrayAsHex(key);
	}

	public_key = FormatArrayAsHex(key.CreatePublicKey());
	return key;
}

/* virtual */ NetworkAuthenticationServerHandler::ResponseResult X25519AuthorizedKeyServerHandler::ReceiveResponse(Packet &p)
{
	ResponseResult result = this->X25519AuthenticationHandler::ReceiveResponse(p, {});
	if (result != AUTHENTICATED) return result;

	std::string peer_public_key = this->GetPeerPublicKey();
	return this->authorized_key_handler->IsAllowed(peer_public_key) ? AUTHENTICATED : NOT_AUTHENTICATED;
}


/* virtual */ NetworkAuthenticationClientHandler::RequestResult CombinedAuthenticationClientHandler::ReceiveRequest(struct Packet &p)
{
	NetworkAuthenticationMethod method = static_cast<NetworkAuthenticationMethod>(p.Recv_uint8());

	auto is_of_method = [method](Handler &handler) { return handler->GetAuthenticationMethod() == method; };
	auto it = std::find_if(handlers.begin(), handlers.end(), is_of_method);
	if (it == handlers.end()) return INVALID;

	this->current_handler = it->get();

	Debug(net, 9, "Received {} authentication request", this->GetName());
	return this->current_handler->ReceiveRequest(p);
}

/* virtual */ bool CombinedAuthenticationClientHandler::SendResponse(struct Packet &p)
{
	Debug(net, 9, "Sending {} authentication response", this->GetName());

	return this->current_handler->SendResponse(p);
}

/* virtual */ std::string_view CombinedAuthenticationClientHandler::GetName() const
{
	return this->current_handler != nullptr ? this->current_handler->GetName() : "Unknown";
}

/* virtual */ NetworkAuthenticationMethod CombinedAuthenticationClientHandler::GetAuthenticationMethod() const
{
	return this->current_handler != nullptr ? this->current_handler->GetAuthenticationMethod() : NETWORK_AUTH_METHOD_END;
}


/**
 * Add the given sub-handler to this handler, if the handler can be used (e.g. there are authorized keys or there is a password).
 * @param handler The handler to add.
 */
void CombinedAuthenticationServerHandler::Add(CombinedAuthenticationServerHandler::Handler &&handler)
{
	/* Is the handler configured correctly, e.g. does it have a password? */
	if (!handler->CanBeUsed()) return;

	this->handlers.push_back(std::move(handler));
}

/* virtual */ void CombinedAuthenticationServerHandler::SendRequest(struct Packet &p)
{
	Debug(net, 9, "Sending {} authentication request", this->GetName());

	p.Send_uint8(this->handlers.back()->GetAuthenticationMethod());
	this->handlers.back()->SendRequest(p);
}

/* virtual */ NetworkAuthenticationServerHandler::ResponseResult CombinedAuthenticationServerHandler::ReceiveResponse(struct Packet &p)
{
	Debug(net, 9, "Receiving {} authentication response", this->GetName());

	ResponseResult result = this->handlers.back()->ReceiveResponse(p);
	if (result != NOT_AUTHENTICATED) return result;

	this->handlers.pop_back();
	return this->CanBeUsed() ? RETRY_NEXT_METHOD : NOT_AUTHENTICATED;
}

/* virtual */ std::string_view CombinedAuthenticationServerHandler::GetName() const
{
	return this->CanBeUsed() ? this->handlers.back()->GetName() : "Unknown";
}

/* virtual */ NetworkAuthenticationMethod CombinedAuthenticationServerHandler::GetAuthenticationMethod() const
{
	return this->CanBeUsed() ? this->handlers.back()->GetAuthenticationMethod() : NETWORK_AUTH_METHOD_END;
}

/* virtual */ bool CombinedAuthenticationServerHandler::CanBeUsed() const
{
	return !this->handlers.empty();
}


/* virtual */ void NetworkAuthenticationPasswordRequestHandler::Reply(const std::string &password)
{
	this->password = password;
	this->SendResponse();
}

/* virtual */ bool NetworkAuthenticationDefaultAuthorizedKeyHandler::IsAllowed(std::string_view peer_public_key) const
{
	for (const auto &allowed : *this->authorized_keys) {
		if (StrEqualsIgnoreCase(allowed, peer_public_key)) return true;
	}
	return false;
}


/**
 * Create a NetworkAuthenticationClientHandler.
 * @param password_handler The handler for when a request for password needs to be passed on to the user.
 * @param secret_key The location where the secret key is stored; can be overwritten when invalid.
 * @param public_key The location where the public key is stored; can be overwritten when invalid.
 */
/* static */ std::unique_ptr<NetworkAuthenticationClientHandler> NetworkAuthenticationClientHandler::Create(std::shared_ptr<NetworkAuthenticationPasswordRequestHandler> password_handler, std::string &secret_key, std::string &public_key)
{
	auto secret = X25519AuthorizedKeyClientHandler::GetValidSecretKeyAndUpdatePublicKey(secret_key, public_key);
	auto handler = std::make_unique<CombinedAuthenticationClientHandler>();
	handler->Add(std::make_unique<X25519KeyExchangeOnlyClientHandler>(secret));
	handler->Add(std::make_unique<X25519PAKEClientHandler>(secret, std::move(password_handler)));
	handler->Add(std::make_unique<X25519AuthorizedKeyClientHandler>(secret));
	return handler;
}

/**
 * Create a NetworkAuthenticationServerHandler.
 * @param password_provider Callback to provide the password handling. Must remain valid until the authentication has succeeded or failed. Can be \c nullptr to skip password checks.
 * @param authorized_key_handler Callback to provide the authorized key handling. Must remain valid until the authentication has succeeded or failed. Can be \c nullptr to skip authorized key checks.
 * @param client_supported_method_mask Bitmask of the methods that are supported by the client. Defaults to support of all methods.
 */
std::unique_ptr<NetworkAuthenticationServerHandler> NetworkAuthenticationServerHandler::Create(const NetworkAuthenticationPasswordProvider *password_provider, const NetworkAuthenticationAuthorizedKeyHandler *authorized_key_handler, NetworkAuthenticationMethodMask client_supported_method_mask)
{
	auto secret = X25519SecretKey::CreateRandom();
	auto handler = std::make_unique<CombinedAuthenticationServerHandler>();
	if (password_provider != nullptr && HasBit(client_supported_method_mask, NETWORK_AUTH_METHOD_X25519_PAKE)) {
		handler->Add(std::make_unique<X25519PAKEServerHandler>(secret, password_provider));
	}

	if (authorized_key_handler != nullptr && HasBit(client_supported_method_mask, NETWORK_AUTH_METHOD_X25519_AUTHORIZED_KEY)) {
		handler->Add(std::make_unique<X25519AuthorizedKeyServerHandler>(secret, authorized_key_handler));
	}

	if (!handler->CanBeUsed() && HasBit(client_supported_method_mask, NETWORK_AUTH_METHOD_X25519_KEY_EXCHANGE_ONLY)) {
		/* Fall back to the plain handler when neither password, nor authorized keys are configured. */
		handler->Add(std::make_unique<X25519KeyExchangeOnlyServerHandler>(secret));
	}
	return handler;
}
