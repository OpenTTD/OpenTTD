# Published surfaces

One generator turns what a repository already states into everything it publishes. The
sources are its tracked Markdown, its git history and its `.ai/` layer. The outputs are:

| Surface | Command | What it is | Where it goes |
|---|---|---|---|
| Site | `surfaces site` | Every Markdown document as a page, plus an overview with interactive charts, the `.ai/` layer (features, decisions, rules, plan, dependency graph) and search | GitHub Pages |
| Wiki | `surfaces wiki` | The same documents as wiki pages: `Home`, a sidebar, links rewritten to pages | The repository's GitHub wiki |
| Markdown regions | `surfaces sync` | Tables inside the documents themselves (document indexes, features, decisions, rules, plan, surfaces), between markers | The repository |
| Gate | `surfaces check` | Fails while a region is stale, a relative link or anchor leads nowhere, a document's front matter does not parse, or a tool's flags and its usage disagree | Pre-commit, `tools/gate.sh`, CI |

Nothing in the generator names a repository. Every fact comes from git (`origin`, `HEAD`,
the tracked files, the log), from the `.ai/manifest.yaml` sections, and from each
document's own front matter and headings. The only declared input is `surfaces.toml` at
the repository root, and only for the choices data cannot make. Today that is which
documents to publish and which to exempt from the link check.

## Why

Documentation drifts the moment it restates something. A hand-kept index misses the
newest document. A status table says "planned" a week after the work merged. A landing
page describes a repository that changed visibility, and links point at files that were
renamed. Each copy is a second source of truth, so it is wrong as soon as the first one
moves.

The fix is not more diligence. Publish nothing that a person has to keep in step:

- **Derive, never restate.** A page is rendered from its document. A chart is computed
  from the history. A table is generated from the records it summarises. The published
  copy cannot disagree with the source because it has no content of its own.
- **One source, many surfaces.** A reader on GitHub, on the site, in the wiki or in an
  editor sees the same text. The region tables put the derived summaries inside the
  documents, where an agent working in the repository reads them too.
- **Enforced, not advised.** A stale region or a dangling link fails the commit, the gate
  and CI. Drift becomes a failing check with a name and a line number, not something a
  reviewer has to notice.
- **Readers drive the views.** Every chart is a model the reader controls: a window, a
  metric or a cut-off, kept in the URL so a view can be linked. Each chart maximizes to the
  viewport and Escape restores it. A diagram is Mermaid source in the document, rendered in
  place, never a pasted image.
- **Generic.** The same files work in any repository. A repository supervised by
  Majordomus also gets its `.ai/` layer rendered. The site, wiki and region report each
  declare themselves as `web-surface/v1` under `target/web/<id>/`, where
  `majordomus_web_surfaces` discovers them.

## Use

```bash
tools/surfaces/surfaces check          # the gate: exit 10 on a finding
tools/surfaces/surfaces sync           # regenerate every Markdown region in place
tools/surfaces/surfaces sync --check   # write nothing; exit 11 when a region would change
tools/surfaces/surfaces site           # build target/web/site (needs node for the assets)
tools/surfaces/surfaces wiki           # build target/web/wiki
tools/surfaces/surfaces list           # what each surface publishes, one document per line
```

Prerequisites: `python3` 3.11 or newer. The wrapper creates `tools/surfaces/.venv` from
`requirements.txt` on first use. `site` also needs `node`; it runs `npm ci` in this
directory the first time. Exit codes follow Majordomus: 0 ok, 2 usage, 10 findings,
11 drift.

To preview the site, serve `target/web/site` with any static server, for example
`python3 -m http.server -d target/web/site`.

## Tool flags

A document that shows `tool --flag` restates the tool's parser, so `check` treats it like a link:
the flag has to lead somewhere. The tools are the tracked executable scripts. Shell scripts are
read through their `case` arms (and `[[ "$1" == --x ]]` tests), Python scripts through argparse
(and `"--x" in argv`). Each finding names the file and line:

| Finding | When |
|---|---|
| `usage` | the script accepts a flag that its own usage header never names |
| `help` | `--help` is missing from a script with a usage header, exits non-zero, or prints a fixed line range that stops before the header ends |
| `flag` | a document's code shows `tool --flag` and the tool's parser does not accept it |

A shell script whose catch-all `*)` arm does not `exit`, or that only tests flags with `[[ ]]`,
passes unknown flags on, so it cannot refuse one and the `flag` check skips it. The usage header
is the comment block after the shebang. `-h|--help) sed -n '2,/^[^#]/s/^# \{0,1\}//p' "$0"; exit 0 ;;`
prints all of it, however long it grows. `[flags] exclude` in `surfaces.toml` takes path globs
for scripts the check must not read.

## Markdown regions

A region is a pair of HTML comments whose contents the generator owns:

```text
<!-- surfaces:begin docs-index dir=research -->
(generated table)
<!-- surfaces:end docs-index -->
```

