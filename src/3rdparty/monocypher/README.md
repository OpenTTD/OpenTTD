Monocypher
----------

Monocypher is an easy to use, easy to deploy, auditable crypto library
written in portable C.  It approaches the size of [TweetNaCl][] and the
speed of [libsodium][].

[Official site.](https://monocypher.org/)
[Official releases.](https://monocypher.org/download/)

[libsodium]: https://libsodium.org
[TweetNaCl]: https://tweetnacl.cr.yp.to/


Features
--------

- [Authenticated Encryption][AEAD] with XChaCha20 and Poly1305
  (RFC 8439).
- [Hashing and key derivation][HASH] with BLAKE2b (and [SHA-512][]).
- [Password Hashing][PWH] with Argon2.
- [Public Key Cryptography][PKC] with X25519 key exchanges.
- [Public Key Signatures][EDDSA] with EdDSA and [Ed25519][].
- [Steganography and PAKE][STEG] with [Elligator 2][ELLI].

[AEAD]:    https://monocypher.org/manual/aead
[HASH]:    https://monocypher.org/manual/blake2
[SHA-512]: https://monocypher.org/manual/sha-512
[PWH]:     https://monocypher.org/manual/argon2
[PKC]:     https://monocypher.org/manual/x25519
[EDDSA]:   https://monocypher.org/manual/eddsa
[Ed25519]: https://monocypher.org/manual/ed25519
[STEG]:    https://monocypher.org/manual/elligator
[ELLI]:    https://elligator.org


Manual
------

The manual can be found at https://monocypher.org/manual/, and in the
`doc/` folder.


Installation
------------

### Option 1: grab the sources

The easiest way to use Monocypher is to include `src/monocypher.h` and
`src/monocypher.c` directly into your project.  They compile as C (since
C99) and C++ (since C++98).

If you need the optional SHA-512 or Ed25519, grab
`src/optional/monocypher-ed25519.h` and
`src/optional/monocypher-ed25519.c` as well.

### Option 2: grab the library

Run `make`, then grab the `src/monocypher.h` header and either the
`lib/libmonocypher.a` or `lib/libmonocypher.so` library.  The default
compiler is `gcc -std=c99`, and the default flags are `-pedantic -Wall
-Wextra -O3 -march=native`.  If they don't work on your platform, you
can change them like this:

    $ make CC="clang -std=c11" CFLAGS="-O2"

### Option 3: install it on your system

Run `make`, then `make install` as root. This will install Monocypher in
`/usr/local` by default. This can be changed with `PREFIX` and
`DESTDIR`:

    $ make install PREFIX="/opt"

Once installed, you may use `pkg-config` to compile and link your
program.  For instance:

    $ gcc program.c $(pkg-config monocypher --cflags) -c
    $ gcc program.o $(pkg-config monocypher --libs)   -o program

If for any reason you wish to avoid installing the man pages or the
`pkg-config` file, you can use the following installation sub targets
instead: `install-lib`, `install-doc`, and `install-pc`.


Test suite
----------

    $ make test

It should display a nice printout of all the tests, ending with "All
tests OK!". If you see "failure" or "Error" anywhere, something has gone
wrong.

*Do not* use Monocypher without running those tests at least once.

The same test suite can be run under Clang sanitisers and Valgrind, and
be checked for code coverage:

    $ tests/test.sh
    $ tests/coverage.sh


### Serious auditing

The code may be analysed more formally with [Frama-c][] and the
[TIS interpreter][TIS].  To analyse the code with Frama-c, run:

    $ tests/formal-analysis.sh
    $ tests/frama-c.sh

This will have Frama-c parse, and analyse the code, then launch a GUI.
You must have Frama-c installed.  See `frama-c.sh` for the recommended
settings.  To run the code under the TIS interpreter, run

    $ tests/formal-analysis.sh
    $ tis-interpreter.sh --cc -Dvolatile= tests/formal-analysis/*.c

Notes:

- `tis-interpreter.sh` is part of TIS.  If it is not in your path,
  adjust the command accordingly.

- The TIS interpreter sometimes fails to evaluate correct programs when
  they use the `volatile` keyword (which is only used as an attempt to
  prevent dead store elimination for memory wipes).  The `-cc
  -Dvolatile=` option works around that bug by ignoring `volatile`
  altogether.

[Frama-c]:https://frama-c.com/
[TIS]: https://trust-in-soft.com/tis-interpreter/


Customisation
-------------

Monocypher has optional compatibility with Ed25519. To have that, add
`monocypher-ed25519.h` and `monocypher-ed25519.c` provided in
`src/optional` to your project.  If you compile or install Monocypher
with the makefile, they will be automatically included.

Monocypher also has the `BLAKE2_NO_UNROLLING` preprocessor flag, which
is activated by compiling monocypher.c with the `-DBLAKE2_NO_UNROLLING`
option.

The `-DBLAKE2_NO_UNROLLING` option is a performance tweak.  By default,
Monocypher unrolls the BLAKE2b inner loop, because doing so is over 25%
faster on modern processors.  Some embedded processors however, run the
unrolled loop _slower_ (possibly because of the cost of fetching 5KB of
additional code).  If you're using an embedded platform, try this
option.  The binary will be about 5KB smaller, and in some cases faster.

The `MONOCYPHER_CPP_NAMESPACE` preprocessor definition allows C++ users
who compile Monocypher as C++ to wrap it in a namespace. When it is not
defined (the default), we assume Monocypher is compiled as C, and an
`extern "C"` declaration is added when we detect that the header is
included in C++ code.

The `change-prefix.sh` script can rename all functions by replacing
`crypto_` by a chosen prefix, so you can avoid name clashes. For
instance, the following command changes all instances of `crypto_` by
`foobar_` (note the absence of the underscore):

    ./change-prefix.sh foobar
