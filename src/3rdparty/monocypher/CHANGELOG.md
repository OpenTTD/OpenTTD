4.0.2
-----
2023/08/24

- Fixed multiple-lanes Argon2.
- Improved Poly1305 performance.
- Improved Argon2 performance.
- Makefiles no longer override standard environment variables.


4.0.1
-----
2023/03/06

- Fixed Ed25519 secret key size in function prototype.
- Fixed soname (should have been changed in 4.0.0)
- Added convenience sub-targets to makefile.
- Briefly specified wire format of Elligator and incremental AEAD.


4.0.0
-----
2023/02/20

- Fixed unsafe signature API.
- Simpler, more flexible low-level signature API.
- Fully specified, consensus-friendly signatures.
- Added Argon2d and Argon2id, support multiple lanes.
- Added safe and fast streaming AEAD.
- Added HKDF-SHA-512 and documented BLAKE2b KDF.
- More consistent and memorable function names.
- POSIX makefile.


3.1.3
-----
2022/04/25

- Fixed many typos in the documentation.
- Fixed buffer overflow in speed benchmarks.
- Fixed some MSVC warnings.
- Fixed a minor violation of the Elligator2 reverse map specs.
- Added `change-prefix.sh` to help changing the `crypto_` prefix.
- Added the `MONOCYPHER_CPP_NAMESPACE` preprocessor definition to
  support namespaces for C++.
- Deprecated `crypto_key_exchange()`
- Use GitHub actions to automate the regular test suite.


3.1.2
-----
2020/12/27

- Addressed issues from Cure53's audit:
  - MON-01-001: Clarified which CSPRNG to use on Darwin.
  - MON-01-002: Won't fix (nonce handling is a core design decision).
  - MON-01-004: Compared with Kleshni's implementation.
  - MON-01-005: Split a dedicated "advanced" folder in the manual.
- Quality assurance for 2^255-19 arithmetic (elliptic curves):
  - Documented carry propagation.
  - Enforced slightly safer invariants.
- Improved the speed of EdDSA signature generation.
- Made the vectors.h header more compact and easier to modify.
- TIS-CI integration.
- Added speed benchmark for ed25519-donna.
- Documented lengths limits of `crypto_ietf_chacha20()`


3.1.1
-----
2020/06/15

- Various documentation fixes.
- Fixed various compiler warnings.
- Fixed some integer overflows (16-bit platforms only).


3.1.0
-----
2020/04/03

- Added Elligator 2 mappings (hash to curve, curve to hash).
- Added OPRF support (with scalar inversion).
- Added Edwards25519 -> Curve25519 conversions.


3.0.0
-----
2020/01/19

- Deprecated the incremental AEAD interface.
- Deprecated the incremental Chacha20, added a direct interface.
- Added IETF Chacha20 (96-bit nonce), as described in RFC 8439.
- Moved deprecated interfaces to a separate `src/deprecated` folder.
- Removed the `ED25519_SHA512` preprocessor flag.
- `crypto_x25519()` and `crypto_key_exchange()` now return `void`.
- Added a custom hash interface to EdDSA.  Several instances of EdDSA
  can share the same binary.
- Added optional support for HMAC SHA-512.
- Moved SHA-512 operations to `src/optional/monocypher-ed25519.(h|c)`.
- Optional support for Ed25519 no longer requires a preprocessor flag.
  Add `src/optional/monocypher-ed25519.(h|c)` to your project instead.


2.0.6
-----
2019/10/21

- Added the `BLAKE2_NO_UNROLLING` preprocessor definition. Activating
  it makes the binary about 5KB smaller and speeds up processing times
  on many embedded processors.
- Reduced the stack usage of signature verification by about
  40%. Signature verification now fits in smaller machines.
- Fixed many implicit casts warnings.
- Fixed the manual here and there.
- Lots of small nitpicks.


2.0.5
-----
2018/08/23

- Faster EdDSA signatures and verification.  Like, 4 times as fast.


2.0.4
-----
2018/06/24

- Corrected a critical vulnerability found by Mike Pechkin in EdDSA,
  where crypto_check() was accepting invalid signatures.  The current
  fix removes a buggy optimisation, effectively halving the performance
  of EdDSA.
- The test suite no longer tries to allocate zero bytes (some platforms
  fail such an allocation).

2.0.3
-----
2018/06/16

- Corrected undefined behaviour in BLAKE2b.
- Improved the test suite (faster, better coverage).

2.0.2
-----
2018/04/23

- Corrected a couple failures to wipe secret buffers.
- Corrected a bug that prevented compilation in Ed25519 mode.
- Adjusted the number of test vectors in the test suite.
- Improved tests for incremental interfaces.
- Replaced the GNU all permissive licence by a public domain dedication
  (Creative Commons CC-0).  The BSD licence remains as a fallback.

