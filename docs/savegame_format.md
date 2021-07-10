# OpenTTD's Savegame Format

Last updated: 2021-06-15

## Outer container

Savegames for OpenTTD start with an outer container, to contain the compressed data for the rest of the savegame.

`[0..3]` - The first four bytes indicate what compression is used.
In ASCII, these values are possible:

- `OTTD` - Compressed with LZO (deprecated, only really old savegames would use this).
- `OTTN` - No compression.
- `OTTZ` - Compressed with zlib.
- `OTTX` - Compressed with LZMA.

`[4..5]` - The next two bytes indicate which savegame version used.

`[6..7]` - The next two bytes can be ignored, and were only used in really old savegames.

`[8..N]` - Next follows a binary blob which is compressed with the indicated compression algorithm.

The rest of this document talks about this decompressed blob of data.

## Data types

The savegame is written in Big Endian, so when we talk about a 16-bit unsigned integer (`uint16`), we mean it is stored in Big Endian.

The following types are valid:

- `1` - `int8` / `SLE_FILE_I8` -8-bit signed integer
- `2` - `uint8` / `SLE_FILE_U8` - 8-bit unsigned integer
- `3` - `int16` / `SLE_FILE_I16` - 16-bit signed integer
- `4` - `uint16` / `SLE_FILE_U16` - 16-bit unsigned integer
- `5` - `int32` / `SLE_FILE_I32` - 32-bit signed integer
- `6` - `uint32` / `SLE_FILE_U32` - 32-bit unsigned integer
- `7` - `int64` / `SLE_FILE_I64` - 64-bit signed integer
- `8` - `uint64` / `SLE_FILE_U64` - 64-bit unsigned integer
- `9` - `StringID` / `SLE_FILE_STRINGID` - a StringID inside the OpenTTD's string table
- `10` - `str` / `SLE_FILE_STRING` - a string (prefixed with a length-field)
- `11` - `struct` / `SLE_FILE_STRUCT` - a struct

### Gamma value

There is also a field-type called `gamma`.
This is most often used for length-fields, and uses as few bytes as possible to store an integer.
For values <= 127, it uses a single byte.
For values > 127, it uses two bytes and sets the highest bit to high.
For values > 32767, it uses three bytes and sets the two highest bits to high.
And this continues till the value fits.
In a more visual approach:
```
  0xxxxxxx
  10xxxxxx xxxxxxxx
  110xxxxx xxxxxxxx xxxxxxxx
  1110xxxx xxxxxxxx xxxxxxxx xxxxxxxx
  11110--- xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx
```

## Chunks

Savegames for OpenTTD store their data in chunks.
Each chunk contains data for a certain part of the game, for example "Companies", "Vehicles", etc.

`[0..3]` - Each chunk starts with four bytes to indicate the tag.
If the tag is `\x00\x00\x00\x00` it means the end of the savegame is reached.
An example of a valid tag is `PLYR` when looking at it via ASCII, which contains the information of all the companies.

`[4..4]` - Next follows a byte where the lower 4 bits contain the type.
The possible valid types are:

- `0` - `CH_RIFF` - This chunk is a binary blob.
- `1` - `CH_ARRAY` - This chunk is a list of items.
- `2` - `CH_SPARSE_ARRAY` - This chunk is a list of items.
- `3` - `CH_TABLE` - This chunk is self-describing list of items.
- `4` - `CH_SPARSE_TABLE` - This chunk is self-describing list of items.

Now per type the format is (slightly) different.

### CH_RIFF

(since savegame version 295, this chunk type is only used for MAP-chunks, containing bit-information about each tile on the map)

A `CH_RIFF` starts with an `uint24` which together with the upper-bits of the type defines the length of the chunk.
In pseudo-code:

```
type = read uint8
if type == 0
    length = read uint24
    length |= ((type >> 4) << 24)
```

The next `length` bytes are part of the chunk.
What those bytes mean depends on the tag of the chunk; further details per chunk can be found in the source-code.

### CH_ARRAY / CH_SPARSE_ARRAY

(this chunk type is deprecated since savegame version 295 and is no longer in use)

`[0..G1]` - A `CH_ARRAY` / `CH_SPARSE_ARRAY` starts with a `gamma`, indicating the size of the next item plus one.
If this size value is zero, it indicates the end of the list.
This indicates the full length of the next item minus one.
In psuedo-code:

