---
schema: context/v1
id: fork.substrate.third-party
kind: context
title: "Third-party code: vendored fmt, nlohmann json, monocypher, Catch2, md5, ICU scriptrun, OpenGL headers and the social integration API, their licences and build"
description: "Read before editing, upgrading or newly including anything under src/3rdparty/ (except Squirrel), or before adding a vendored library."
status: active
scope: directory
providers: ["*"]
audience: [human, agent]
composition: extend
order: 210
tracks:
  - 'src/3rdparty/CMakeLists.txt'
  - 'src/3rdparty/README.licensing'
  - 'src/3rdparty/fmt/**'
  - 'src/3rdparty/nlohmann/**'
  - 'src/3rdparty/monocypher/**'
  - 'src/3rdparty/catch2/**'
  - 'src/3rdparty/md5/**'
  - 'src/3rdparty/icu/**'
  - 'src/3rdparty/opengl/**'
  - 'src/3rdparty/openttd_social_integration_api/**'
  - 'cmake/3rdparty/**'
---

# Third-party code

Read at f8b1d52e80.

## Purpose and ownership

`src/3rdparty/` holds libraries upstream vendors instead of finding them on the system, so every
platform builds the same version. Squirrel (`src/3rdparty/squirrel/`) is owned by the
[scripting](../scripting/README.md) map. Upstream owns all of these directories; the fork has no
hunk in them (`git log --oneline upstream/master..HEAD -- src/3rdparty cmake/3rdparty` is empty)
and treats them as read-only.

| Directory | What, version | Licence (file) | Main users |
|---|---|---|---|
| `src/3rdparty/fmt/` | {fmt}, `FMT_VERSION` in `src/3rdparty/fmt/base.h` | MIT (`src/3rdparty/fmt/LICENSE.rst`) | everything, via `src/core/format.hpp` and `src/debug.h:Debug` |
| `src/3rdparty/nlohmann/` | nlohmann/json single header, `NLOHMANN_JSON_VERSION_*` in `src/3rdparty/nlohmann/json.hpp` | MIT (`src/3rdparty/nlohmann/LICENSE.MIT`) | survey and crash log, `src/signature.cpp`, Game Script admin JSON, **[fork]** `src/prismatic_mcp/` |
| `src/3rdparty/monocypher/` | Monocypher (`src/3rdparty/monocypher/CHANGELOG.md` heads with the version) | BSD-2 / CC-0 (`src/3rdparty/monocypher/LICENCE.md`) | network authentication and encryption (`src/network/network_crypto.cpp`), `src/signature.cpp`, `src/misc.cpp:GenerateUid` |
| `src/3rdparty/catch2/` | Catch2 v2 single header, `CATCH_VERSION_*` in `src/3rdparty/catch2/catch.hpp` | BSL-1.0 (`src/3rdparty/catch2/LICENSE.txt`) | `src/tests/` only |
| `src/3rdparty/md5/` | L. Peter Deutsch's MD5, adapted (`src/3rdparty/md5/md5.h:Md5`, `MD5Hash`) | Zlib (in the source comments) | base set, NewGRF and script checksums, content download |
| `src/3rdparty/icu/` | ICU's `ScriptRun` sample (`src/3rdparty/icu/scriptrun.h`) | Unicode (`src/3rdparty/icu/LICENSE`) | `src/gfx_layout_icu.cpp` |
| `src/3rdparty/opengl/` | Khronos `glext.h`, `khrplatform.h`, `wglext.h` | MIT (`src/3rdparty/opengl/khrplatform.h`) | `src/video/opengl.cpp`, `src/video/win32_v.cpp` |
| `src/3rdparty/openttd_social_integration_api/` | OpenTTD's own plugin ABI headers | MIT (`src/3rdparty/openttd_social_integration_api/LICENSE`) | `src/social_integration.cpp` |
| `cmake/3rdparty/llvm/` | LLVM `CheckAtomic.cmake` | Apache-2.0 (`cmake/3rdparty/llvm/LICENSE.txt`) | top-level `CMakeLists.txt` |

Measure a version instead of trusting this table: `grep -m1 'define FMT_VERSION'
src/3rdparty/fmt/base.h`, `git log --format='%h %ad %s' --date=short -3 -- src/3rdparty/<dir>`.

## Entry points

- `src/3rdparty/CMakeLists.txt` - `add_subdirectory` per library; each `CMakeLists.txt` below it
  lists its files with `add_files` (`cmake/SourceList.cmake`), so they compile into `openttd_lib`.
- Conditions: `src/3rdparty/icu/CMakeLists.txt` only with `ICU_I18N_FOUND`;
  `src/3rdparty/opengl/CMakeLists.txt` only with `OPENGL_FOUND` (`wglext.h` also needs `WIN32`).
- `src/3rdparty/fmt/format.cc` and `src/3rdparty/fmt/os.cc` are the compiled part of fmt;
  `format.cc` is built with `SKIP_PRECOMPILE_HEADERS` (top-level `CMakeLists.txt`) and is also
  listed directly by `src/strgen/CMakeLists.txt` and `src/settingsgen/CMakeLists.txt`.
- Licensing index: `src/3rdparty/README.licensing` points to the "Licensing" section of
  `README.md`, which names each library and its licence file.

## Data flow

