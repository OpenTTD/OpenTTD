/** @file md5.cpp Creating MD5 checksums of files. */

/*
  Copyright (C) 1999, 2000, 2002 Aladdin Enterprises.  All rights reserved.

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  L. Peter Deutsch
  ghost@aladdin.com

 */

/*
  Independent implementation of MD5 (RFC 1321).

  This code implements the MD5 Algorithm defined in RFC 1321, whose
  text is available at
  http://www.ietf.org/rfc/rfc1321.txt
  The code is derived from the text of the RFC, including the test suite
  (section A.5) but excluding the rest of Appendix A.  It does not include
  any code or documentation that is identified in the RFC as being
  copyrighted.

  The original and principal author of md5.c is L. Peter Deutsch
  <ghost@aladdin.com>.  Other authors are noted in the change history
  that follows (in reverse chronological order):

  2007-12-24 Changed to C++ and adapted to OpenTTD source
  2002-04-13 lpd Clarified derivation from RFC 1321; now handles byte order
             either statically or dynamically; added missing #include <string.h>
             in library.
  2002-03-11 lpd Corrected argument list for main(), and added int return
             type, in test program and T value program.
  2002-02-21 lpd Added missing #include <stdio.h> in test program.
  2000-07-03 lpd Patched to eliminate warnings about "constant is
             unsigned in ANSI C, signed in traditional"; made test program
             self-checking.
  1999-11-04 lpd Edited comments slightly for automatic TOC extraction.
  1999-10-18 lpd Fixed typo in header comment (ansi2knr rather than md5).
  1999-05-03 lpd Original version.
 */

#include "../../stdafx.h"
#include "../../core/endian_func.hpp"
#include "md5.h"

#include "../../safeguards.h"

