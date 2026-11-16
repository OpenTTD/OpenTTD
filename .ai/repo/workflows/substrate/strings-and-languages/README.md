---
schema: context/v1
id: fork.substrate.strings-and-languages
kind: context
title: "Strings and languages: english.txt, strgen, StringID and parameters, plurals, genders and cases, translations, text input and currencies"
description: "Read before adding or changing a player-facing string, a string parameter or control code, strgen, a language file, the language loader, a text box, autocompletion or a currency."
status: active
scope: directory
providers: ["*"]
audience: [human, agent]
composition: extend
order: 210
tracks:
  - 'src/lang/**'
  - 'src/strgen/**'
  - 'src/string*'
  - 'src/language.h'
  - 'src/textbuf*'
  - 'src/currency*'
  - 'src/autocompletion.*'
  - 'src/table/strgen_tables.h'
  - 'src/table/control_codes.h'
  - 'src/table/unicode.h'
  - 'src/saveload/strings_sl.cpp'
  - 'src/tests/string_func.cpp'
  - 'docs/eints.md'
---

# Strings and languages

Read at f8b1d52e80.

## Purpose and ownership

Owns the player-facing text pipeline: `src/lang/english.txt` as the only hand-edited source,
the build-time compiler `strgen`, the generated `table/strings.h` (one `StringID` constant per
string) and `lang/*.lng` packs, the runtime loader and formatter (`GetString`, `FormatString`,
plural/gender/case selection), the string utilities (`src/string.cpp`), text input (`Textbuf`,
`AutoCompletion`, `StringFilter`) and the currency table used when money is formatted. NewGRF
text (`src/newgrf_text.cpp`), Game Script text (`src/game/game_text.cpp`) and town names
(`src/townname.cpp`) plug into the same formatter but are owned by
[NewGRF](../newgrf/README.md), [scripting](../scripting/README.md) and
[towns](../towns-industries-stations/README.md). No fork code here:
`git log --oneline upstream/master..HEAD -- src/lang src/strgen 'src/string*' 'src/currency*'`
is empty.

## Entry points

- `src/lang/english.txt` - `NAME :text` lines; `##` pragmas (`##id`, `##plural`, `##isocode`,
  ...) handled by `src/strgen/strgen.cpp:FileStringReader::HandlePragma`.
- `src/strgen/strgen_base.cpp:StringReader::HandleString` - parses one line; the master file
  assigns IDs, a translation only fills `translated` / `translated_cases`.
- `src/strgen/strgen.cpp:HeaderFileWriter` - writes `table/strings.h` (`static constexpr
  StringID STR_X{0x...}` plus `LANGUAGE_PACK_VERSION`); `src/lang/CMakeLists.txt` defines the
  `table_strings` and `language_files` targets.
- `src/table/strgen_tables.h:_cmd_structs` - every `{COMMAND}` usable in a string, with the
  number of parameters it consumes; `src/table/control_codes.h:SCC_ENCODED` starts the `SCC_*`
  codes they compile to.
- `src/strings_func.h:GetString` / `src/strings_func.h:MakeParameters` - format a `StringID`
  with typed `src/strings_type.h:StringParameter` values in the current language.
- `src/strings.cpp:GetEncodedStringWithArgs` - freeze an ID plus parameters into an
  `src/strings_type.h:EncodedString` (news, errors, GS text) that is decoded later per client.
- `src/strings.cpp:GetStringWithArgs` - dispatch by `src/strings_type.h:StringTab` (town names,
  special names, Game Script, NewGRF, language pack) into `src/strings.cpp:FormatString`.
- `src/strings.cpp:InitializeLanguagePacks`, `src/strings.cpp:ReadLanguagePack` - scan
  `lang/` on every search path, pick and load one pack.
- `src/currency_func.h:GetCurrency` - the `src/currency_type.h:CurrencySpec` selected by
  `locale.currency`; `src/strings.cpp:FormatGenericCurrency` applies it.