- Formatting: `fmt::format` / `format_append` everywhere; `src/core/format.hpp` adds
  `fmt::formatter` specialisations for enums and strong types; `src/stdafx.h` and
  `src/core/format.hpp` are the precompiled headers of `openttd_lib`.
- JSON: `src/stdafx.h` defines `JSON_ASSERT(x)` as OpenTTD's `assert` before nlohmann is included,
  so a JSON contract violation hits the game's assert handler, not `<cassert>` directly.
- Crypto: monocypher primitives (`crypto_x25519`, `crypto_aead_*`, `crypto_blake2b_*`,
  `crypto_ed25519_check`) are wrapped by `src/network/network_crypto.cpp` and
  `src/signature.cpp`; nothing else should call them directly. Entropy comes from
  `src/core/random_func.cpp:RandomBytesWithFallback`, never `_random`.

## Invariants

- **[verified]** Duplicate file names across `add_files` calls are a CMake `FATAL_ERROR`, except a
  header inside `3rdparty` (`cmake/SourceList.cmake:_add_files_tgt`).
- **[verified]** `src/3rdparty/md5/md5.cpp` includes `stdafx.h`, `core/endian_func.hpp` and
  `safeguards.h`: it is adapted to OpenTTD's conventions. fmt, monocypher and ICU sources include
  neither, so `src/safeguards.h` bans do not apply inside them.
- **[verified]** Upstream does make small style edits inside vendored code (e.g. catch2 and md5
  commits in `git log -- src/3rdparty/catch2 src/3rdparty/md5`); library upgrades are separate
  "Update:"/"Codechange: Upgrade" commits.
- **[supported]** "The files in this directory are not licensed under the same terms as the rest
  of OpenTTD" (`src/3rdparty/README.licensing`); the rest is GPL-2.0 (`COPYING.md`).

## Extension points

- Upgrade a library: replace its files wholesale from the upstream release, keep the
  `CMakeLists.txt` file list in step, update the Licensing section of `README.md` if the licence
  file name changes, build with `tools/gate.sh full`.
- New library: a new directory with its licence file and `CMakeLists.txt`, an `add_subdirectory`
  line in `src/3rdparty/CMakeLists.txt`, a Licensing entry in `README.md`; the licence must be
  compatible with OpenTTD's GPL-2.0. **[fork]** research-only users add the include behind the
  research guard, as every `src/prismatic_mcp/` include in upstream files is.

## Traps and forbidden operations

- Do not edit vendored code to fix an OpenTTD problem; wrap it in OpenTTD code. A deliberate local
  patch must be its own commit that names the library and the reason, or the next upstream upgrade
  silently drops it. The fork follows upstream by rebase (ADR 0011): any fork hunk here conflicts on
  every library upgrade.
- `README.md`'s Licensing section names the monocypher licence file with a US spelling; the
  tracked file is `src/3rdparty/monocypher/LICENCE.md`.
- `nlohmann/json.hpp` is very large; including it in a widely included header lengthens every
  build. **[fork]** It reaches upstream files only through `src/prismatic_mcp/` headers that
  `src/console_cmds.cpp` and `src/dropdown.cpp` include under `#ifdef OTTD_PRISMATIC_MCP_SERVER`,
  and `src/prismatic_mcp/CMakeLists.txt` adds those sources only with
  `OPTION_PRISMATIC_MCP_SERVER` ([prismatic-mcp](../prismatic-mcp/README.md)).
- `catch.hpp` is listed in `openttd_lib` but only `src/tests/test_main.cpp` defines
  `CATCH_CONFIG_MAIN`; include it from test files only.
- Exceptions from nlohmann (`json::parse`, `at`, a wrong `get<T>`) escape into game code unless
  caught. **[fork]** `src/prismatic_mcp/prismatic_mcp_server.cpp` catches `json::exception` around
  `json::parse` of the request body and answers a JSON-RPC parse error; keep every new parse of
  untrusted input inside such a boundary.

## Tests and validation

- `src/3rdparty/icu/tests/test_srtest.cpp` (added to `openttd_test` only with ICU i18n);
  `src/tests/test_network_crypto.cpp` exercises monocypher through the network authentication
  handlers; `src/tests/test_script_admin.cpp` the Squirrel<->JSON conversion over nlohmann.
- Tier: an upgrade or a new library is `tools/gate.sh full` (clean configure); a change that only
  includes an existing header is `tools/gate.sh change`.
- Debug: `-d net=<n>` for crypto/authentication failures, `-d misc=<n>` for
  `RandomBytesWithFallback` reporting a missing strong RNG.

## Decisions, rules and knowledge

- ADR 0027 (CMake only; `add_files` per directory), ADR 0011 (follow upstream by rebase),
  ADR 0001 (no upstream interaction).
- Rules: `project.no-upstream-interaction`, `project.research-code-is-gated-and-labelled`.
- Neighbours: [core utilities](../core-utilities/README.md), [network](../network/README.md),
  [unit tests](../unit-tests/README.md), [scripting](../scripting/README.md) (Squirrel).

## Typical tasks

- To find which version is vendored: read the version macro or the newest `git log` subject for
  the directory, as above.
- To use JSON in new fork code: include `src/3rdparty/nlohmann/json.hpp` from a `.cpp` behind the
  research guard, catch `nlohmann::json::exception` at the input boundary, validate with
  `tools/gate.sh research`.
- To hash or sign: call `src/signature.cpp` or `src/network/network_crypto.cpp` helpers, not raw
  `crypto_*` functions.
