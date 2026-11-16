---
schema: context/v1
id: fork.substrate.core-utilities
kind: context
title: "Core utilities: pools, strong IDs, enum bitsets, bit and integer math, random, strings, containers, debug output, safeguards and threads"
description: "Read before changing src/core/ or src/misc/, adding a pool or strong ID type, drawing random numbers, adding Debug() output, starting a thread, or including a C function safeguards.h bans."
status: active
scope: directory
providers: ["*"]
audience: [human, agent]
composition: extend
order: 210
tracks:
  - 'src/core/**'
  - 'src/misc/**'
  - 'src/stdafx.h'
  - 'src/safeguards.h'
  - 'src/debug*'
  - 'src/thread.h'
---

# Core utilities

Read at f8b1d52e80.

## Purpose and ownership

`src/core/` holds generic building blocks with no game-domain knowledge (pools, ADR 0017;
strong types, bitsets, math, random, strings, containers). `src/misc/` holds helpers grown
around specific users: heap and hash table under YAPF, LRU cache under text layout and OpenGL,
`history` statistics, endian buffers for command packing, command-line parsing. `src/stdafx.h`
is included first by every translation unit, `src/safeguards.h` last; `src/debug.h` is logging,
`src/thread.h` the only way to start a thread. No fork hunk
(`git log --oneline upstream/master..HEAD -- src/core src/misc` is empty).

## Entry points

- `src/core/pool_type.hpp:Pool` and `src/core/pool_type.hpp:PoolItem` - typed slot pools;
  `src/core/pool_type.hpp:PoolID` the strong index type (`Begin`, `End`, `Invalid`, `base`).
  Out-of-line methods in `src/core/pool_func.hpp`, instantiated per pool by
  `INSTANTIATE_POOL_METHODS(Name)` in the owning `.cpp` (e.g. `src/town_cmd.cpp`).
- `src/core/pool_func.cpp:PoolBase::Clean` - wipes every pool of the given `PoolType`s; called
  from `src/misc.cpp:InitializeGame` (Normal), `src/openttd.cpp` (all) and the network code.
- `src/core/random_func.hpp:Random`, `src/core/random_func.hpp:RandomRange`,
  `src/core/random_func.hpp:Chance16` (game state, `_random`) versus
  `src/core/random_func.hpp:InteractiveRandom` and `InteractiveRandomRange` (everything else);
  `src/core/random_func.cpp:RandomBytesWithFallback` for OS entropy (tokens, UIDs).
- `src/core/enum_type.hpp:EnumBitSet`, `EnumIndexArray`, `EnumRange`, `to_underlying`;
  `src/core/base_bitset_type.hpp:BaseBitSet` (`Test`, `Set`, `Reset`);
  `src/core/strong_typedef_type.hpp:Typedef` (`Compare`/`Integer`/`Compatible` mixins, e.g.
  `StringID`); `src/core/overflowsafe_type.hpp:OverflowSafeInt` (`Money`, `src/economy_type.h`).
- `src/core/bitmath_func.hpp:GB`, `SB`, `HasBit`, `SetBit`, `FindFirstBit`,
  `src/core/bitmath_func.hpp:SetBitIterator`; `src/core/math_func.hpp:Clamp`, `ClampTo`,
  `CeilDiv`, `DivideApprox`, `IsInsideMM`.
- Strings: `src/core/string_builder.hpp:StringBuilder`, `src/core/string_consumer.hpp:StringConsumer`
  (`TryRead*`/`Read*`/`Peek*`, `ParseIntegerBase`), `src/core/string_inplace.hpp:InPlaceReplacement`,
  `src/core/utf8.hpp:Utf8View`; `src/core/format.hpp` makes enums and strong types formattable.
- Scope helpers: `src/core/backup_type.hpp:Backup` (`Restore`/`Trash`) and
  `src/core/backup_type.hpp:AutoRestoreBackup`, e.g. around `_current_company`.
- Containers: `src/core/flatset_type.hpp:FlatSet`, `src/core/multimap.hpp:MultiMap`,
  `src/core/kdtree.hpp:Kdtree` (`Build`, `Insert`, `Remove`, `FindNearest`, `FindContained`),
  `src/misc/binaryheap.hpp:CBinaryHeapT`, `src/misc/hashtable.hpp:HashTable`,
  `src/misc/lrucache.hpp:LRUCache`, `src/misc/endian_buffer.hpp:EndianBufferWriter`.
- `src/debug.h:Debug` (macro), `src/debug_type.h:Facility`, `src/debug_type.h:Severity`,
  `src/debug.cpp:SetDebugString`, `src/debug.h:TicToc`; `src/thread.h:StartNewThread`.

## Lifecycle