#define T_MASK ((uint32_t)~0)
#define T1 /* 0xd76aa478 */ (T_MASK ^ 0x28955b87)
#define T2 /* 0xe8c7b756 */ (T_MASK ^ 0x173848a9)
#define T3    0x242070db
#define T4 /* 0xc1bdceee */ (T_MASK ^ 0x3e423111)
#define T5 /* 0xf57c0faf */ (T_MASK ^ 0x0a83f050)
#define T6    0x4787c62a
#define T7 /* 0xa8304613 */ (T_MASK ^ 0x57cfb9ec)
#define T8 /* 0xfd469501 */ (T_MASK ^ 0x02b96afe)
#define T9    0x698098d8
#define T10 /* 0x8b44f7af */ (T_MASK ^ 0x74bb0850)
#define T11 /* 0xffff5bb1 */ (T_MASK ^ 0x0000a44e)
#define T12 /* 0x895cd7be */ (T_MASK ^ 0x76a32841)
#define T13    0x6b901122
#define T14 /* 0xfd987193 */ (T_MASK ^ 0x02678e6c)
#define T15 /* 0xa679438e */ (T_MASK ^ 0x5986bc71)
#define T16    0x49b40821
#define T17 /* 0xf61e2562 */ (T_MASK ^ 0x09e1da9d)
#define T18 /* 0xc040b340 */ (T_MASK ^ 0x3fbf4cbf)
#define T19    0x265e5a51
#define T20 /* 0xe9b6c7aa */ (T_MASK ^ 0x16493855)
#define T21 /* 0xd62f105d */ (T_MASK ^ 0x29d0efa2)
#define T22    0x02441453
#define T23 /* 0xd8a1e681 */ (T_MASK ^ 0x275e197e)
#define T24 /* 0xe7d3fbc8 */ (T_MASK ^ 0x182c0437)
#define T25    0x21e1cde6
#define T26 /* 0xc33707d6 */ (T_MASK ^ 0x3cc8f829)
#define T27 /* 0xf4d50d87 */ (T_MASK ^ 0x0b2af278)
#define T28    0x455a14ed
#define T29 /* 0xa9e3e905 */ (T_MASK ^ 0x561c16fa)
#define T30 /* 0xfcefa3f8 */ (T_MASK ^ 0x03105c07)
#define T31    0x676f02d9
#define T32 /* 0x8d2a4c8a */ (T_MASK ^ 0x72d5b375)
#define T33 /* 0xfffa3942 */ (T_MASK ^ 0x0005c6bd)
#define T34 /* 0x8771f681 */ (T_MASK ^ 0x788e097e)
#define T35    0x6d9d6122
#define T36 /* 0xfde5380c */ (T_MASK ^ 0x021ac7f3)
#define T37 /* 0xa4beea44 */ (T_MASK ^ 0x5b4115bb)
#define T38    0x4bdecfa9
#define T39 /* 0xf6bb4b60 */ (T_MASK ^ 0x0944b49f)
#define T40 /* 0xbebfbc70 */ (T_MASK ^ 0x4140438f)
#define T41    0x289b7ec6
#define T42 /* 0xeaa127fa */ (T_MASK ^ 0x155ed805)
#define T43 /* 0xd4ef3085 */ (T_MASK ^ 0x2b10cf7a)
#define T44    0x04881d05
#define T45 /* 0xd9d4d039 */ (T_MASK ^ 0x262b2fc6)
#define T46 /* 0xe6db99e5 */ (T_MASK ^ 0x1924661a)
#define T47    0x1fa27cf8
#define T48 /* 0xc4ac5665 */ (T_MASK ^ 0x3b53a99a)
#define T49 /* 0xf4292244 */ (T_MASK ^ 0x0bd6ddbb)
#define T50    0x432aff97
#define T51 /* 0xab9423a7 */ (T_MASK ^ 0x546bdc58)
#define T52 /* 0xfc93a039 */ (T_MASK ^ 0x036c5fc6)
#define T53    0x655b59c3
#define T54 /* 0x8f0ccc92 */ (T_MASK ^ 0x70f3336d)
#define T55 /* 0xffeff47d */ (T_MASK ^ 0x00100b82)
#define T56 /* 0x85845dd1 */ (T_MASK ^ 0x7a7ba22e)
#define T57    0x6fa87e4f
#define T58 /* 0xfe2ce6e0 */ (T_MASK ^ 0x01d3191f)
#define T59 /* 0xa3014314 */ (T_MASK ^ 0x5cfebceb)
#define T60    0x4e0811a1
#define T61 /* 0xf7537e82 */ (T_MASK ^ 0x08ac817d)
#define T62 /* 0xbd3af235 */ (T_MASK ^ 0x42c50dca)
#define T63    0x2ad7d2bb
#define T64 /* 0xeb86d391 */ (T_MASK ^ 0x14792c6e)

static inline void Md5Set1(const uint32_t *X, uint32_t *a, const uint32_t *b, const uint32_t *c, const uint32_t *d, const uint8_t k, const uint8_t s, const uint32_t Ti)
{
	uint32_t t = (*b & *c) | (~*b & *d);
	t += *a + X[k] + Ti;
	*a = ROL(t, s) + *b;
}

static inline void Md5Set2(const uint32_t *X, uint32_t *a, const uint32_t *b, const uint32_t *c, const uint32_t *d, const uint8_t k, const uint8_t s, const uint32_t Ti)
{
	uint32_t t = (*b & *d) | (*c & ~*d);
	t += *a + X[k] + Ti;
	*a = ROL(t, s) + *b;
}


static inline void Md5Set3(const uint32_t *X, uint32_t *a, const uint32_t *b, const uint32_t *c, const uint32_t *d, const uint8_t k, const uint8_t s, const uint32_t Ti)
{
	uint32_t t = *b ^ *c ^ *d;
	t += *a + X[k] + Ti;
	*a = ROL(t, s) + *b;
}

static inline void Md5Set4(const uint32_t *X, uint32_t *a, const uint32_t *b, const uint32_t *c, const uint32_t *d, const uint8_t k, const uint8_t s, const uint32_t Ti)
{
	uint32_t t = *c ^ (*b | ~*d);
	t += *a + X[k] + Ti;
	*a = ROL(t, s) + *b;
}

