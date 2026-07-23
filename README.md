# OpenTTD

> **Private fork notice:** `korczis/OpenTTD` is not the official OpenTTD project and is not staged for upstream contribution. See [0.0) About this fork](#00-about-this-fork) below for what it actually is.

## Table of contents

- 0.0) [About this fork](#00-about-this-fork)
    - 0.1) [Purpose](#01-purpose)
    - 0.2) [What this fork is not](#02-what-this-fork-is-not)
    - 0.3) [What this fork adds on top of upstream OpenTTD](#03-what-this-fork-adds-on-top-of-upstream-openttd)
    - 0.4) [Validation / gating model](#04-validation--gating-model)
    - 0.5) [Ground rules (short version)](#05-ground-rules-short-version)
    - 0.6) [Relationship to upstream OpenTTD](#06-relationship-to-upstream-openttd)
- 1.0) [About](#10-about)
    - 1.1) [Downloading OpenTTD](#11-downloading-openttd)
    - 1.2) [OpenTTD gameplay manual](#12-openttd-gameplay-manual)
    - 1.3) [Supported platforms](#13-supported-platforms)
    - 1.4) [Installing and running OpenTTD](#14-installing-and-running-openttd)
    - 1.5) [Add-on content / mods](#15-add-on-content--mods)
    - 1.6) [OpenTTD directories](#16-openttd-directories)
    - 1.7) [Compiling OpenTTD](#17-compiling-openttd)
- 2.0) [Contact and community](#20-contact-and-community)
    - 2.1) [Multiplayer games](#21-multiplayer-games)
    - 2.2) [Contributing to OpenTTD](#22-contributing-to-openttd)
    - 2.3) [Reporting bugs](#23-reporting-bugs)
    - 2.4) [Translating](#24-translating)
- 3.0) [Licensing](#30-licensing)
- 4.0) [Credits](#40-credits)

## 0.0) About this fork

`korczis/OpenTTD` is a **private research fork**, not the official OpenTTD project and not staged for upstream contribution. It is owned by korczis and used as a real, complex, long-lived C++ codebase to validate the **Prismatic platform** — a research effort evaluating coding-agent workflows: how agents plan and scope changes, how gating and reproducibility hold up in practice, and how agent-driven engineering behaves against a substrate too large and too convention-heavy to fake.

Sections 1.0 onward below are the **original upstream OpenTTD README**, kept intact for its technical value and attribution. This section only documents what's specific to this fork.

### 0.1) Purpose

OpenTTD plays two roles at once in this repository:

- **System under test.** `src/`, `regression/`, the build system, and OpenTTD's own engineering conventions (savegame compatibility, the deterministic command-pattern architecture, `CODINGSTYLE.md`) are the real substrate an agent has to actually respect to make a correct, working change — that's what makes it useful as a validation target instead of a toy.
- **Not a product being developed here.** Nothing in this fork is intended to become an OpenTTD feature or fix. Changes exist to produce evidence about *how the change was made and validated*, not to ship gameplay value.

### 0.2) What this fork is not

- Not affiliated with, and not a contribution channel to, the official [OpenTTD/OpenTTD](https://github.com/OpenTTD/OpenTTD) project.
- Not a place where issues or pull requests get opened upstream — everything stays local to this fork.
- Not bound, as a *local* rule, by upstream's own contribution process (`CONTRIBUTING.md`, PR templates, commit-message grammar, "ask before large changes"). That process describes upstream's repository, not this one — it's kept here only as background technical reference.

### 0.3) What this fork adds on top of upstream OpenTTD

| Path | Purpose |
|---|---|
| [`AGENTS.md`](./AGENTS.md) | Vendor-neutral ground rules for any coding agent working here: the four kinds of change, no-upstream / no-auto-commit / protect-dirty-tree rules, fail-closed reporting, the validation-layer model. |
| [`CLAUDE.md`](./CLAUDE.md) | Claude Code-specific workflow notes (points to `AGENTS.md` first), plus OpenTTD's own build/run/test commands, architecture, and code style as technical reference. |
| [`tools/gate.sh`](./tools/gate.sh) | Single entry point for layered local validation — see 0.4 below. |
| [`research/`](./research/) | The validation-layer writeup and a PASS/FAIL/PARTIAL/NOT RUN/NOT APPLICABLE experiment-report template. |
| `.claude/`, `.aiad/` *(not committed)* | Personal Claude Code tooling imported from a sibling private project, kept strictly local via `.git/info/exclude`. |

### 0.4) Validation / gating model

Local validation is layered by cost, with one entry point (`tools/gate.sh`) for the build/test tiers:

| Tier | Command | Cost | When to use it |
|---|---|---|---|
| Smoke | `tools/gate.sh smoke` | cheapest | fast feedback while actively iterating |
| Change | `tools/gate.sh change` | standard | before calling a task/change done |
| Full | `tools/gate.sh full` | expensive | significant changes, or an experimental baseline |
| Research/evaluation | [`research/experiment-template.md`](./research/experiment-template.md) | n/a | records what was actually done and validated for an experiment |

All three build/test tiers are thin wrappers around OpenTTD's own existing CMake/CTest toolchain (see `CLAUDE.md` §Test) — there is no separate lint/format gate, because no such tool currently exists in this codebase, and this fork doesn't fake one. Full details and the experiment-report taxonomy are in [`research/README.md`](./research/README.md).

### 0.5) Ground rules (short version)

The full rules live in [`AGENTS.md`](./AGENTS.md); in short:

- No pushes, issues, or PRs to upstream OpenTTD, ever.
- No commit or push without an explicit, current instruction — prior approval doesn't carry forward to later, different changes.
- Protect the existing working tree; check `git status` before anything destructive.
- Prefer minimal, scoped changes over broad refactors.
- Report validation honestly: never claim a gate passed if it wasn't run, and never round an ambiguous result up to PASS.

### 0.6) Relationship to upstream OpenTTD

This fork tracks upstream OpenTTD as its technical foundation and does not modify its license, copyright, or attribution — see [3.0) Licensing](#30-licensing) and [4.0) Credits](#40-credits) below, both unchanged from upstream. Everything from section 1.0 onward is the original upstream README.

## 1.0) About

OpenTTD is a transport simulation game based upon the popular game Transport Tycoon Deluxe, written by Chris Sawyer.
It attempts to mimic the original game as closely as possible while extending it with new features.

OpenTTD is licensed under the GNU General Public License version 2.0, but includes some 3rd party software under different licenses.
See the section ["Licensing"](#30-licensing) below for details.

## 1.1) Downloading OpenTTD

OpenTTD can be downloaded from the [official OpenTTD website](https://www.openttd.org/).

Both 'stable' and 'nightly' versions are available for download:

- most people should choose the 'stable' version, as this has been more extensively tested
- the 'nightly' version includes the latest changes and features, but may sometimes be less reliable

OpenTTD is also available for free on [Steam](https://store.steampowered.com/app/1536610/OpenTTD/), [GOG.com](https://www.gog.com/game/openttd), and the [Microsoft Store](https://www.microsoft.com/p/openttd-official/9ncjg5rvrr1c). On some platforms OpenTTD will be available via your OS package manager or a similar service.

## 1.2) OpenTTD gameplay manual

OpenTTD has a [community-maintained wiki](https://wiki.openttd.org/), including a gameplay manual and tips.

## 1.3) Supported platforms

OpenTTD has been ported to several platforms and operating systems.

The currently supported platforms are:

- Linux (SDL (OpenGL and non-OpenGL))
- macOS (universal) (Cocoa)
- Windows (Win32 GDI / OpenGL)

Other platforms may also work (in particular various BSD systems), but we don't actively test or maintain these.

### 1.3.1) Legacy support

Platforms, languages and compilers change.
We'll keep support going on old platforms as long as someone is interested in supporting them, except where it means the project can't move forward to keep up with language and compiler features.

We guarantee that every revision of OpenTTD will be able to load savegames from every older revision (excepting where the savegame is corrupt).
Please report a bug if you find a save that doesn't load.

## 1.4) Installing and running OpenTTD

OpenTTD is usually straightforward to install, but for more help the wiki [includes an installation guide](https://wiki.openttd.org/en/Manual/Installation).

OpenTTD needs some additional graphics and sound files to run.

For some platforms these will be downloaded during the installation process if required.

For some platforms, you will need to refer to [the installation guide](https://wiki.openttd.org/en/Manual/Installation).

### 1.4.1) Free graphics and sound files

The free data files, split into OpenGFX for graphics, OpenSFX for sounds and
OpenMSX for music can be found at:

- [OpenGFX](https://www.openttd.org/downloads/opengfx-releases/latest)
- [OpenSFX](https://www.openttd.org/downloads/opensfx-releases/latest)
- [OpenMSX](https://www.openttd.org/downloads/openmsx-releases/latest)

Please follow the readme of these packages about the installation procedure.
The Windows installer can optionally download and install these packages.

### 1.4.2) Original Transport Tycoon Deluxe graphics and sound files

If you want to play with the original Transport Tycoon Deluxe data files you have to copy the data files from the CD-ROM into the baseset/ directory.
It does not matter whether you copy them from the DOS or Windows version of Transport Tycoon Deluxe.
The Windows install can optionally copy these files.

You need to copy the following files:
- sample.cat
- trg1r.grf or TRG1.GRF
- trgcr.grf or TRGC.GRF
- trghr.grf or TRGH.GRF
- trgir.grf or TRGI.GRF
- trgtr.grf or TRGT.GRF

### 1.4.3) Original Transport Tycoon Deluxe music

If you want the Transport Tycoon Deluxe music, copy the appropriate files from the original game into the baseset folder.
- TTD for Windows: All files in the gm/ folder (gm_tt00.gm up to gm_tt21.gm)
- TTD for DOS: The GM.CAT file
- Transport Tycoon Original: The GM.CAT file, but rename it to GM-TTO.CAT

## 1.5) Add-on content / mods

OpenTTD features multiple types of add-on content, which modify gameplay in different ways.

Most types of add-on content can be downloaded within OpenTTD via the 'Check Online Content' button in the main menu.

Add-on content can also be installed manually, but that's more complicated; the [OpenTTD wiki](https://wiki.openttd.org/) may offer help with that, or the [OpenTTD directory structure guide](./docs/directory_structure.md).

### 1.5.1) Social Integration

OpenTTD has the ability to load plugins to integrate with Social Platforms like Steam, Discord, etc.

To enable such integration, the plugin for the specific platform has to be downloaded and stored in the `social_integration` folder.

See [OpenTTD's website](https://www.openttd.org), under Downloads, for what plugins are available.

### 1.6) OpenTTD directories

OpenTTD uses its own directory structure to store game data, add-on content etc.

For more information, see the [directory structure guide](./docs/directory_structure.md).

### 1.7) Compiling OpenTTD

If you want to compile OpenTTD from source, instructions can be found in [COMPILING.md](./COMPILING.md).

## 2.0) Contact and Community

'Official' channels

- [OpenTTD website](https://www.openttd.org)
- [OpenTTD official Discord](https://discord.gg/openttd)
- IRC chat using #openttd on irc.oftc.net [more info about our irc channel](https://wiki.openttd.org/en/Development/IRC%20channel)
- [OpenTTD on Github](https://github.com/OpenTTD/) for code repositories and for reporting issues
- [forum.openttd.org](https://forum.openttd.org/) - the primary community forum site for discussing OpenTTD and related games
- [OpenTTD wiki](https://wiki.openttd.org/) community-maintained wiki, including topics like gameplay guide, detailed explanation of some game mechanics, how to use add-on content (mods) and much more

'Unofficial' channels

- the OpenTTD wiki has a [page listing OpenTTD communities](https://wiki.openttd.org/en/Community/Community) including some in languages other than English


### 2.1) Multiplayer games

You can play OpenTTD with others, either cooperatively or competitively.

See the [multiplayer documentation](./docs/multiplayer.md) for more details.

### 2.2) Contributing to OpenTTD

We welcome contributors to OpenTTD.  More information for contributors can be found in [CONTRIBUTING.md](./CONTRIBUTING.md)

### 2.3) Reporting bugs

Good bug reports are very helpful.  We have a [guide to reporting bugs](./CONTRIBUTING.md#bug-reports) to help with this.

Desyncs in multiplayer are complex to debug and report (some software development skils are required).
Instructions can be found in [debugging and reporting desyncs](./docs/debugging_desyncs.md).

### 2.4) Translating

OpenTTD is translated into many languages.  Translations are added and updated via the [online translation tool](https://translator.openttd.org).

## 3.0) Licensing

OpenTTD is licensed under the GNU General Public License version 2.0.
For the complete license text, see the file '[COPYING.md](./COPYING.md)'.
This license applies to all files in this distribution, except as noted below.

The squirrel implementation in `src/3rdparty/squirrel` is licensed under the Zlib license.
See `src/3rdparty/squirrel/COPYRIGHT` for the complete license text.

The md5 implementation in `src/3rdparty/md5` is licensed under the Zlib license.
See the comments in the source files in `src/3rdparty/md5` for the complete license text.

The fmt implementation in `src/3rdparty/fmt` is licensed under the MIT license.
See `src/3rdparty/fmt/LICENSE.rst` for the complete license text.

The nlohmann json implementation in `src/3rdparty/nlohmann` is licensed under the MIT license.
See `src/3rdparty/nlohmann/LICENSE.MIT` for the complete license text.

The OpenGL API in `src/3rdparty/opengl` is licensed under the MIT license.
See `src/3rdparty/opengl/khrplatform.h` for the complete license text.

The catch2 implementation in `src/3rdparty/catch2` is licensed under the Boost Software License, Version 1.0.
See `src/3rdparty/catch2/LICENSE.txt` for the complete license text.

The icu scriptrun implementation in `src/3rdparty/icu` is licensed under the Unicode license.
See `src/3rdparty/icu/LICENSE` for the complete license text.

The monocypher implementation in `src/3rdparty/monocypher` is licensed under the 2-clause BSD and CC-0 license.
See `src/3rdparty/monocypher/LICENSE.md` for the complete license text.

The OpenTTD Social Integration API in `src/3rdparty/openttd_social_integration_api` is licensed under the MIT license.
See `src/3rdparty/openttd_social_integration_api/LICENSE` for the complete license text.

The atomic datatype support detection in `cmake/3rdparty/llvm/CheckAtomic.cmake` is licensed under the Apache 2.0 license.
See `cmake/3rdparty/llvm/LICENSE.txt` for the complete license text.

## 4.0) Credits

See [CREDITS.md](./CREDITS.md)