- Pool item: `T::CanAllocateItem(n)` -> `T::Create(args...)` (lowest free index, found through
  the used-bitmap from `first_free`) -> use by ID (`T::Get`, `T::GetIfValid`, `T::IsValidID`) ->
  `delete item` (`operator delete` hands the slot back via `FreeItem`, then
  `Titem::PostDestructor(index)`). Loading recreates items at their saved index with
  `PoolItem::CreateAtIndex`, which raises `SlErrorCorruptFmt` on an out-of-range or occupied
  index. `CleanPool` deletes every item with `cleaning` set and skips `PostDestructor`.
- Random: `src/core/random_func.cpp:SetRandomSeed` seeds both generators (interactive with
  `seed * 0x1234567`); world generation reseeds `_random` from `generation_seed`
  (`src/genworld.cpp`); `src/saveload/misc_sl.cpp` saves `_random.state` (not the interactive one),
  so a joining client inherits the stream with the map.
- Debug output: `-d <spec>` on the command line or console `debug_level <spec>` ->
  `SetDebugString` parses `misc=3,net=4` or a bare global level into `_debug_level`; `Debug()`
  formats only when the level is visible; `DebugPrint` writes to stderr, or for
  `Facility::Desync` (non-critical) to `commands-out.log` in the autosave directory, and queues a
  copy for the admin port / console when `developer >= 2` or an admin subscribed.

## Invariants

- **[verified]** In an assert build every `Create` must be preceded by `CanAllocateItem` for the
  same pool: `CanAllocate` sets `checked = n`, `GetNew(size)` asserts `checked != 0` and decrements
  it (`src/core/pool_func.hpp`). `CreateAtIndex` (loading) is not checked. Without asserts a full
  pool is a `FatalError("<name>: no more free items")`.
- **[verified]** Slot reuse is lowest-index-first (`FindFirstFree` from `first_free`), so the same
  create/delete sequence yields the same IDs and the same `Iterate()` order on every peer
  (ADR 0016, ADR 0017).
- **[verified]** `PoolItem::Get` asserts `index < first_unused` but returns `nullptr` for a freed
  slot; `GetIfValid` returns `nullptr` for both. Raw `new`/placement-new of a pool item are deleted
  operators.
- **[verified]** Pool iteration re-reads `GetPoolSize()` and validity each step
  (`src/core/pool_type.hpp:PoolIterator`); **[inferred]** deleting the current item is safe, and
  items created during the loop may be visited.
- **[verified]** `Random()` draws from `_random` (shared, compared across peers for desyncs);
  `InteractiveRandom()` from `_interactive_random` (local). With `RANDOM_DEBUG` defined by hand
  (no CMake option) every networked `Random()` call logs date, frame, company and call site to
  `random-out.log` (`src/core/random_func.cpp`, `src/debug.cpp:DebugPrint`).
- **[verified]** `Severity::Critical` messages always print; a facility at level N shows every
  severity whose enum value is at most N (0 Critical ... 4 Info, 5-6 Debug, 7-9 Trace) (`src/debug.h:IsVisibleSeverity`).
- **[verified]** `OverflowSafeInt` saturates instead of wrapping. `Backup`'s destructor logs "Backed-up value was not restored!" and restores; it
  does not assert, because world-generation aborts unwind through it (`src/core/backup_type.hpp`).
- **[verified]** `OPTION_USE_ASSERTS` (default ON) defines `WITH_ASSERT`, else `NDEBUG`
  (`cmake/Options.cmake`); in an `NDEBUG` + `WITH_ASSERT` build `assert` is redirected to
  `AssertFailedError` (`src/stdafx.h`).
- **[supported]** `Kdtree` shape can differ between clients; game code must not depend on
  `FindContained` order. `FindNearest` breaks distance ties by `T`'s `<` and `NOT_REACHED`s on a
  duplicate element (`src/core/kdtree.hpp`).

## Extension points

- New pool: `using FooPool = Pool<Foo, FooID, growth>; extern FooPool _foo_pool;`, a
  `PoolID<base, struct FooIDTag, End, Invalid>` ID type, `struct Foo : FooPool::PoolItem<&_foo_pool>`,
  then `FooPool _foo_pool("Foo"); INSTANTIATE_POOL_METHODS(Foo)` in one `.cpp`. Use
  `PoolType::Data` if it must survive `PoolBase::Clean(PoolType::Normal)`. A saved pool also needs a
  chunk handler ([saveload](../saveload/README.md)).
- New debug facility: add a value before `End` in `src/debug_type.h:Facility` and its name, in the
  same order, in `_debug_facilities` in `src/debug.cpp`.