| Generator | Arguments | Table |
|---|---|---|
| `docs-index` | `dir=<path>` (default: all), `depth=<n>` | Every published document under the path, with its derived summary |
| `ai-features` | — | `.ai/` features: name, status, headline |
| `ai-adrs` | — | Architecture decisions: id, status, title |
| `ai-rules` | `origin=project\|vendored\|all` | Rules: id, class, statement |
| `ai-plan` | — | Milestones with issues done and still open (status derived from the issue stamps) |
| `surfaces` | — | Where the site and the wiki are published |
| `facts` | `group=<name>`, `as=table\|line\|pie` | Numbers counted from the tracked files: each `[[facts.items]]` in `surfaces.toml` counts regex `matches` `in` a glob, tracked `files` matching a glob, or published `docs` under a path; the table shows what was counted, `line` is one sentence of bold numbers, `pie` a Mermaid pie |

A fact is the one kind of number a committed document can state without going stale: it changes
only when the files it counts change, and `surfaces sync` rewrites it in the same commit. A count
of commits or of today's date is not a fact in this sense, since every commit would change it; the
site's overview computes those at build time instead. The overview also shows every fact as a card
linked to its page, with what was counted as its tooltip.

Edit the text around a region, never inside it. `surfaces sync` rewrites the inside, and
`surfaces check` fails while it would.

A document's title comes from its front matter `title`, or else its first `# `, `<h1>` or
setext (`====`) heading, or else its file name.
Its summary comes from front matter `headline`, `description` or `statement`, or else its
first paragraph. A better first paragraph therefore improves every index that lists the
document.

Front matter that does not parse as YAML would otherwise be published as text: the page opens
with its raw fields and every table that reads its `id` or `title` gets an empty cell. `check`
fails on it (finding `front`), naming the parser's error. The usual cause is an unquoted value
containing `: `. `[check] front_matter_exclude` takes path globs for documents vendored from
elsewhere, which are fixed where they come from.

## Site

- **Shell.** The site uses the Flowbite admin shell: a navbar with search, a sidebar
  grouped by top-level directory, a breadcrumb, cards and tables. It is built from
  `assets/input.css` with Tailwind 4, Flowbite 4 and `flowbite-typography`.
- **Front-end libraries.** Mermaid 12 renders diagrams and Apache ECharts 6 renders charts.
  Both are copied from `node_modules` and loaded only on pages that need them. Nothing is
  fetched from a CDN at view time.
- **Links.** A relative link to a published document becomes a link to its page, with
  GitHub-compatible heading anchors. A link to any other tracked file goes to GitHub at
  the exact commit the site was built from. Images load from that same commit.
- **Data.** `data/site.json` holds the chart inputs: weekly commits, the documentation map,
  per-document age at `HEAD` and plan progress. `data/search.json` holds the search index.
  Both are computed at build time from git, so the same commit always builds the same site.

Every page ends with where it is linked from and what it links to, and the "All documents" page
draws every link between published documents as a force graph (drag, zoom, hover, click; a
`links.min` selector, kept in the URL, hides rarely linked documents). Both come from the links
the documents already contain, resolved the way `check` resolves them, so a new link shows up
on both ends with no index to maintain. The wiki gets the same "Linked from" line on every page.

The shell shows the document's title as the page heading, so an article's first `# ` heading
is dropped when it is that same title. The sidebar lists documents by title, with the path as
the tooltip. Search reads the title, path, summary, headings and the first part of the body,
and is shown at every width. `404.html` carries a `<base href>` set to the Pages path, because
GitHub serves it at whatever URL was missed.

## Wiki

GitHub keeps a wiki in a separate repository, `<repo>.wiki.git`, which exists only after
its first page has been saved once in the web interface. Until then the workflow's wiki
job fails and says so. Page names are derived from document paths, `README.md` becomes
`Home`, and `_Sidebar.md` and `_Footer.md` are generated. A page is the document's body: its front matter,
when it has one, is folded into a "Record metadata" block at the end, and a document whose
title only exists in its front matter gets it as a `# ` heading. The sidebar holds one collapsed
group per top-level directory, and titles that two documents share carry the file name. The
workflow copies the pages with `rsync --checksum`, so a page whose size and time happen to match
the old one is still replaced. Each sync replaces the whole
wiki: any edit made in the wiki is overwritten, and the footer of every page says so.

## In CI

`.github/workflows/surfaces.yml` runs `check`, `site` and `wiki` on every push and pull
request. From the default branch only, it deploys the site to Pages and pushes the wiki.
The checkout is full-history but blobless, because the charts read the commit log.

## Enforcement

`surfaces check` runs in three places:

- the pre-commit hook, declared in `.ai/repo/policy.yaml` under `enforcement` and
  reconciled by `majordomus doctor`;
- `tools/gate.sh change`;
- the workflow.

The project rule `project.published-surfaces-are-derived` states the contract.

## Files

| File | Role |
|---|---|
| `surfaces` | Entry point. Creates the virtual environment, then runs `surfaces.py` |
| `surfaces.py` | The generator: discovery, rendering, regions, check |
| `assets/input.css`, `assets/app.js` | The site's stylesheet source and client script |
| `package.json`, `package-lock.json` | Pinned front-end dependencies |
| `requirements.txt` | Pinned Python dependencies |
