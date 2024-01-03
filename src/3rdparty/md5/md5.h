/** @file md5.h Functions to create MD5 checksums. */

/*
  Copyright (C) 1999, 2002 Aladdin Enterprises.  All rights reserved.

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

  The original and principal author of md5.h is L. Peter Deutsch
  <ghost@aladdin.com>.  Other authors are noted in the change history
  that follows (in reverse chronological order):

  2007-12-24 Changed to C++ and adapted to OpenTTD source
  2002-04-13 lpd Removed support for non-ANSI compilers; removed
             references to Ghostscript; clarified derivation from RFC 1321;
             now handles byte order either statically or dynamically.
  1999-11-04 lpd Edited comments slightly for automatic TOC extraction.
  1999-10-18 lpd Fixed typo in header comment (ansi2knr rather than md5);
             added conditionalization for C++ compilation from Martin
             Purschke <purschke@bnl.gov>.
  1999-05-03 lpd Original version.
 */

#ifndef MD5_INCLUDED
#define MD5_INCLUDED

/** The number of bytes in a MD5 hash. */
static const size_t MD5_HASH_BYTES = 16;

/** Container for storing a MD5 hash/checksum/digest. */
struct MD5Hash : std::array<byte, MD5_HASH_BYTES> {
	MD5Hash() : std::array<byte, MD5_HASH_BYTES>{} {}

	/**
	 * Exclusively-or the given hash into this hash.
	 * @param other The other hash.
	 * @return Reference to this hash.
	 */
	MD5Hash &operator^=(const MD5Hash &other)
	{
		for (size_t i = 0; i < size(); i++) this->operator[](i) ^= other[i];
		return *this;
	}
};

struct Md5 {
private:
	uint32_t count[2]; ///< message length in bits, lsw first
	uint32_t abcd[4];  ///< digest buffer
	uint8_t buf[64];   ///< accumulate block

	void Process(const uint8_t *data);

public:
	Md5();
	void Append(const void *data, const size_t nbytes);
	void Finish(MD5Hash &digest);
};

#endif /* MD5_INCLUDED */
