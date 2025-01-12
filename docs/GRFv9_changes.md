## GRFv9 changes from GRFv8.

* ALL existing Extended Byte fields are now Word fields.
* Action 00:
  * `num-info` is now a Word.
  * Feature 00: Trains
    * Property 05 `tracktype` is now a Word.
    * Property 15 `cargotype` is now a Word.
    * Property 1D `refitmask` is **removed**.
    * Property 2C/2D `cttinclude`/`cttexclude` are now a Word followed by a list of Words.
  * Feature 01: Road vehicles
    * Property 05 `roadtramtype` is now a Word.
    * Property 10 `cargotype` is now a Word.
    * Property 12 `sfx` is now a Word.
    * Property 16 `refitmask` is **removed**.
    * Property 24/25 `cttinclude`/`cttexclude` are now a Word followed by a list of Words.
  * Feature 02: Ships
    * Property 0C `cargotype` is now a Word.
    * Property 10 `sfx` is now a Word.
    * Property 11 `refitmask` is **removed**.
    * Property 1E/1F `cttinclude`/`cttexclude` are now a Word followed by a list of Words.
  * Feature 03: Aircraft
    * Property 12 `sfx` is now a Word.
    * Property 13 `refitmask` is **removed**.
    * Property 1D/1E `cttinclude`/`cttexclude` are now a Word followed by a list of Words.
  * Feature 07: Houses
    * Properties 0D, 0E, 0F and 1E are **removed**.
    * Property 20 `watched cargoes` is now a Word followed by a list of Words.
    * Property 23 `accepted cargoes` is now a Word followed by a list of (Word, Byte).
  * Feature 09: Industry tiles
    * Properties 0A, 0B and 0C are **removed**.
    * Property 13 `cargo acceptance` is now a Word followed by a list of (Word, Byte).
  * Feature 0A: Industries
    * Properties 10, 11, 12, 13 are **removed**.
    * Property 15 `sfx` is now a Word followed by a list of Words.
    * Properties 1C, 1D and 1E are **removed**.
    * Properties 25/16 `producedcargoes`/`acceptedcargoes` are now a Word followed by a list of Words.
    * Property 27 `productionrates` is now a Word follow by a list of Bytes.
  * Feature 10: Railtypes
    * Properties 0E, 0F, 18, 19 and 1D are now a Word followed by a list of DWords.
  * Feature 12/13: Roadtypes and Tramtypes
    * Properties 0F, 18, 19 and 1D are now a Word followed by a list of DWords.
* Action 02:
  * `set-id` is now a Word. Maximum set-id is now 7FFF instead of FF.
  * `subroutine` is now a Word.
  * `parameter` is now a **DWord**.
  * VariationalAction2 `nvar` is now a Word.
  * RandomAction2 `nrand` is now a Word.
* Action 03:
  * `n-id` is now a Word.
  * `ids...` are each now a Word.
  * `num-cid` is now a Word.
  * `cargo-type` is now a Word.
* Action 04:
  * `num-ent` is now a Word.
  * `offset` is now always a Word, instead of varying depending on feature/flags.
* Action 07/09:
  * `num-sprites` is now a Word. If bit 15 is set, then this is a label instead of the number of sprites.
* Action 0A:
  * `num-sets` is now a Word.
  * `num-sprites` is now a Word.
* Action 0E:
  * `num` is now a Word.
* Action 10:
  * `label` is now a Word. Labels MUST have bit 15 set. This means there is no longer any conflict with labels overlapping numbers.
* Action 12:
  * `num-def` is now a Word.
  * `num-char` is now a Word.
  * `base-char` is now a **DWord**. This allows custom glyphs between 0x10000 and 0x1FFFFD to be defined.
* Action 13:
  * `num-ent` is now a Word.