```
loop
    size = read gamma - 1
    if size == -1
        break loop
    read <size> bytes
```

`[]` - For `CH_ARRAY` there is an implicit index.
The loop starts at zero, and every iteration adds one to the index.
For entries in the game that were not allocated, the `size` will be zero.

`[G1+1..G2]` - For `CH_SPARSE_ARRAY` there is an explicit index.
The `gamma` following the size indicates the index.

The content of the item is a binary blob, and similar to `CH_RIFF`, it depends on the tag of the chunk what it means.
Please check the source-code for further details.

### CH_TABLE / CH_SPARSE_TABLE

(this chunk type only exists since savegame version 295)

Both `CH_TABLE` and `CH_SPARSE_TABLE` are very similar to `CH_ARRAY` / `CH_SPARSE_ARRAY` respectively.
The only change is that the chunk starts with a header.
This header describes the chunk in details; with the header you know the meaning of each byte in the binary blob that follows.

`[0..G]` - The header starts with a `gamma` to indicate the size of all the headers in this chunk plus one.
If this size value is zero, it means there is no header, which should never be the case.

Next follows a list of `(type, key)` pairs:

- `[0..0]` - Type of the field.
- `[1..G]` - `gamma` to indicate length of key.
- `[G+1..N]` - Key (in UTF-8) of the field.

If at any point `type` is zero, the list stops (and no `key` follows).

The `type`'s lower 4 bits indicate the data-type (see chapter above).
The `type`'s 5th bit (so `0x10`) indicates if the field is a list, and if this field in every record starts with a `gamma` to indicate how many times the `type` is repeated.

If the `type` indicates either a `struct` or `str`, the `0x10` flag is also always set.

As the savegame format allows (list of) structs in structs, if any `struct` type is found, this header will be followed by a header of that struct.
This nesting of structs is stored depth-first, so given this table:

```
type   | key
-----------------
uint8  | counter
struct | substruct1
struct | substruct2
```

With `substruct1` being like:

```
type   | key
-----------------
uint8  | counter
struct | substruct3
```

The headers will be, in order: `table`, `substruct1`, `substruct3`, `substruct2`, each ending with a `type` is zero field.

After reading all the fields of all the headers, there is a list of records.
To read this, see `CH_ARRAY` / `CH_SPARSE_ARRAY` for details.

As each `type` has a well defined length, you can read the records even without knowing anything about the chunk-tag yourself.

Do remember, that if the `type` had the `0x10` flag active, the field in the record first has a `gamma` to indicate how many times that `type` is repeated.

#### Guidelines for network-compatible patch-packs

For network-compatible patch-packs (client-side patches that can play together with unpatched clients) we advise to prefix the field-name with `__<shortname>` when introducing new fields to an existing chunk.

Example: you have an extra setting called `auto_destroy_rivers` you want to store in the savegame for your patched client called `mypp`.
We advise you to call this setting `__mypp_auto_destroy_rivers` in the settings chunk.

Doing it this way ensures that a savegame created by these patch-packs can still safely be loaded by unpatched clients.
They will simply ignore the field and continue loading the savegame as usual.
The prefix is strongly advised to avoid conflicts with future-settings in an unpatched client or conflicts with other patch-packs.

## Scripts custom data format

Script chunks (`AIPL` and `GSDT`) use `CH_TABLE` chunk type.

At the end of each record there's an `uint8` to indicate if there's custom data (1) or not (0).

There are 6 data types for scripts, called `script-data-type`.
When saving, each `script-data-type` starts with the type marker saved as `uint8` followed by the actual data.
- `0` - `SQSL_INT`:
  - an `int64` with the actual value (`int32` before savegame version 296).
- `1` - `SQSL_STRING`:
  - an `uint8` with the string length.
  - a list of `int8` for the string itself.
- `2` - `SQSL_ARRAY`:
  - each element saved as `script-data-type`.
  - an `SQSL_ARRAY_TABLE_END` (0xFF) marker (`uint8`).
- `3` - `SQSL_TABLE`:
  - for each element:
    - key saved as `script-data-type`.
    - value saved as `script-data-type`.
  - an `SQSL_ARRAY_TABLE_END` (0xFF) marker (`uint8`).
- `4` - `SQSL_BOOL`:
  - an `uint8` with 0 (false) or 1 (true).
- `5` - `SQSL_NULL`:
  - (no data follows)

The first data type is always a `SQSL_TABLE`.
