// Monocypher version 4.0.2
//
// This file is dual-licensed.  Choose whichever licence you want from
// the two licences listed below.
//
// The first licence is a regular 2-clause BSD licence.  The second licence
// is the CC-0 from Creative Commons. It is intended to release Monocypher
// to the public domain.  The BSD licence serves as a fallback option.
//
// SPDX-License-Identifier: BSD-2-Clause OR CC0-1.0
//
// ------------------------------------------------------------------------
//
// Copyright (c) 2017-2019, Loup Vaillant
// All rights reserved.
//
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the
//    distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// ------------------------------------------------------------------------
//
// Written in 2017-2019 by Loup Vaillant
//
// To the extent possible under law, the author(s) have dedicated all copyright
// and related neighboring rights to this software to the public domain
// worldwide.  This software is distributed without any warranty.
//
// You should have received a copy of the CC0 Public Domain Dedication along
// with this software.  If not, see
// <https://creativecommons.org/publicdomain/zero/1.0/>

#ifndef ED25519_H
#define ED25519_H

#include "monocypher.h"

#ifdef MONOCYPHER_CPP_NAMESPACE
namespace MONOCYPHER_CPP_NAMESPACE {
#elif defined(__cplusplus)
extern "C" {
#endif

////////////////////////
/// Type definitions ///
////////////////////////

// Do not rely on the size or content on any of those types,
// they may change without notice.
typedef struct {
	uint64_t hash[8];
	uint64_t input[16];
	uint64_t input_size[2];
	size_t   input_idx;
} crypto_sha512_ctx;

typedef struct {
	uint8_t key[128];
	crypto_sha512_ctx ctx;
} crypto_sha512_hmac_ctx;


// SHA 512
// -------
void crypto_sha512_init  (crypto_sha512_ctx *ctx);
void crypto_sha512_update(crypto_sha512_ctx *ctx,
                          const uint8_t *message, size_t  message_size);
void crypto_sha512_final (crypto_sha512_ctx *ctx, uint8_t hash[64]);
void crypto_sha512(uint8_t hash[64],
                   const uint8_t *message, size_t message_size);

// SHA 512 HMAC
// ------------
void crypto_sha512_hmac_init(crypto_sha512_hmac_ctx *ctx,
                             const uint8_t *key, size_t key_size);
void crypto_sha512_hmac_update(crypto_sha512_hmac_ctx *ctx,
                               const uint8_t *message, size_t  message_size);
void crypto_sha512_hmac_final(crypto_sha512_hmac_ctx *ctx, uint8_t hmac[64]);
void crypto_sha512_hmac(uint8_t hmac[64],
                        const uint8_t *key    , size_t key_size,
                        const uint8_t *message, size_t message_size);

// SHA 512 HKDF
// ------------
void crypto_sha512_hkdf_expand(uint8_t       *okm,  size_t okm_size,
                               const uint8_t *prk,  size_t prk_size,
                               const uint8_t *info, size_t info_size);
void crypto_sha512_hkdf(uint8_t       *okm , size_t okm_size,
                        const uint8_t *ikm , size_t ikm_size,
                        const uint8_t *salt, size_t salt_size,
                        const uint8_t *info, size_t info_size);

// Ed25519
// -------
// Signatures (EdDSA with curve25519 + SHA-512)
// --------------------------------------------
void crypto_ed25519_key_pair(uint8_t secret_key[64],
                             uint8_t public_key[32],
                             uint8_t seed[32]);
void crypto_ed25519_sign(uint8_t        signature [64],
                         const uint8_t  secret_key[64],
                         const uint8_t *message, size_t message_size);
int crypto_ed25519_check(const uint8_t  signature [64],
                         const uint8_t  public_key[32],
                         const uint8_t *message, size_t message_size);

// Pre-hash variants
void crypto_ed25519_ph_sign(uint8_t       signature   [64],
                            const uint8_t secret_key  [64],
                            const uint8_t message_hash[64]);
int crypto_ed25519_ph_check(const uint8_t signature   [64],
                            const uint8_t public_key  [32],
                            const uint8_t message_hash[64]);

#ifdef __cplusplus
}
#endif

#endif // ED25519_H
