Designers
---------

- **ChaCha20:** Daniel J. Bernstein.
- **Poly1305:** Daniel J. Bernstein.
- **BLAKE2:**   Jean-Philippe Aumasson, Christian Winnerlein, Samuel Neves,
                and Zooko Wilcox-O'Hearn.
- **Argon2:**   Alex Biryukov, Daniel Dinu, and Dmitry Khovratovich.
- **X25519:**   Daniel J. Bernstein.
- **EdDSA:**    Daniel J. Bernstein, Bo-Yin Yang, Niels Duif, Peter
                Schwabe, and Tanja Lange.

Implementors
------------

- **ChaCha20:** Loup Vaillant, implemented from spec.
- **Poly1305:** Loup Vaillant, implemented from spec.
- **BLAKE2b:**  Loup Vaillant, implemented from spec.
- **Argon2i:**  Loup Vaillant, implemented from spec.
- **X25519:**   Daniel J. Bernstein, taken and packaged from SUPERCOP
                ref10.
- **EdDSA:**    Loup Vaillant, with bits and pieces from SUPERCOP ref10.

Test suite
----------

Designed and implemented by Loup Vaillant, using _libsodium_ (by many
authors), and _ed25519-donna_ (by Andrew Moon —floodyberry).

Manual
------

Loup Vaillant, Fabio Scotoni, and Michael Savage.

- Loup Vaillant did a first draft.
- Fabio Scotoni rewrote the manual into proper man pages (and
  substantially changed it in the process).
- Michael Savage did extensive editing and proofreading.

Thanks
------

Fabio Scotoni provided much needed advice about testing, interface,
packaging, and the general direction of the whole project.  He also
redesigned the monocypher.org style sheets.

Mike Pechkin and André Maroneze found bugs in earlier versions of
Monocypher.

Andrew Moon clarified carry propagation in modular arithmetic, and
provided advice and code that significantly simplified and improved
Elligator2 mappings.

Mike Hamburg explained comb algorithms, including the signed
all-bits-set comb described in his 2012 paper, Fast and compact
elliptic-curve cryptography.  This made EdDSA signatures over twice as
fast.

Samuel Lucas found many typos in both the manual and the website.

Jens Alfke added some #ifdefs that enabled Monocypher to compile into
a C++ namespace, preventing symbol collisions with similarly-named
functions in other crypto libraries.