- New enum flag set: an `enum class` plus `using FooFlags = EnumBitSet<Foo, uint8_t>;`; upstream
  has been converting `DECLARE_ENUM_AS_BIT_SET` enums to this form (`git log --oneline
  --grep=EnumBitSet`).

## Traps and forbidden operations

- `src/safeguards.h` `#define`s `malloc`, `memcpy`, `memset`, `strcmp`, `sprintf`, `printf`,
  `atoi`/`strto*`/`sto*`, `to_string` and others to `SAFEGUARD_DO_NOT_USE_THIS_METHOD`. Include it
  last in every `.cpp`; anything included after it that uses those names fails to compile. Use
  `std::copy_n`, `fmt::format`, `StringConsumer`/`ParseInteger`, `std::string` instead.
- `Random()` in code that does not run identically on every peer (GUI, sound, test passes, local
  settings) desyncs the game: draw `InteractiveRandom()` in the GUI and pass the value as a
  command argument (`src/industry_gui.cpp` passes a seed to `Commands::BuildIndustry`). Script
  randomness uses `ScriptObject::GetRandomizer` per company, not `_random`.
- `src/core/string_*.cpp`, `src/core/utf8.cpp` and `src/misc/getoptdata.cpp` are also compiled into
  the host tools `strgen` and `settingsgen` (`src/strgen/CMakeLists.txt`,
  `src/settingsgen/CMakeLists.txt`) with `STRGEN`/`SETTINGSGEN` defined; game headers there must
  be behind those guards, as `src/core/string_consumer.cpp:StringConsumer::LogError` does.
- `Backup` must be `Restore`d or `Trash`ed exactly once; prefer `AutoRestoreBackup`.
- An item destructor that touches other objects must return early on `CleaningPool()`
  (`src/company_cmd.cpp`, `src/depot.cpp`): during `CleanPool` the other pools may already be gone.
- `src/thread.h:StartNewThread` returns false when threads are unavailable; every caller must
  fall back to running the work inline (`src/saveload/saveload.cpp`,
  `src/linkgraph/linkgraphjob.cpp:LinkGraphJob::SpawnThread`). Game state belongs to the game thread.

## Tests and validation

- Catch2 cases under `src/tests/` cover these files directly: `src/tests/bitmath_func.cpp`
  (`SetBitIterator tests`), `src/tests/math_func.cpp`, `src/tests/enum_over_optimisation.cpp`,
  `src/tests/flatset_type.cpp`, `src/tests/string_builder.cpp`, `src/tests/string_consumer.cpp`,
  `src/tests/string_inplace.cpp`, `src/tests/utf8.cpp`, `src/tests/history_func.cpp`,
  `src/tests/alternating_iterator.cpp`. Pools, random and the k-d tree have no unit test; they are
  exercised by `regression_*` and play. See [unit tests](../unit-tests/README.md).
- Tier: `tools/gate.sh change`; `tools/gate.sh smoke -R 'StringConsumer|SetBitIterator'` while
  iterating; a `src/stdafx.h` change rebuilds everything. Debug: `-d misc=<n>`; `-d desync=1` and a
  hand-defined `RANDOM_DEBUG` for random-stream divergence; `-d yapf=<n>` for the heap/hash users.

## Decisions, rules and knowledge

- ADR 0017 (typed pools and strong IDs), ADR 0016 (deterministic lockstep), ADR 0027 (CMake,
  `add_files`).
- Rules: `project.game-state-mutates-only-through-commands`, `project.never-report-unrun-validation-as-pass`.
- Knowledge: `a-command-test-pass-must-not-draw-random`, `evaluate-random-before-client-settings`,
  `the-desync-check-compares-only-the-game-random-state`,
  `a-pool-create-needs-can-allocate-item-first`, `core-string-files-also-build-into-strgen-and-settingsgen`.
- Neighbours: [commands](../commands/README.md), [core loop](../core-loop/README.md),
  [vehicles and pathfinding](../vehicles-and-pathfinding/README.md) (YAPF on `CBinaryHeapT`/`HashTable`),
  [saveload](../saveload/README.md), [third-party code](../third-party/README.md).

## Typical tasks

- To add a pooled object type: copy the `TownPool` pattern (`src/town.h`, `src/town_cmd.cpp`),
  check `CanAllocateItem` in the command's test pass, add a chunk handler, validate with
  `tools/gate.sh change`.
- To add logging: `Debug(Facility::X, Severity::Info, "{}", value)` (arguments are evaluated only
  when visible); run with `-d x=4`.
- To use randomness in a new mechanic: `Random()`/`RandomRange()` only in the execute pass or the
  game loop; `InteractiveRandom()` for anything local.
- To parse text or numbers: `StringConsumer` / `ParseInteger`, never `atoi`/`sscanf`; add a case to
  `src/tests/string_consumer.cpp`.