Md5::Md5()
{
	count[0] = 0;
	count[1] = 0;
	abcd[0] = 0x67452301;
	abcd[1] = /*0xefcdab89*/ T_MASK ^ 0x10325476;
	abcd[2] = /*0x98badcfe*/ T_MASK ^ 0x67452301;
	abcd[3] = 0x10325476;
}

void Md5::Process(const uint8_t *data /*[64]*/)
{
	uint32_t a = this->abcd[0];
	uint32_t b = this->abcd[1];
	uint32_t c = this->abcd[2];
	uint32_t d = this->abcd[3];

	uint32_t X[16];

	/* Convert the uint8_t data to uint32_t LE */
	const uint32_t *px = (const uint32_t *)data;
	for (uint i = 0; i < 16; i++) {
		X[i] = TO_LE32(*px);
		px++;
	}

	/* Round 1. */
	Md5Set1(X, &a, &b, &c, &d,  0,  7,  T1);
	Md5Set1(X, &d, &a, &b, &c,  1, 12,  T2);
	Md5Set1(X, &c, &d, &a, &b,  2, 17,  T3);
	Md5Set1(X, &b, &c, &d, &a,  3, 22,  T4);
	Md5Set1(X, &a, &b, &c, &d,  4,  7,  T5);
	Md5Set1(X, &d, &a, &b, &c,  5, 12,  T6);
	Md5Set1(X, &c, &d, &a, &b,  6, 17,  T7);
	Md5Set1(X, &b, &c, &d, &a,  7, 22,  T8);
	Md5Set1(X, &a, &b, &c, &d,  8,  7,  T9);
	Md5Set1(X, &d, &a, &b, &c,  9, 12, T10);
	Md5Set1(X, &c, &d, &a, &b, 10, 17, T11);
	Md5Set1(X, &b, &c, &d, &a, 11, 22, T12);
	Md5Set1(X, &a, &b, &c, &d, 12,  7, T13);
	Md5Set1(X, &d, &a, &b, &c, 13, 12, T14);
	Md5Set1(X, &c, &d, &a, &b, 14, 17, T15);
	Md5Set1(X, &b, &c, &d, &a, 15, 22, T16);

	/* Round 2. */
	Md5Set2(X, &a, &b, &c, &d,  1,  5, T17);
	Md5Set2(X, &d, &a, &b, &c,  6,  9, T18);
	Md5Set2(X, &c, &d, &a, &b, 11, 14, T19);
	Md5Set2(X, &b, &c, &d, &a,  0, 20, T20);
	Md5Set2(X, &a, &b, &c, &d,  5,  5, T21);
	Md5Set2(X, &d, &a, &b, &c, 10,  9, T22);
	Md5Set2(X, &c, &d, &a, &b, 15, 14, T23);
	Md5Set2(X, &b, &c, &d, &a,  4, 20, T24);
	Md5Set2(X, &a, &b, &c, &d,  9,  5, T25);
	Md5Set2(X, &d, &a, &b, &c, 14,  9, T26);
	Md5Set2(X, &c, &d, &a, &b,  3, 14, T27);
	Md5Set2(X, &b, &c, &d, &a,  8, 20, T28);
	Md5Set2(X, &a, &b, &c, &d, 13,  5, T29);
	Md5Set2(X, &d, &a, &b, &c,  2,  9, T30);
	Md5Set2(X, &c, &d, &a, &b,  7, 14, T31);
	Md5Set2(X, &b, &c, &d, &a, 12, 20, T32);

	/* Round 3. */
	Md5Set3(X, &a, &b, &c, &d,  5,  4, T33);
	Md5Set3(X, &d, &a, &b, &c,  8, 11, T34);
	Md5Set3(X, &c, &d, &a, &b, 11, 16, T35);
	Md5Set3(X, &b, &c, &d, &a, 14, 23, T36);
	Md5Set3(X, &a, &b, &c, &d,  1,  4, T37);
	Md5Set3(X, &d, &a, &b, &c,  4, 11, T38);
	Md5Set3(X, &c, &d, &a, &b,  7, 16, T39);
	Md5Set3(X, &b, &c, &d, &a, 10, 23, T40);
	Md5Set3(X, &a, &b, &c, &d, 13,  4, T41);
	Md5Set3(X, &d, &a, &b, &c,  0, 11, T42);
	Md5Set3(X, &c, &d, &a, &b,  3, 16, T43);
	Md5Set3(X, &b, &c, &d, &a,  6, 23, T44);
	Md5Set3(X, &a, &b, &c, &d,  9,  4, T45);
	Md5Set3(X, &d, &a, &b, &c, 12, 11, T46);
	Md5Set3(X, &c, &d, &a, &b, 15, 16, T47);
	Md5Set3(X, &b, &c, &d, &a,  2, 23, T48);

	/* Round 4. */
	Md5Set4(X, &a, &b, &c, &d,  0,  6, T49);
	Md5Set4(X, &d, &a, &b, &c,  7, 10, T50);
	Md5Set4(X, &c, &d, &a, &b, 14, 15, T51);
	Md5Set4(X, &b, &c, &d, &a,  5, 21, T52);
	Md5Set4(X, &a, &b, &c, &d, 12,  6, T53);
	Md5Set4(X, &d, &a, &b, &c,  3, 10, T54);
	Md5Set4(X, &c, &d, &a, &b, 10, 15, T55);
	Md5Set4(X, &b, &c, &d, &a,  1, 21, T56);
	Md5Set4(X, &a, &b, &c, &d,  8,  6, T57);
	Md5Set4(X, &d, &a, &b, &c, 15, 10, T58);
	Md5Set4(X, &c, &d, &a, &b,  6, 15, T59);
	Md5Set4(X, &b, &c, &d, &a, 13, 21, T60);
	Md5Set4(X, &a, &b, &c, &d,  4,  6, T61);
	Md5Set4(X, &d, &a, &b, &c, 11, 10, T62);
	Md5Set4(X, &c, &d, &a, &b,  2, 15, T63);
	Md5Set4(X, &b, &c, &d, &a,  9, 21, T64);

	/* Then perform the following additions. (That is increment each
	 * of the four registers by the value it had before this block
	 * was started.) */
	this->abcd[0] += a;
	this->abcd[1] += b;
	this->abcd[2] += c;
	this->abcd[3] += d;
}

