# Following upstream: the rebase procedure

The fork follows `OpenTTD/OpenTTD` by rebasing its own commits onto `upstream/master`
(ADR 0011). Upstream is a fetch-only remote (rule `project.no-upstream-interaction`). Every
step that rewrites a published branch needs an explicit, current instruction
(rule `project.no-commit-without-current-instruction`).

## 1. Remotes

```bash
git remote add upstream https://github.com/OpenTTD/OpenTTD.git   # once
git remote set-url --push upstream no-push-to-upstream            # once: push is impossible
git fetch origin && git fetch upstream master --no-tags
```

## 2. Measure before rewriting

```bash
MB=$(git merge-base master upstream/master)
git rev-list --count upstream/master..master        # the fork's own commits
git rev-list --count master..upstream/master        # what upstream added
git rev-list --merges --count "$MB"..master         # must be 0: the series is linear
comm -12 <(git diff --name-only "$MB" master | sort) \
         <(git diff --name-only "$MB" upstream/master | sort)   # files both sides touched
```

A file touched on both sides is where a conflict or a silent semantic break can hide. Read
the upstream change to each one before continuing.

## 3. Keep the old head, then rebase

```bash
git branch backup/master-pre-rebase-$(date +%F) master
git rebase upstream/master
git range-diff "$MB"..backup/master-pre-rebase-$(date +%F) upstream/master..master
```

`range-diff` must show every fork patch as `=` (unchanged). A `!` is a patch the rebase
changed. Review it like new code.

## 4. Verify the gated code, not only the default build

A default build does not compile research code (rule `project.research-code-is-gated-and-labelled`).
Upstream API drift breaks exactly that code, so verify both builds:

```bash
tools/gate.sh change                                   # default build + full ctest
tools/gate.sh research                                 # the same with the research options ON
E=rebase-verify-$(date +%F)
tools/research/research init --experiment-id "$E"
tools/research/research configure --experiment-id "$E" --profile research-debug
tools/research/research build     --experiment-id "$E" --profile research-debug
tools/research/research run       --experiment-id "$E" --profile research-debug
tools/research/research report    --experiment-id "$E"
```

Then prove the other half of containment, that a release-like build carries none of it:

```bash
tools/research/research configure --experiment-id "$E-baseline" --profile baseline-release
tools/research/research build     --experiment-id "$E-baseline" --profile baseline-release
strings build-research/openttd | grep -q '^research_status'
! strings build-research-baseline-release/openttd | grep -q '^research_status'
bash tools/research/test/research_test.sh
```

Once the MCP server is on master, also build with `OPTION_PRISMATIC_MCP_SERVER=ON` and run
`openttd_test '[prismatic_mcp]'`. `ctest -R prismatic` matches no test name, so pass
`--no-tests=error` to any ctest filter. The base set must be installed first
(knowledge `macos-missing-baseset-hangs-headless-runs`).

Do not run the two heavy builds at the same time as the headless `run`. Under load, the
dedicated server's stdin start-up race can lose the `research_status` line
(knowledge `dedicated-server-stdin-startup-race`).

## 5. Publish

```bash
git push --force-with-lease origin master
```

Then rebase every other branch that was cut from the old base. Such a branch still carries the
pre-rebase copies of the fork series under its own commits, so a plain `git rebase` would
replay them. Back the branch up and push it, then move only its own commits, naming the last
old fork commit it was cut from:

```bash
git branch backup/<branch>-pre-rebase-$(date +%F) <branch>
git rebase --onto master <last-old-fork-commit> <branch>   # e.g. 0822f6b762 for research/prismatic-mcp-server
```

Then re-verify it with its own options ON (issue I0017).

Once the rewritten branches are verified and pushed, keep each pre-rewrite head as an
annotated tag on origin and drop the backup branch: the history stays reachable, and master
stays linear.

```bash
git tag -a archive/<backup-name> backup/<backup-name> -m "Archive of <what it was>"
git push origin archive/<backup-name>
git branch -D backup/<backup-name>      # only after the tag is on origin
```

## Record

The rebase is an issue in the plan (I0021 for 2026-11-15). Its evidence is the range-diff
count, the gate results and the experiment's `summary.md`.

## After the rewrite

A rewrite changes every hash the layer cites. Re-derive and re-prove the record with
`majordomus://skill/derive-the-ai-layer-from-history`. A task that was open across the rewrite
can no longer be finished: close it with `majordomus://skill/close-a-task-after-a-history-rewrite`.
