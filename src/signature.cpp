/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file signature.cpp Implementation of signature validation routines. */

#include "stdafx.h"

#include "signature.h"

#include "debug.h"
#include "fileio_func.h"
#include "string_func.h"

#include "3rdparty/monocypher/monocypher.h"
#include "3rdparty/monocypher/monocypher-ed25519.h"
#include "3rdparty/nlohmann/json.hpp"

#include "safeguards.h"

/** The public keys used for signature validation. */
static const std::initializer_list<std::array<uint8_t, 32>> _public_keys_v1 = {
	/* 2024-01-20 - Public key for Social Integration Plugins. */
	{ 0xed, 0x5d, 0x57, 0x47, 0x21, 0x99, 0x8b, 0x02, 0xdf, 0x6e, 0x3d, 0x69, 0xe1, 0x87, 0xca, 0xd0, 0x0e, 0x88, 0xc3, 0xe2, 0xb2, 0xa6, 0x7b, 0xc0, 0x42, 0xc8, 0xd6, 0x4b, 0x65, 0xe6, 0x48, 0xf7 },
};

/**
 * Calculate the 32-byte blake2b hash of a file.
 *
 * @param filename The filename to calculate the hash of.
 * @return The 32-byte blake2b hash of the file, hex-encoded.
 */
static std::string CalculateHashV1(const std::string &filename)
{
	FILE *f = FioFOpenFile(filename, "rb", NO_DIRECTORY);
	if (f == nullptr) {
		return "";
	}

	std::array<uint8_t, 32> digest;
	crypto_blake2b_ctx ctx;
	crypto_blake2b_init(&ctx, digest.size());

	while (!feof(f)) {
		std::array<uint8_t, 1024> buf;
		size_t len = fread(buf.data(), 1, buf.size(), f);

		crypto_blake2b_update(&ctx, buf.data(), len);
	}
	fclose(f);

	crypto_blake2b_final(&ctx, digest.data());
	return FormatArrayAsHex(digest);
}

/**
 * Validate whether the checksum of a file is the same.
 *
 * @param filename The filename to validate the checksum of.
 * @param checksum The expected checksum.
 * @return True iff the checksum of the file is the same as the expected checksum.
 */
static bool ValidateChecksum(const std::string &filename, const std::string &checksum)
{
	/* Checksums are "<version>$<hash>". Split out the version. */
	auto pos = checksum.find('$');
	assert(pos != std::string::npos); // Already validated by ValidateSchema().
	const std::string version = checksum.substr(0, pos);
	const std::string hash = checksum.substr(pos + 1);

	/* Calculate the checksum over the file. */
	std::string calculated_hash;
	if (version == "1") {
		calculated_hash = CalculateHashV1(filename);
	} else {
		Debug(misc, 0, "Failed to validate signature: unknown checksum version: {}", filename);
		return false;
	}

	/* Validate the checksum is the same. */
	if (calculated_hash.empty()) {
		Debug(misc, 0, "Failed to validate signature: couldn't calculate checksum for: {}", filename);
		return false;
	}
	if (calculated_hash != hash) {
		Debug(misc, 0, "Failed to validate signature: checksum mismatch for: {}", filename);
		return false;
	}

	return true;
}

/**
 * Validate whether the signature is valid for this set of files.
 *
 * @param signature The signature to validate.
 * @param files The files to validate the signature against.
 * @param filename The filename of the signatures file (for error-reporting).
 * @return True iff the signature is valid for this set of files.
 */
static bool ValidateSignature(const std::string &signature, const nlohmann::json &files, const std::string &filename)
{
	/* Signatures are "<version>$<signature>". Split out the version. */
	auto pos = signature.find('$');
	assert(pos != std::string::npos); // Already validated by ValidateSchema().
	const std::string version = signature.substr(0, pos);
	const std::string sig_value = signature.substr(pos + 1);

	/* Create the message we are going to validate. */
	std::string message = files.dump(-1);

	/* Validate the signature. */
	if (version == "1") {
		std::array<uint8_t, 64> sig;
		if (sig_value.size() != 128 || !ConvertHexToBytes(sig_value, sig)) {
			Debug(misc, 0, "Failed to validate signature: invalid signature: {}", filename);
			return false;
		}

		for (auto &pk_value : _public_keys_v1) {
			/* Check if the message is valid with this public key. */
			auto res = crypto_ed25519_check(sig.data(), pk_value.data(), reinterpret_cast<uint8_t *>(message.data()), message.size());
			if (res == 0) {
				return true;
			}
		}

		Debug(misc, 0, "Failed to validate signature: signature validation failed: {}", filename);
		return false;
	} else {
		Debug(misc, 0, "Failed to validate signature: unknown signature version: {}", filename);
		return false;
	}

	return true;
}

