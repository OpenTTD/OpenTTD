# Delivering to master: a pull request that merges itself when it passes

`korczis/OpenTTD` `master` is branch-protected (2026-11-22): a commit reaches it only after the
required check "Check and build" (workflow Surfaces) is green on it, for admins too. Several
sessions land the same day, so a direct `git push origin HEAD:master` loses the race whenever
master moves during the check. The owner's instruction of 2026-11-22: "use gh cli a vzdy enabluj
automerge pokud vse projde, at to odsypa, vse zdokumentuj, informuj ostatni a zacni pouzivat"
(use the gh CLI and always enable auto-merge so it merges when everything passes). The repository
allows auto-merge since then (`allow_auto_merge: true`).

| Step | Run |
|---|---|
| work on a branch in its worktree | `majordomus worktree create <branch>`, then `majordomus start ...` |
| bring master in and test the merged tree | `git fetch && git merge origin/master`, then the gate the change needs (`tools/gate.sh change`, the crate's `cargo test --release`) |
| close the task | `majordomus handover`, `majordomus finish --outcome completed --verify-command "<cmd>"` |
| push the branch | `git push origin HEAD:<branch>` |
| open the pull request | `gh pr create -R korczis/OpenTTD --base master --head <branch> --title "<commit-style title>" --body-file <file>` |
| let it merge itself | `gh pr merge <n> -R korczis/OpenTTD --auto --merge` |
| tell the peers | the board (`majordomus_announce`) and a message to the sessions working on the same paths |

GitHub merges the pull request as soon as the required check is green; nothing waits on the
session. When master moves first and the branch conflicts, the pull request stays open: merge
master into the branch again, re-run the gate and push; auto-merge stays enabled.

- The title follows the fork's commit-message form (`Add: [Component] ...`, CODINGSTYLE.md); the
  merge commit is a plain merge, so every commit of the branch is kept.
- The body says what changed and how it was validated, each check as PASS, FAIL or NOT RUN, and
  ends with `🤖 Generated with [Claude Code](https://claude.com/claude-code)` when an agent wrote it.
- A pull request is opened only against `korczis/OpenTTD`. Never against upstream OpenTTD: `gh`
  defaults to the parent repository of a fork, so `-R korczis/OpenTTD` is always given.
- Direct pushes to `master` stay possible for a commit already green on Surfaces with master
  unchanged since, but a pull request with auto-merge is the default.
