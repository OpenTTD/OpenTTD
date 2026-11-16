# Playing on official public servers: the vanilla client

RESEARCH-ONLY tooling, not a change to the game. Issue I0071; ADR 0030; complements
[net-compat.md](net-compat.md) (ADR 0029), which is about games **you** run.

## Why a fork build cannot join a public server

A public server runs an official release, and a tagged release accepts only a client of exactly its
own revision (`IsNetworkCompatibleVersion`, `src/network/network.cpp`; the revision comes from
`cmake/FindVersion.cmake`: `REV_VERSION`, `REV_ISTAG`, `REV_MODIFIED`). Every build of this fork
reports its own commit, so the server refuses it at the version check: deterministic lockstep
(ADR 0016) means a client with different code would desync. Making the fork *report* an official
revision would be lying to the server about the code it runs, the one thing that gets a client
banned, and it would still desync. ADR 0030 rules it out ("no disguise").

## The answer: two clients

| Where you play | Client | Fork features |
|---|---|---|
| An official public server | the **vanilla client**: the release tag built unmodified | none: no MCP, no automation, no armed input |
| Your own server, same version | a fork client on the release tag (separate work) | yes, on servers you run |
| A loopback R&D game | the fork, both ends from one commit (or ADR 0029's policy) | yes |

## Usage

```bash
tools/research/vanilla-client tags            # the release tags, newest first
tools/research/vanilla-client build 15.3      # worktree <repo>-wt/vanilla/15.3 at the tag, Release build
tools/research/vanilla-client check 15.3      # proves the build is the unmodified release
tools/research/vanilla-client run 15.3        # plays it, with a private home
tools/research/vanilla-client run 15.3 -- -n <server>:<port>   # straight onto a server
```

Pick the tag the server shows in its server list; the default is the newest stable tag.

`check` passes only when every one of these holds, read from the tree and the generated
`rev.cpp`, never assumed:

- the worktree's HEAD is the tag's commit, with no local change;
- no file of this fork is present (`src/prismatic_mcp`, `.ai`);
- the build's revision string is exactly the tag, `tagged` is 1 and `modified` is 0;
- the binary exists.

`run` refuses a build that does not pass `check`.

## "Hide me": what the client tells a server

The game derives its personal directory from `$HOME` (`GetHomeDir`, `src/fileio.cpp`), so `run`
gives the vanilla client a home of its own under `$VANILLA_HOME/<tag>/home` (default
`~/.local/share/openttd-vanilla`). Configuration, saves and downloaded content stay apart from your
fork games; no fork AI is scanned. Only the official base sets are linked in from your personal
directory.

Before every run, `private.cfg` (where the game keeps them) gets:

- `network.client_name`: `Player`, or `$VANILLA_PLAYER`: a name that identifies nobody;
- `network.participate_survey`: `no`.

On a home with no `openttd.cfg` yet, the game migrates its settings on first start and rewrites
`private.cfg` with defaults, so `run` first lets it write its configuration (one headless tick
with the null drivers), then sets the two values.

What a server still sees: your IP address (use your own network measures if that matters), the
release revision, which is exactly what it expects, and your chosen name. Nothing of the fork.

## Rules of conduct on public servers

- Never run fork features against a server you do not own: no MCP server, no bot, no armed input.
  The vanilla client has none of them to run.
- Follow each server's own rules; a server's admin decides who plays there.

## What has been verified

`check 15.3` passes on a build of tag 15.3 (14ec60f248); a headless `run` with a fresh home writes
`client_name = Player` and `participate_survey = no` (mode 600) and scans no fork AI. A join to a
real public server has not been run by this tooling: the revision match is read from the build,
and the first join is yours to make.