## Data flow

- Build: `strgen -s src/lang -d generated/table` reads only `english.txt` and writes
  `strings.h` when its content changed (temp file compared, then renamed); `strgen` with a
  translation file writes `lang/<name>.lng`. `strgen` is a host tool: with `HOST_BINARY_DIR` set
  the cross build imports it from `strgen.cmake` (see [build](../build-and-packaging/README.md)).
- ID layout: `StringID = tab << TAB_SIZE_BITS | index` (`src/strings_func.h:MakeStringID`).
  Tabs below `TEXT_TAB_END` come from the language pack; `TEXT_TAB_GAMESCRIPT_START` and
  `TEXT_TAB_NEWGRF_START` are ranges filled at runtime. Within a tab IDs are positional: each
  string takes the next index after the last `##id`.
- Runtime: `InitializeLanguagePacks` keeps every `.lng` whose header passes
  `src/strings.cpp:LanguagePackHeader::IsValid`, then chooses the configured file, else a
  reasonably finished pack matching the OS locale (`GetCurrentLocale("LC_MESSAGES")`), else en_GB.
- Formatting: `FormatString` walks the compiled string, pulling parameters through
  `src/strings_internal.h:StringParameters`; `{P ...}` picks a plural form with
  `src/strings.cpp:DeterminePluralForm`, `{G ...}` a gender, `{STRING.case}` a case.
- Money: amounts are stored in base units; display multiplies by `CurrencySpec::rate`, adds
  prefix/suffix and the separator (`locale.digit_group_separator_currency`, else the
  currency's, else the language's).

## Invariants

- **[verified]** Only `english.txt` defines IDs: a translation string absent from the master is
  a warning and skipped; cases and genders are rejected in the base language
  (`src/strgen/strgen_base.cpp:StringReader::HandleString`, `FileStringReader::HandlePragma`).
- **[verified]** A translated string whose commands do not match the English one is dropped and
  the English text is compiled instead; strgen only warns ("info" in translation mode)
  (`src/strgen/strgen_base.cpp:CheckCommandsMatch`, `src/strgen/strgen.cpp:StrgenWarningI`).
- **[verified]** `LANGUAGE_PACK_VERSION` is a hash over every string's ID, name and counted
  commands (`src/strgen/strgen_base.cpp:StringData::Version`); a `.lng` with another version is
  skipped, and with none left the game stops: "No available language packs (invalid versions?)".
- **[verified]** Strings in the `##id 0x6000` region of `english.txt` are referenced by number in
  savegames ("These strings may never get a new id"); `Station::string_id` is saved as a
  `StringID` and `src/saveload/strings_sl.cpp:RemapOldStringID` maps pre-OpenTTD numbers.
- **[verified]** `SCC_ENCODED*` codes must keep their values: encoded strings are saved
  (comment at `src/table/control_codes.h:SCC_ENCODED`); strgen rejects private-use characters
  U+E000-U+F8FF in source text because control codes live there.
- **[verified]** A parameter read past the end or with a different type than first read throws
  inside `FormatString`, which logs at `misc` Critical and prints "(invalid parameter)"
  (`src/strings.cpp:StringParameters::GetNextParameterReference`).
- **[verified]** A language-pack index beyond the loaded tab is `FatalError` for engine strings
  and `STR_UNDEFINED` only for Game Script strings (`src/strings.cpp:GetStringWithArgs`).
- **[verified]** The currency switch to the euro runs in a yearly calendar timer and writes
  `_settings_game.locale.currency` (`src/currency.cpp:_check_switch_to_euro`), so every peer does
  it on the same date (ADR 0025).
- **[inferred]** Rendered text depends on the local language, locale and currency, so it must
  never feed a command or other game state; keep the `StringID`/`EncodedString` and render on
  display (ADR 0016).

## Extension points

- New string: add `STR_...` to `english.txt` in its thematic block, outside the 0x6000 region;
  use it as `STR_...` after the next build. Never touch other `src/lang/*.txt` files (ADR 0020).