void Md5::Append(const void *data, const size_t nbytes)
{
	const uint8_t *p = (const uint8_t *)data;
	size_t left = nbytes;
	const size_t offset = (this->count[0] >> 3) & 63;
	const uint32_t nbits = (uint32_t)(nbytes << 3);

	if (nbytes <= 0) return;

	/* Update the message length. */
	this->count[1] += (uint32_t)(nbytes >> 29);
	this->count[0] += nbits;

	if (this->count[0] < nbits) this->count[1]++;

	/* Process an initial partial block. */
	if (offset) {
		size_t copy = (offset + nbytes > 64 ? 64 - offset : nbytes);

		memcpy(this->buf + offset, p, copy);

		if (offset + copy < 64) return;

		p += copy;
		left -= copy;
		this->Process(this->buf);
	}

	/* Process full blocks. */
	for (; left >= 64; p += 64, left -= 64) this->Process(p);

	/* Process a final partial block. */
	if (left) memcpy(this->buf, p, left);
}

void Md5::Finish(MD5Hash &digest)
{
	static const uint8_t pad[64] = {
		0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	};
	uint8_t data[8];

	/* Save the length before padding. */
	for (uint i = 0; i < 8; ++i) {
		data[i] = (uint8_t)(this->count[i >> 2] >> ((i & 3) << 3));
	}

	/* Pad to 56 bytes mod 64. */
	this->Append(pad, ((55 - (this->count[0] >> 3)) & 63) + 1);
	/* Append the length. */
	this->Append(data, 8);

	for (uint i = 0; i < 16; ++i) {
		digest[i] = (uint8_t)(this->abcd[i >> 2] >> ((i & 3) << 3));
	}
}
