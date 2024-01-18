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

#include "monocypher-ed25519.h"

#ifdef MONOCYPHER_CPP_NAMESPACE
namespace MONOCYPHER_CPP_NAMESPACE {
#endif

/////////////////
/// Utilities ///
/////////////////
#define FOR(i, min, max)     for (size_t i = min; i < max; i++)
#define COPY(dst, src, size) FOR(_i_, 0, size) (dst)[_i_] = (src)[_i_]
#define ZERO(buf, size)      FOR(_i_, 0, size) (buf)[_i_] = 0
#define WIPE_CTX(ctx)        crypto_wipe(ctx   , sizeof(*(ctx)))
#define WIPE_BUFFER(buffer)  crypto_wipe(buffer, sizeof(buffer))
#define MIN(a, b)            ((a) <= (b) ? (a) : (b))
typedef uint8_t u8;
typedef uint64_t u64;

// Returns the smallest positive integer y such that
// (x + y) % pow_2  == 0
// Basically, it's how many bytes we need to add to "align" x.
// Only works when pow_2 is a power of 2.
// Note: we use ~x+1 instead of -x to avoid compiler warnings
static size_t align(size_t x, size_t pow_2)
{
	return (~x + 1) & (pow_2 - 1);
}

static u64 load64_be(const u8 s[8])
{
	return((u64)s[0] << 56)
		| ((u64)s[1] << 48)
		| ((u64)s[2] << 40)
		| ((u64)s[3] << 32)
		| ((u64)s[4] << 24)
		| ((u64)s[5] << 16)
		| ((u64)s[6] <<  8)
		|  (u64)s[7];
}

static void store64_be(u8 out[8], u64 in)
{
	out[0] = (in >> 56) & 0xff;
	out[1] = (in >> 48) & 0xff;
	out[2] = (in >> 40) & 0xff;
	out[3] = (in >> 32) & 0xff;
	out[4] = (in >> 24) & 0xff;
	out[5] = (in >> 16) & 0xff;
	out[6] = (in >>  8) & 0xff;
	out[7] =  in        & 0xff;
}

static void load64_be_buf (u64 *dst, const u8 *src, size_t size) {
	FOR(i, 0, size) { dst[i] = load64_be(src + i*8); }
}

///////////////
/// SHA 512 ///
///////////////
static u64 rot(u64 x, int c       ) { return (x >> c) | (x << (64 - c));   }
static u64 ch (u64 x, u64 y, u64 z) { return (x & y) ^ (~x & z);           }
static u64 maj(u64 x, u64 y, u64 z) { return (x & y) ^ ( x & z) ^ (y & z); }
static u64 big_sigma0(u64 x) { return rot(x, 28) ^ rot(x, 34) ^ rot(x, 39); }
static u64 big_sigma1(u64 x) { return rot(x, 14) ^ rot(x, 18) ^ rot(x, 41); }
static u64 lit_sigma0(u64 x) { return rot(x,  1) ^ rot(x,  8) ^ (x >> 7);   }
static u64 lit_sigma1(u64 x) { return rot(x, 19) ^ rot(x, 61) ^ (x >> 6);   }

static const u64 K[80] = {
	0x428a2f98d728ae22,0x7137449123ef65cd,0xb5c0fbcfec4d3b2f,0xe9b5dba58189dbbc,
	0x3956c25bf348b538,0x59f111f1b605d019,0x923f82a4af194f9b,0xab1c5ed5da6d8118,
	0xd807aa98a3030242,0x12835b0145706fbe,0x243185be4ee4b28c,0x550c7dc3d5ffb4e2,
	0x72be5d74f27b896f,0x80deb1fe3b1696b1,0x9bdc06a725c71235,0xc19bf174cf692694,
	0xe49b69c19ef14ad2,0xefbe4786384f25e3,0x0fc19dc68b8cd5b5,0x240ca1cc77ac9c65,
	0x2de92c6f592b0275,0x4a7484aa6ea6e483,0x5cb0a9dcbd41fbd4,0x76f988da831153b5,
	0x983e5152ee66dfab,0xa831c66d2db43210,0xb00327c898fb213f,0xbf597fc7beef0ee4,
	0xc6e00bf33da88fc2,0xd5a79147930aa725,0x06ca6351e003826f,0x142929670a0e6e70,
	0x27b70a8546d22ffc,0x2e1b21385c26c926,0x4d2c6dfc5ac42aed,0x53380d139d95b3df,
	0x650a73548baf63de,0x766a0abb3c77b2a8,0x81c2c92e47edaee6,0x92722c851482353b,
	0xa2bfe8a14cf10364,0xa81a664bbc423001,0xc24b8b70d0f89791,0xc76c51a30654be30,
	0xd192e819d6ef5218,0xd69906245565a910,0xf40e35855771202a,0x106aa07032bbd1b8,
	0x19a4c116b8d2d0c8,0x1e376c085141ab53,0x2748774cdf8eeb99,0x34b0bcb5e19b48a8,
	0x391c0cb3c5c95a63,0x4ed8aa4ae3418acb,0x5b9cca4f7763e373,0x682e6ff3d6b2b8a3,
	0x748f82ee5defb2fc,0x78a5636f43172f60,0x84c87814a1f0ab72,0x8cc702081a6439ec,
	0x90befffa23631e28,0xa4506cebde82bde9,0xbef9a3f7b2c67915,0xc67178f2e372532b,
	0xca273eceea26619c,0xd186b8c721c0c207,0xeada7dd6cde0eb1e,0xf57d4f7fee6ed178,
	0x06f067aa72176fba,0x0a637dc5a2c898a6,0x113f9804bef90dae,0x1b710b35131c471b,
	0x28db77f523047d84,0x32caab7b40c72493,0x3c9ebe0a15c9bebc,0x431d67c49c100d4c,
	0x4cc5d4becb3e42b6,0x597f299cfc657e2a,0x5fcb6fab3ad6faec,0x6c44198c4a475817
};

static void sha512_compress(crypto_sha512_ctx *ctx)
{
	u64 a = ctx->hash[0];    u64 b = ctx->hash[1];
	u64 c = ctx->hash[2];    u64 d = ctx->hash[3];
	u64 e = ctx->hash[4];    u64 f = ctx->hash[5];
	u64 g = ctx->hash[6];    u64 h = ctx->hash[7];

	FOR (j, 0, 16) {
		u64 in = K[j] + ctx->input[j];
		u64 t1 = big_sigma1(e) + ch (e, f, g) + h + in;
		u64 t2 = big_sigma0(a) + maj(a, b, c);
		h = g;  g = f;  f = e;  e = d  + t1;
		d = c;  c = b;  b = a;  a = t1 + t2;
	}
	size_t i16 = 0;
	FOR(i, 1, 5) {
		i16 += 16;
		FOR (j, 0, 16) {
			ctx->input[j] += lit_sigma1(ctx->input[(j- 2) & 15]);
			ctx->input[j] += lit_sigma0(ctx->input[(j-15) & 15]);
			ctx->input[j] +=            ctx->input[(j- 7) & 15];
			u64 in = K[i16 + j] + ctx->input[j];
			u64 t1 = big_sigma1(e) + ch (e, f, g) + h + in;
			u64 t2 = big_sigma0(a) + maj(a, b, c);
			h = g;  g = f;  f = e;  e = d  + t1;
			d = c;  c = b;  b = a;  a = t1 + t2;
		}
	}

	ctx->hash[0] += a;    ctx->hash[1] += b;
	ctx->hash[2] += c;    ctx->hash[3] += d;
	ctx->hash[4] += e;    ctx->hash[5] += f;
	ctx->hash[6] += g;    ctx->hash[7] += h;
}

// Write 1 input byte
static void sha512_set_input(crypto_sha512_ctx *ctx, u8 input)
{
	size_t word = ctx->input_idx >> 3;
	size_t byte = ctx->input_idx &  7;
	ctx->input[word] |= (u64)input << (8 * (7 - byte));
}

// Increment a 128-bit "word".
static void sha512_incr(u64 x[2], u64 y)
{
	x[1] += y;
	if (x[1] < y) {
		x[0]++;
	}
}

void crypto_sha512_init(crypto_sha512_ctx *ctx)
{
	ctx->hash[0] = 0x6a09e667f3bcc908;
	ctx->hash[1] = 0xbb67ae8584caa73b;
	ctx->hash[2] = 0x3c6ef372fe94f82b;
	ctx->hash[3] = 0xa54ff53a5f1d36f1;
	ctx->hash[4] = 0x510e527fade682d1;
	ctx->hash[5] = 0x9b05688c2b3e6c1f;
	ctx->hash[6] = 0x1f83d9abfb41bd6b;
	ctx->hash[7] = 0x5be0cd19137e2179;
	ctx->input_size[0] = 0;
	ctx->input_size[1] = 0;
	ctx->input_idx = 0;
	ZERO(ctx->input, 16);
}

void crypto_sha512_update(crypto_sha512_ctx *ctx,
                          const u8 *message, size_t message_size)
{
	// Avoid undefined NULL pointer increments with empty messages
	if (message_size == 0) {
		return;
	}

	// Align ourselves with word boundaries
	if ((ctx->input_idx & 7) != 0) {
		size_t nb_bytes = MIN(align(ctx->input_idx, 8), message_size);
		FOR (i, 0, nb_bytes) {
			sha512_set_input(ctx, message[i]);
			ctx->input_idx++;
		}
		message      += nb_bytes;
		message_size -= nb_bytes;
	}

	// Align ourselves with block boundaries
	if ((ctx->input_idx & 127) != 0) {
		size_t nb_words = MIN(align(ctx->input_idx, 128), message_size) >> 3;
		load64_be_buf(ctx->input + (ctx->input_idx >> 3), message, nb_words);
		ctx->input_idx += nb_words << 3;
		message        += nb_words << 3;
		message_size   -= nb_words << 3;
	}

	// Compress block if needed
	if (ctx->input_idx == 128) {
		sha512_incr(ctx->input_size, 1024); // size is in bits
		sha512_compress(ctx);
		ctx->input_idx = 0;
		ZERO(ctx->input, 16);
	}

	// Process the message block by block
	FOR (i, 0, message_size >> 7) { // number of blocks
		load64_be_buf(ctx->input, message, 16);
		sha512_incr(ctx->input_size, 1024); // size is in bits
		sha512_compress(ctx);
		ctx->input_idx = 0;
		ZERO(ctx->input, 16);
		message += 128;
	}
	message_size &= 127;

	if (message_size != 0) {
		// Remaining words
		size_t nb_words = message_size >> 3;
		load64_be_buf(ctx->input, message, nb_words);
		ctx->input_idx += nb_words << 3;
		message        += nb_words << 3;
		message_size   -= nb_words << 3;

		// Remaining bytes
		FOR (i, 0, message_size) {
			sha512_set_input(ctx, message[i]);
			ctx->input_idx++;
		}
	}
}

void crypto_sha512_final(crypto_sha512_ctx *ctx, u8 hash[64])
{
	// Add padding bit
	if (ctx->input_idx == 0) {
		ZERO(ctx->input, 16);
	}
	sha512_set_input(ctx, 128);

	// Update size
	sha512_incr(ctx->input_size, ctx->input_idx * 8);

	// Compress penultimate block (if any)
	if (ctx->input_idx > 111) {
		sha512_compress(ctx);
		ZERO(ctx->input, 14);
	}
	// Compress last block
	ctx->input[14] = ctx->input_size[0];
	ctx->input[15] = ctx->input_size[1];
	sha512_compress(ctx);

	// Copy hash to output (big endian)
	FOR (i, 0, 8) {
		store64_be(hash + i*8, ctx->hash[i]);
	}

	WIPE_CTX(ctx);
}

void crypto_sha512(u8 hash[64], const u8 *message, size_t message_size)
{
	crypto_sha512_ctx ctx;
	crypto_sha512_init  (&ctx);
	crypto_sha512_update(&ctx, message, message_size);
	crypto_sha512_final (&ctx, hash);
}

////////////////////
/// HMAC SHA 512 ///
////////////////////
void crypto_sha512_hmac_init(crypto_sha512_hmac_ctx *ctx,
                             const u8 *key, size_t key_size)
{
	// hash key if it is too long
	if (key_size > 128) {
		crypto_sha512(ctx->key, key, key_size);
		key      = ctx->key;
		key_size = 64;
	}
	// Compute inner key: padded key XOR 0x36
	FOR (i, 0, key_size)   { ctx->key[i] = key[i] ^ 0x36; }
	FOR (i, key_size, 128) { ctx->key[i] =          0x36; }
	// Start computing inner hash
	crypto_sha512_init  (&ctx->ctx);
	crypto_sha512_update(&ctx->ctx, ctx->key, 128);
}

void crypto_sha512_hmac_update(crypto_sha512_hmac_ctx *ctx,
                               const u8 *message, size_t message_size)
{
	crypto_sha512_update(&ctx->ctx, message, message_size);
}

void crypto_sha512_hmac_final(crypto_sha512_hmac_ctx *ctx, u8 hmac[64])
{
	// Finish computing inner hash
	crypto_sha512_final(&ctx->ctx, hmac);
	// Compute outer key: padded key XOR 0x5c
	FOR (i, 0, 128) {
		ctx->key[i] ^= 0x36 ^ 0x5c;
	}
	// Compute outer hash
	crypto_sha512_init  (&ctx->ctx);
	crypto_sha512_update(&ctx->ctx, ctx->key , 128);
	crypto_sha512_update(&ctx->ctx, hmac, 64);
	crypto_sha512_final (&ctx->ctx, hmac); // outer hash
	WIPE_CTX(ctx);
}

void crypto_sha512_hmac(u8 hmac[64], const u8 *key, size_t key_size,
                        const u8 *message, size_t message_size)
{
	crypto_sha512_hmac_ctx ctx;
	crypto_sha512_hmac_init  (&ctx, key, key_size);
	crypto_sha512_hmac_update(&ctx, message, message_size);
	crypto_sha512_hmac_final (&ctx, hmac);
}

////////////////////
/// HKDF SHA 512 ///
////////////////////
void crypto_sha512_hkdf_expand(u8       *okm,  size_t okm_size,
                               const u8 *prk,  size_t prk_size,
                               const u8 *info, size_t info_size)
{
	int not_first = 0;
	u8 ctr = 1;
	u8 blk[64];

	while (okm_size > 0) {
		size_t out_size = MIN(okm_size, sizeof(blk));

		crypto_sha512_hmac_ctx ctx;
		crypto_sha512_hmac_init(&ctx, prk , prk_size);
		if (not_first) {
			// For some reason HKDF uses some kind of CBC mode.
			// For some reason CTR mode alone wasn't enough.
			// Like what, they didn't trust HMAC in 2010?  Really??
			crypto_sha512_hmac_update(&ctx, blk , sizeof(blk));
		}
		crypto_sha512_hmac_update(&ctx, info, info_size);
		crypto_sha512_hmac_update(&ctx, &ctr, 1);
		crypto_sha512_hmac_final(&ctx, blk);

		COPY(okm, blk, out_size);

		not_first = 1;
		okm      += out_size;
		okm_size -= out_size;
		ctr++;
	}
}

void crypto_sha512_hkdf(u8       *okm , size_t okm_size,
                        const u8 *ikm , size_t ikm_size,
                        const u8 *salt, size_t salt_size,
                        const u8 *info, size_t info_size)
{
	// Extract
	u8 prk[64];
	crypto_sha512_hmac(prk, salt, salt_size, ikm, ikm_size);

	// Expand
	crypto_sha512_hkdf_expand(okm, okm_size, prk, sizeof(prk), info, info_size);
}

///////////////
/// Ed25519 ///
///////////////
void crypto_ed25519_key_pair(u8 secret_key[64], u8 public_key[32], u8 seed[32])
{
	u8 a[64];
	COPY(a, seed, 32);                      // a[ 0..31]  = seed
	crypto_wipe(seed, 32);
	COPY(secret_key, a, 32);                // secret key = seed
	crypto_sha512(a, a, 32);                // a[ 0..31]  = scalar
	crypto_eddsa_trim_scalar(a, a);         // a[ 0..31]  = trimmed scalar
	crypto_eddsa_scalarbase(public_key, a); // public key = [trimmed scalar]B
	COPY(secret_key + 32, public_key, 32);  // secret key includes public half
	WIPE_BUFFER(a);
}

static void hash_reduce(u8 h[32],
                        const u8 *a, size_t a_size,
                        const u8 *b, size_t b_size,
                        const u8 *c, size_t c_size,
                        const u8 *d, size_t d_size)
{
	u8 hash[64];
	crypto_sha512_ctx ctx;
	crypto_sha512_init  (&ctx);
	crypto_sha512_update(&ctx, a, a_size);
	crypto_sha512_update(&ctx, b, b_size);
	crypto_sha512_update(&ctx, c, c_size);
	crypto_sha512_update(&ctx, d, d_size);
	crypto_sha512_final (&ctx, hash);
	crypto_eddsa_reduce(h, hash);
}

static void ed25519_dom_sign(u8 signature [64], const u8 secret_key[32],
                             const u8 *dom,     size_t dom_size,
                             const u8 *message, size_t message_size)
{
	u8 a[64];  // secret scalar and prefix
	u8 r[32];  // secret deterministic "random" nonce
	u8 h[32];  // publically verifiable hash of the message (not wiped)
	u8 R[32];  // first half of the signature (allows overlapping inputs)
	const u8 *pk = secret_key + 32;

	crypto_sha512(a, secret_key, 32);
	crypto_eddsa_trim_scalar(a, a);
	hash_reduce(r, dom, dom_size, a + 32, 32, message, message_size, 0, 0);
	crypto_eddsa_scalarbase(R, r);
	hash_reduce(h, dom, dom_size, R, 32, pk, 32, message, message_size);
	COPY(signature, R, 32);
	crypto_eddsa_mul_add(signature + 32, h, a, r);

	WIPE_BUFFER(a);
	WIPE_BUFFER(r);
}

void crypto_ed25519_sign(u8 signature [64], const u8 secret_key[64],
                         const u8 *message, size_t message_size)
{
	ed25519_dom_sign(signature, secret_key, 0, 0, message, message_size);
}

int crypto_ed25519_check(const u8 signature[64], const u8 public_key[32],
                         const u8 *msg, size_t msg_size)
{
	u8 h_ram[32];
	hash_reduce(h_ram, signature, 32, public_key, 32, msg, msg_size, 0, 0);
	return crypto_eddsa_check_equation(signature, public_key, h_ram);
}

static const u8 domain[34] = "SigEd25519 no Ed25519 collisions\1";

void crypto_ed25519_ph_sign(uint8_t signature[64], const uint8_t secret_key[64],
                            const uint8_t message_hash[64])
{
	ed25519_dom_sign(signature, secret_key, domain, sizeof(domain),
	                 message_hash, 64);
}

int crypto_ed25519_ph_check(const uint8_t sig[64], const uint8_t pk[32],
                            const uint8_t msg_hash[64])
{
	u8 h_ram[32];
	hash_reduce(h_ram, domain, sizeof(domain), sig, 32, pk, 32, msg_hash, 64);
	return crypto_eddsa_check_equation(sig, pk, h_ram);
}


#ifdef MONOCYPHER_CPP_NAMESPACE
}
#endif