- New parameter/command: a row in `src/table/strgen_tables.h:_cmd_structs` and its `SCC_*` in
  `src/table/control_codes.h` (append, never renumber), handled in `FormatString`.
- New plural form: a row in `src/table/strgen_tables.h:_plural_forms` and a `case` in
  `src/strings.cpp:DeterminePluralForm` (its `default` is `NOT_REACHED`).
- New currency: a row in `src/currency.cpp:_origin_currency_specs`, a value in
  `src/currency_type.h:Currency` before `Custom`, and its `STR_GAME_OPTIONS_CURRENCY_*`.
- Text input: subclass `src/autocompletion.h:AutoCompletion` (console and chat do); a
  platform word/character iterator comes from `src/string_base.h:StringIterator::Create`.

## Traps and forbidden operations

- Never hand-edit translations or the generated `table/strings.h`; eints owns `src/lang/*.txt`
  except `english.txt` (`docs/eints.md`). Changing parameters in English makes translations
  "invalid" until translators fix them; their warnings are expected.
- Adding, removing or renaming a string, or changing its parameters, changes
  `LANGUAGE_PACK_VERSION`: a binary run with an older `lang/` directory (copied, or found on
  another search path) silently skips those packs.
- The 0x6000 region is append-only by position, not by name: inserting a line shifts every later
  ID there and breaks savegames ([saveload](../saveload/README.md)).
- `{STRINGn}` consumes the sub-string ID plus n parameters; a wrong n shifts every later
  parameter of the outer string.
- With no language in the config, the pack follows the OS locale, so a headless run on a
  non-English machine can print translated text. Pin `language = english.lng` under `[misc]`
  in the run's config (`src/table/settings/misc_settings.ini`) when output is parsed.
- `GetCurrency()` reads `GetGameSettings()`: in the menu that is `_settings_newgame`. Convert
  user-entered money back with `/ GetCurrency().rate`, as `src/settings_gui.cpp` does.

## Tests and validation

- Catch2: `src/tests/string_func.cpp` (case-insensitive compare, natural sort, validation),
  plus `src/tests/string_builder.cpp`, `src/tests/string_consumer.cpp`, `src/tests/utf8.cpp`;
  run `ctest -R 'Str|Utf8'` or `tools/gate.sh smoke -R Str`.
- The build is the strgen check: `cmake --build build --target table_strings language_files`
  fails on master-file errors; translation problems are only warnings.
- Regression: `regression_ai`/`regression_game` print through script string APIs.
- Tier: `tools/gate.sh change`. Debug: `-d misc=3` lists search paths and every `.lng` rejected
  as invalid; formatting errors are logged at `misc` Critical regardless of level.

## Decisions, rules and knowledge

- ADR 0020 (text compiled from english.txt by strgen, translations via eints), ADR 0018
  (saved string IDs), ADR 0016 (no locale-dependent state), ADR 0025 (euro switch timer).
- Rules: `project.savegame-fields-are-version-gated`, `project.never-report-unrun-validation-as-pass`.
- Knowledge: `the-savegame-string-range-never-gets-new-ids`,
  `a-language-pack-must-match-the-binarys-english-txt`.
- Upstream docs: `docs/eints.md`, `CODINGSTYLE.md`.

## Typical tasks

- To add a message: add `STR_...` to `english.txt`, format with `GetString(STR_..., args...)`
  or keep `GetEncodedString(STR_..., args...)` if it is stored; validate with `tools/gate.sh change`.
- To change a string's parameters: edit English only, update every caller's argument list,
  grep `git grep -n STR_X -- src` (including script API docs); validate with `tools/gate.sh change`.
- To add a currency or plural form: follow Extension points, check the option list in the
  game options window, validate with `tools/gate.sh change`.
- To debug "(invalid parameter)" output: count the commands in the English string against the
  caller's `MakeParameters`, remembering each `{STRINGn}` consumes n+1.