/**
 * Validate the signatures file complies with the JSON schema.
 *
 * @param signatures The signatures JSON to validate.
 * @param filename The filename of the signatures file (for error-reporting).
 * @return True iff the signatures file complies with the JSON schema.
 */
static bool ValidateSchema(const nlohmann::json &signatures, const std::string &filename)
{
	if (signatures["files"].is_null()) {
		Debug(misc, 0, "Failed to validate signature: no files found: {}", filename);
		return false;
	}

	if (signatures["signature"].is_null()) {
		Debug(misc, 0, "Failed to validate signature: no signature found: {}", filename);
		return false;
	}

	for (auto &signature : signatures["files"]) {
		if (signature["filename"].is_null() || signature["checksum"].is_null()) {
			Debug(misc, 0, "Failed to validate signature: invalid entry in files: {}", filename);
			return false;
		}

		const std::string sig_filename = signature["filename"];
		const std::string sig_checksum = signature["checksum"];

		if (sig_filename.empty() || sig_checksum.empty()) {
			Debug(misc, 0, "Failed to validate signature: invalid entry in files: {}", filename);
			return false;
		}

		auto pos = sig_checksum.find('$');
		if (pos == std::string::npos) {
			Debug(misc, 0, "Failed to validate signature: invalid checksum format: {}", filename);
			return false;
		}
	}

	const std::string signature = signatures["signature"];
	auto pos = signature.find('$');
	if (pos == std::string::npos) {
		Debug(misc, 0, "Failed to validate signature: invalid signature format: {}", filename);
		return false;
	}

	return true;
}

/**
 * Validate that the signatures mentioned in the signature file are matching
 * the files in question.
 *
 * @return True iff the files in the signature file passed validation.
 */
static bool _ValidateSignatureFile(const std::string &filename)
{
	size_t filesize;
	FILE *f = FioFOpenFile(filename, "rb", NO_DIRECTORY, &filesize);
	if (f == nullptr) {
		Debug(misc, 0, "Failed to validate signature: file not found: {}", filename);
		return false;
	}

	std::string text(filesize, '\0');
	size_t len = fread(text.data(), filesize, 1, f);
	FioFCloseFile(f);
	if (len != 1) {
		Debug(misc, 0, "Failed to validate signature: failed to read file: {}", filename);
		return false;
	}

	nlohmann::json signatures;
	try {
		signatures = nlohmann::json::parse(text);
	} catch (nlohmann::json::exception &) {
		Debug(misc, 0, "Failed to validate signature: not a valid JSON file: {}", filename);
		return false;
	}

	/*
	 * The JSON file should look like:
	 *
	 *   {
	 *     "files": [
	 *       {
	 *         "checksum": "version$hash"
	 *         "filename": "filename",
	 *       },
	 *       ...
	 *     ],
	 *     "signature": "version$signature"
	 *   }
	 *
	 * The signature is a signed message of the content of "files", dumped as
	 * JSON without spaces / newlines, keys in the order as indicated above.
	 */

	if (!ValidateSchema(signatures, filename)) {
		return false;
	}

	if (!ValidateSignature(signatures["signature"], signatures["files"], filename)) {
		return false;
	}

	std::string dirname = FS2OTTD(std::filesystem::path(OTTD2FS(filename)).parent_path());

	for (auto &signature : signatures["files"]) {
		const std::string sig_filename = dirname + PATHSEPCHAR + signature["filename"].get<std::string>();
		const std::string sig_checksum = signature["checksum"];

		if (!ValidateChecksum(sig_filename, sig_checksum)) {
			return false;
		}
	}

	return true;
}

/**
 * Validate that the signatures mentioned in the signature file are matching
 * the files in question.
 *
 * @note if ALLOW_INVALID_SIGNATURE is defined, this function will always
 * return true (but will still report any errors in the console).
 *
 * @return True iff the files in the signature file passed validation.
 */
bool ValidateSignatureFile(const std::string &filename)
{
	auto res = _ValidateSignatureFile(filename);;
#if defined(ALLOW_INVALID_SIGNATURE)
	(void)res; // Ignore the result.
	return true;
#else
	return res;
#endif
}