2.0.1
-----
2018/03/07

- Followed a systematic pattern for the loading code of symmetric
  crypto.  It is now easier to review.
- Tweaked Poly1305 code to make it easier to prove correct.

2.0.0
-----
2018/02/14

- Changed the authenticated encryption format.  It now conforms to
  RFC 7539, with one exception: it uses XChacha20 initialisation instead
  of the IETF version of Chacha20.  This new format conforms to
  libsodium's `crypto_aead_xchacha20poly1305_ietf_encrypt`.
- Removed `crypto_lock_encrypt()` and `crypto_lock_auth()`.
- Renamed `crypto_lock_aead_auth()` to `crypto_lock_auth_ad()`.
- Renamed `crypto_unlock_aead_auth()` to `crypto_unlock_auth_ad()`.
- Added `crypto_lock_auth_message()` and `crypto_unlock_auth_message()`.
- Renamed `crypto_aead_lock` to `crypto_lock_aead`.
- Renamed `crypto_aead_unlock` to `crypto_unlock_aead`.

The format change facilitates optimisation by aligning data to block
boundaries.  The API changes increase consistency.

1.1.0
-----
2018/02/06

- Rewrote the manual into proper man pages.
- Added incremental interfaces for authenticated encryption and
  signatures.
- Replaced `crypto_memcmp()` by 3 fixed size buffer comparisons (16, 32,
  and 64 bytes), to make sure the generated code remains constant time.
- A couple breaking API changes, easily fixed by renaming the affected
  functions.

1.0.1
-----
2017/07/23

- Optimised the loading and unloading code of the symmetric crypto
  (BLAKE2b, SHA-512, Chacha20, and Poly1305).
- Fused self-contained tests together for easier analysis with Frama-C
  and the TIS interpreter.

1.0
---
2017/07/18

- Renamed `crypto_chacha20_Xinit` to `crypto_chacha20_x_init`, for
  consistency reasons (snake case everywhere).
- Fixed signed integer overflow detected by UBSan.
- Doubled the speed of EdDSA by performing the scalar product in
  Montgomery space.

0.8
---
2017/07/06

- Added about a hundred lines of code to improve performance of public
  key cryptography.  Diffie-Hellman is now 20% faster than before.
  The effects are less pronounced for EdDSA.
- Added random self-consistency tests.
- Added a speed benchmark against libsodium.

0.7
---
2017/06/07

- Slightly changed the authenticated encryption API.  Functions are
  now all in "detached" mode.  The reason is better support for
  authenticated encryption _without_ additional data.
- Rewrote BLAKE2b from spec so it can use the same licence as
  everything else.
- Added random tests that compare Monocypher with libsodium and
  ed25519-donna.
- Added explicit support for Frama-C analysis (this doesn't affect the
  source code).

0.6
---
2017/03/17

- Fixed incorrect Poly1305 output on empty messages.  (Found by Mike
  Pechkin.)

0.5
---
2017/03/10

- Fixed many undefined behaviours in Curve25519 that occur whenever
  we perform a left shift on a signed negative integer.  It doesn't
  affect the generated code, but you never know.  (Found with Frama-C
  by André Maroneze.)

Fun fact: TweetNaCl and ref10 have the same bug.  Libsodium has
corrected the issue, though.

For those who don't comprehend the magnitude of this madness, the
expression `-1 << 3` is undefined in C.  This is explained in
section 6.5.7(§4) of the C11 standard.

0.4
---
2017/03/09

- Fixed critical bug causing Argon2i to fail whenever it uses more
  than 512 blocks.  It was reading uninitialised memory and the
  results were incorrect.  (Found by Mike Pechkin.)
- Fixed an undefined behaviour in Curve25519 (`fe_tobytes()`).  It was
  accessing uninitialised memory before throwing it away.  It didn't
  affect the compiled code nor the results, but you never know.
  (Found with [Frama-C](http://frama-c.com) by André Maroneze.)

0.3
---
2017/02/27

- Got the invariants of Poly1305 right and put them in the comments.
  There was no bug, but that was lucky (turned out the IETF test
  vectors were designed to trigger the bugs I was afraid of).
- Simplified Poly1305 finalisation (replaced conditional subtraction
  by a carry propagation).
- Made a few cosmetic changes here and there.

0.2
---
????/??/??

- Public interface significantly reworked. Removed redundant, hard to
  mess up constructions.
- Added AEAD.
- Sped up Curve25519 by a factor of more than 6 (switched to ref10
  arithmetic).
- Added various test vectors and completed the consistency tests.

0.1
---
2016/??/??
