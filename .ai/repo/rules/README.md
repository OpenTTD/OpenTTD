---
schema: context/v1
id: ai.repo.rules
kind: context
title: Repository rules
description: The rule object format, where rules live, how they are loaded and composed.
status: active
scope: subtree
providers: ["*"]
audience: [human, agent]
composition: extend
order: 100
children:
  require_contract: true
  exempt: [.ai/repo/rules/vendor]
---

# Repository rules

A rule is a Markdown document with YAML front matter. The front matter is the machine
side: identity, class, dependencies, and how the tool enforces it. The body is the human
side: rationale, required behaviour, failure behaviour, verification.


The vendored package is installed here, not authored here, so it owes no context
document: this contract exempts it by name (`children.exempt`). Every directory the
repository writes itself still owes one.

## Locations

```text
project/                  rules this repository wrote
vendor/majordomus/        the pinned Majordomus baseline; do not edit, upgrade explicitly
vendor/fleet/             the owner's fleet pack, when one applies: rules/, kits/ and MANIFEST
```

The vendored package carries `manifest.yaml` naming every rule file with its identity and
content hash. A hand edit under `vendor/` is detected against that manifest and refused.

The fleet pack is the owner's own package, kept outside every repository
(`MAJORDOMUS_FLEET_PACK`, else `$XDG_CONFIG_HOME/majordomus/fleet`, else
`~/.config/majordomus/fleet`). `init`, `migrate`, `rules vendor update` and `update` install
only the files its manifest lists, and only where its `applies_to` matches: rules and kit
profiles under `vendor/fleet/`, skills as `skills/fleet-<id>/`. `vendor/fleet/MANIFEST` is the
stamp — an `# ai-fleet-stamp/v1` line and a `# version: <n>` line, then one
`<sha256><TAB><installed path><TAB><kind><TAB><pack path>` row per installed file — so a hand
edit is a doctor FAIL and an older copy than the pack a WARN. A row is read only where it names
a place the fleet installs into (`skills/fleet-<id>/SKILL.md`, `vendor/fleet/rules/<id>.v<n>.md`,
`vendor/fleet/kits/<kit>.yaml`); any other row is a doctor FAIL and is never moved or
loaded. The effective set reads the fleet's rules between the baseline and `project/`, and a
fleet rule may not claim the `majordomus` or `project` namespace. Into a public tree
(`ui.visibility` in the policy) an entry marked `visibility: private` is skipped, and a file
that matches the kit's licence evidence or names a home or absolute path refuses the install.
A home path is `/Users/`, `/home/`, `$HOME`, `${HOME}` or `~/` before a non-space, anywhere on
a line; an absolute path is `/root/`, `/opt/`, `/Volumes/`, `/private/`, `/mnt/`, `/srv/`,
`/var/`, `/tmp/`, `/etc/`, `/usr/` or a drive such as `C:\`, at the start of a line or after a
space, quote, parenthesis, `=`, `:` or backtick, so that the path of a URL is not one. A kit in
play (the policy's, or a kit entry the tree would receive) with no evidence on this machine or
in `MAJORDOMUS_KIT_EVIDENCE_<KIT>` refuses the install as not judged.

## Front matter

```yaml
---
id: project.example-rule        # identity; namespaced by origin
version: 1                       # an exact integer
kind: rule
title: One line
description: What the rule requires, in one sentence.
statement: The normative sentence a worker follows.
status: active                   # active | deprecated
class: blocking                  # blocking | advisory
depends_on: []                   # exact references: id@version
tags: []
x-majordomus:                    # present only on a rule the tool enforces
  validator: example             # the function mj_validate_<validator>
  category: example              # the finding category the validator reports under
  enforced_by: [check]           # the commands that dispatch it
  exit_code: 10
  claims: []                     # ids in docs/CLAIMS.yaml
  tests: []                      # the behavioural cases that prove it
---
```

Identity comes from `id` and `version`, never from the file name. A rule without an
`x-majordomus` block is normative for whoever reads it and enforced by nobody; the class
still says what a violation means.

## Loading

Resolve `depends_on` as a graph, deterministically: a missing dependency, a cycle, or two
rules claiming one identity at the same scope is an error, and a set that does not resolve
is not applied at all.

## Composition

The effective set is additive: every active vendored rule plus every active project rule.
A project rule may add a constraint. There is no override mechanism: nothing here disables
or weakens a vendored rule, and a project rule may not reuse a vendored identity.
