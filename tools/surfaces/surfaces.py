#!/usr/bin/env python3
"""Published surfaces of a repository, derived from what git tracks and from its .ai/ layer.

Three surfaces, one source of truth:

  site    a static site (GitHub Pages): every tracked Markdown document as a page, an
          overview with interactive charts, the .ai/ layer (features, decisions, rules,
          plan) as tables and a dependency graph, client-side search
  wiki    the same documents as a GitHub wiki: Home, a sidebar, links rewritten to pages
  regions generated regions inside the Markdown documents themselves, between
          <!-- surfaces:begin NAME --> and <!-- surfaces:end NAME --> markers

and one gate, `check`, that fails while any of them would disagree with the repository:
a stale region, a relative link to nothing, an anchor no heading defines.

Nothing here names a repository. Every fact comes from git, the .ai/ manifest, the
documents' own front matter, or `surfaces.toml` at the repository root for the few choices
data cannot make (which documents to publish). See README.md beside this file.

Exit codes follow Majordomus: 0 ok, 2 usage, 10 findings (contract unmet), 11 drift.
"""

from __future__ import annotations

import argparse
import datetime as dt
import html
import json
import os
import posixpath
import re
import shutil
import subprocess
import sys
import tomllib
import unicodedata
import urllib.parse
from dataclasses import dataclass, field
from pathlib import Path

import yaml
from markdown_it import MarkdownIt

EXIT_OK, EXIT_USAGE, EXIT_FINDINGS, EXIT_DRIFT = 0, 2, 10, 11
HERE = Path(__file__).resolve().parent
CONFIG_FILE = "surfaces.toml"
SURFACE_SCHEMA = "web-surface/v1"
GENERATED_ROOT = "target/web"  # the root Majordomus discovers surface.json declarations in

DEFAULTS = {
    "sources": {"include": ["**/*.md"], "exclude": []},
    "check": {"exclude": []},
    "flags": {"exclude": []},
    "site": {"out": f"{GENERATED_ROOT}/site", "mount": "/site"},
    "wiki": {"out": f"{GENERATED_ROOT}/wiki", "mount": "/wiki"},
    "regions": {"report": f"{GENERATED_ROOT}/markdown"},
}

REGION_RE = re.compile(
    r"(<!-- surfaces:begin (?P<name>[a-z0-9-]+)(?P<args>(?: [a-z0-9_-]+=[^ >]+)*) -->\n)"
    r"(?P<body>.*?)"
    r"(<!-- surfaces:end (?P=name) -->)",
    re.S,
)
SCHEME_RE = re.compile(r"^[a-zA-Z][a-zA-Z0-9+.-]*:")
HTML_ATTR_RE = re.compile(r'''(\s(?:href|src)=)(["'])(.*?)\2''', re.I)
HTML_ID_RE = re.compile(r'''\s(?:id|name)=["']([^"']+)["']''', re.I)


# --------------------------------------------------------------------------- repository


def git(root: Path, *args: str) -> str:
    return subprocess.run(
        ["git", "-C", str(root), *args], check=True, capture_output=True, text=True
    ).stdout


def glob_re(pattern: str) -> re.Pattern:
    out, i = "", 0
    while i < len(pattern):
        if pattern.startswith("**/", i):
            out, i = out + "(?:.*/)?", i + 3
        elif pattern.startswith("**", i):
            out, i = out + ".*", i + 2
        elif pattern[i] == "*":
            out, i = out + "[^/]*", i + 1
        elif pattern[i] == "?":
            out, i = out + "[^/]", i + 1
        else:
            out, i = out + re.escape(pattern[i]), i + 1
    return re.compile(out + r"\Z")


def matches(path: str, patterns: list[str]) -> bool:
    return any(glob_re(p).match(path) for p in patterns)


@dataclass
class Repo:
    root: Path
    sha: str
    slug: str
    branch: str
    head_time: int
    shallow: bool
    tracked: set[str]
    dirs: set[str]

    @property
    def web(self) -> str:
        return f"https://github.com/{self.slug}"

    @property
    def owner(self) -> str:
        return self.slug.split("/")[0]

    @property
    def name(self) -> str:
        return self.slug.split("/")[-1]

    @property
    def pages_url(self) -> str:
        if self.name.lower() == f"{self.owner.lower()}.github.io":
            return f"https://{self.owner.lower()}.github.io/"
        return f"https://{self.owner.lower()}.github.io/{self.name}/"

    @property
    def wiki_url(self) -> str:
        return f"{self.web}/wiki"

    def blob(self, path: str) -> str:
        return f"{self.web}/blob/{self.sha}/{urllib.parse.quote(path)}"

    def tree(self, path: str) -> str:
        return f"{self.web}/tree/{self.sha}/{urllib.parse.quote(path)}"

    def raw(self, path: str) -> str:
        return f"https://raw.githubusercontent.com/{self.slug}/{self.sha}/{urllib.parse.quote(path)}"

    def edit(self, path: str) -> str:
        return f"{self.web}/edit/{self.branch}/{urllib.parse.quote(path)}"


def slug_from_remote(url: str) -> str | None:
    m = re.search(r"github\.com[:/]+([^/]+)/([^/]+?)(?:\.git)?/?$", url.strip())
    return f"{m.group(1)}/{m.group(2)}" if m else None


def load_repo(root: Path) -> Repo:
    sha = os.environ.get("GITHUB_SHA") or git(root, "rev-parse", "HEAD").strip()
    slug = os.environ.get("GITHUB_REPOSITORY")
    if not slug:
        try:
            slug = slug_from_remote(git(root, "remote", "get-url", "origin"))
        except subprocess.CalledProcessError:
            slug = None
    slug = slug or f"local/{root.name}"
    branch = os.environ.get("SURFACES_DEFAULT_BRANCH", "")
    if not branch:
        try:
            branch = git(root, "symbolic-ref", "--short", "refs/remotes/origin/HEAD").strip()
            branch = branch.split("/", 1)[1]
        except subprocess.CalledProcessError:
            branch = git(root, "rev-parse", "--abbrev-ref", "HEAD").strip()
    # What git tracks, plus new files it would not ignore: exactly what the next commit can hold,
    # so `sync` and `check` agree before and after a `git add`. CI checkouts have none of the second.
    tracked = set(git(root, "ls-files", "-z", "--cached", "--others", "--exclude-standard").split("\0")) - {""}
    tracked = {p for p in tracked if (root / p).exists()}
    dirs = {""}
    for p in tracked:
        parts = p.split("/")[:-1]
        for i in range(1, len(parts) + 1):
            dirs.add("/".join(parts[:i]))
    return Repo(
        root=root,
        sha=sha,
        slug=slug,
        branch=branch,
        head_time=int(git(root, "show", "-s", "--format=%ct", "HEAD").strip()),
        shallow=git(root, "rev-parse", "--is-shallow-repository").strip() == "true",
        tracked=tracked,
        dirs=dirs,
    )


def load_config(root: Path) -> dict:
    cfg = json.loads(json.dumps(DEFAULTS))
    path = root / CONFIG_FILE
    if path.exists():
        with path.open("rb") as fh:
            user = tomllib.load(fh)
        for section, values in user.items():
            if section not in cfg:
                raise SystemExit(f"surfaces: {CONFIG_FILE}: unknown section [{section}]")
            for key, value in values.items():
                if key not in cfg[section]:
                    raise SystemExit(f"surfaces: {CONFIG_FILE}: unknown key {section}.{key}")
                cfg[section][key] = value
    return cfg


# --------------------------------------------------------------------------- documents


def split_front_matter(text: str) -> tuple[dict | None, str, int]:
    """Return (front matter, body, number of lines the front matter took)."""
    for fence, loader in (("---", yaml.safe_load), ("+++", tomllib.loads)):
        if text.startswith(fence + "\n"):
            end = re.search(rf"\n{re.escape(fence)}\n", text[len(fence) :])
            if end:
                raw = text[len(fence) + 1 : len(fence) + end.start()]
                try:
                    data = loader(raw)
                except Exception:  # noqa: BLE001 - malformed front matter is just text
                    return None, text, 0
                body = text[len(fence) + end.end() :]
                if isinstance(data, dict):
                    return data, body, text[: len(text) - len(body)].count("\n")
    return None, text, 0


def plain(text: str) -> str:
    text = re.sub(r"<!--.*?-->", "", text, flags=re.S)
    text = re.sub(r"!\[([^\]]*)\]\([^)]*\)", r"\1", text)
    text = re.sub(r"\[([^\]]*)\]\([^)]*\)", r"\1", text)
    text = re.sub(r"<[^>]+>", "", text)
    text = re.sub(r"[*`]+", "", text)
    text = re.sub(r"(?<!\w)_+|_+(?!\w)", "", text)  # emphasis, never snake_case
    return re.sub(r"\s+", " ", text).strip()


def github_slug(text: str) -> str:
    out = []
    for ch in text.lower():
        if ch.isalnum() or ch in "-_ " or unicodedata.category(ch).startswith("M"):
            out.append(ch)
    return "".join(out).replace(" ", "-")


@dataclass
class Doc:
    path: str
    text: str
    fm: dict | None
    body: str
    offset: int
    title: str = ""
    summary: str = ""
    page: str = ""
    wiki: str = ""
    slugs: set[str] = field(default_factory=set)
    headings: list[str] = field(default_factory=list)
    changed: int = 0

    @property
    def group(self) -> str:
        return self.path.split("/")[0] if "/" in self.path else ""

    @property
    def lines(self) -> int:
        return self.text.count("\n") + 1


def first_heading_and_paragraph(body: str) -> tuple[str, str]:
    title, para, in_fence, buf = "", "", False, []
    for line in body.splitlines():
        s = line.strip()
        if s.startswith(("```", "~~~")):
            in_fence = not in_fence
            if buf:
                break
            continue
        if in_fence:
            continue
        if not title and s.startswith("# "):
            title = plain(s[2:])
            continue
        if s.startswith("#"):
            if buf:
                break
            continue
        if not s:
            if buf:
                break
            continue
        if not buf and (s.startswith(("|", "<!--", ">", "- ", "* ", "+ ", "![", "<", "---"))
                        or re.match(r"\d+[.)] ", s)):
            continue
        buf.append(s)
    para = plain(" ".join(buf))
    if len(para) > 220:
        para = para[:217].rsplit(" ", 1)[0] + "..."
    return title, para


def page_for(path: str) -> str:
    stem, base = posixpath.split(path)
    if base.lower() == "readme.md":
        return posixpath.join(stem, "index.html")
    return path[:-3] + ".html"


def wiki_for(path: str) -> str:
    if path.lower() == "readme.md":
        return "Home"
    return re.sub(r"[^A-Za-z0-9_-]+", "-", path[:-3]).strip("-")


def load_docs(repo: Repo, cfg: dict) -> dict[str, Doc]:
    include, exclude = cfg["sources"]["include"], cfg["sources"]["exclude"]
    docs: dict[str, Doc] = {}
    for path in sorted(repo.tracked):
        if not path.lower().endswith(".md") or not matches(path, include) or matches(path, exclude):
            continue
        file = repo.root / path
        if not file.is_file():
            continue
        text = file.read_text(encoding="utf-8", errors="replace")
        fm, body, offset = split_front_matter(text)
        doc = Doc(path=path, text=text, fm=fm, body=body, offset=offset)
        heading, para = first_heading_and_paragraph(body)
        fm = fm or {}
        doc.title = str(fm.get("title") or heading or posixpath.basename(path)[:-3])
        doc.summary = plain(
            str(fm.get("headline") or fm.get("description") or fm.get("statement") or para)
        )
        doc.page = page_for(path)
        doc.wiki = wiki_for(path)
        docs[path] = doc
    pages = {}
    for doc in docs.values():  # two documents may not claim one page or one wiki name
        if doc.page in pages:
            doc.page = doc.path + ".html"
        pages[doc.page] = doc
    names: dict[str, int] = {}
    for doc in docs.values():
        if doc.wiki in names:
            names[doc.wiki] += 1
            doc.wiki = f"{doc.wiki}-{names[doc.wiki]}"
        else:
            names[doc.wiki] = 0
    return docs


def stamp_changed(repo: Repo, docs: dict[str, Doc]) -> None:
    """The commit time each document last changed, from one pass over the history."""
    if not docs:
        return
    out = git(repo.root, "log", "--format=%x1e%ct", "--name-only", "HEAD", "--", *docs)
    for block in out.split("\x1e"):
        lines = block.strip().splitlines()
        if not lines:
            continue
        when = int(lines[0])
        for path in lines[1:]:
            doc = docs.get(path.strip())
            if doc and not doc.changed:
                doc.changed = when
    for doc in docs.values():
        doc.changed = doc.changed or repo.head_time


# --------------------------------------------------------------------------- the .ai layer


def status_of_issue(rec: dict) -> str:
    if rec.get("completed_at") or rec.get("verified_at"):
        return "done"
    if rec.get("started_at"):
        return "active"
    return "open"


@dataclass
class Layer:
    present: bool = False
    features: list[dict] = field(default_factory=list)
    adrs: list[dict] = field(default_factory=list)
    rules: list[dict] = field(default_factory=list)
    milestones: list[dict] = field(default_factory=list)
    issues: list[dict] = field(default_factory=list)


def load_layer(repo: Repo) -> Layer:
    manifest = repo.root / ".ai/manifest.yaml"
    layer = Layer()
    if ".ai/manifest.yaml" not in repo.tracked:
        return layer
    data = yaml.safe_load(manifest.read_text()) or {}
    sections = data.get("sections") or {}
    layer.present = True

    def files(section: str, suffix: str, sub: str = "") -> list[str]:
        rel = sections.get(section)
        if not rel:
            return []
        base = posixpath.join(".ai", rel, sub).rstrip("/") + "/"
        return sorted(
            p for p in repo.tracked
            if p.startswith(base) and p.endswith(suffix) and not p.endswith("README.md")
        )

    def front(path: str) -> dict:
        fm, _, _ = split_front_matter((repo.root / path).read_text(encoding="utf-8"))
        return {**(fm or {}), "path": path}

    layer.features = sorted(
        (front(p) for p in files("features", ".md")), key=lambda f: (f.get("weight", 999), f["path"])
    )
    layer.adrs = [front(p) for p in files("adrs", ".md")]
    layer.rules = [
        {**front(p), "origin": "vendored" if "/vendor/" in p else "project"}
        for p in files("rules", ".md")
    ]
    for p in files("project", ".yaml", "milestones"):
        rec = yaml.safe_load((repo.root / p).read_text()) or {}
        layer.milestones.append({**rec, "path": p})
    layer.milestones.sort(key=lambda m: (m.get("order", 999), m.get("id", "")))
    for p in files("project", ".yaml", "issues"):
        rec = yaml.safe_load((repo.root / p).read_text()) or {}
        layer.issues.append({**rec, "path": p, "status": status_of_issue(rec)})
    return layer


# --------------------------------------------------------------------------- links


@dataclass
class Finding:
    kind: str
    path: str
    line: int
    message: str

    def __str__(self) -> str:
        return f"FAIL {self.kind:<8} {self.path}:{self.line} — {self.message}"


class Links:
    """Resolve a link written in one document to where it leads on a given surface."""

    def __init__(self, repo: Repo, docs: dict[str, Doc]):
        self.repo, self.docs = repo, docs

    def target(self, doc: Doc, href: str) -> tuple[str | None, str, str | None]:
        """(document or repository path, fragment, problem) for a relative link."""
        path_part, _, frag = href.partition("#")
        path_part = urllib.parse.unquote(path_part.partition("?")[0])
        if path_part.startswith("/"):
            target = path_part.lstrip("/")
        else:
            target = posixpath.normpath(posixpath.join(posixpath.dirname(doc.path), path_part))
        target = "" if target == "." else target.rstrip("/")
        if target == ".." or target.startswith("../"):
            return None, frag, f"leaves the repository: {href}"
        if target in self.docs or target in self.repo.tracked or target in self.repo.dirs:
            return target, frag, None
        return None, frag, f"no tracked file or directory: {href}"

    def problem(self, doc: Doc, href: str) -> str | None:
        if not href or SCHEME_RE.match(href) or href.startswith("//"):
            return None
        if href.startswith("#"):
            frag = urllib.parse.unquote(href[1:])
            if frag and frag not in doc.slugs:
                return f"no heading or anchor '{frag}' in this document"
            return None
        target, frag, problem = self.target(doc, href)
        if problem:
            return problem
        tdoc = self.docs.get(target) or self.docs.get(posixpath.join(target, "README.md"))
        frag = urllib.parse.unquote(frag)
        if tdoc and frag and frag not in tdoc.slugs and not frag.startswith("L"):
            return f"no heading or anchor '{frag}' in {tdoc.path}"
        return None

    def site(self, doc: Doc, href: str, image: bool = False) -> str:
        if not href or SCHEME_RE.match(href) or href.startswith(("//", "#")):
            return href
        target, frag, problem = self.target(doc, href)
        if problem or target is None:
            return href
        suffix = f"#{frag}" if frag else ""
        tdoc = self.docs.get(target) or (
            target in self.repo.dirs and self.docs.get(posixpath.join(target, "README.md"))
        )
        if tdoc and not image:
            return posixpath.relpath(tdoc.page, posixpath.dirname(doc.page) or ".") + suffix
        if image or target in self.repo.tracked and re.search(r"\.(png|jpe?g|gif|svg|webp)$", target, re.I):
            return self.repo.raw(target)
        if target in self.repo.dirs:
            return self.repo.tree(target) + suffix
        return self.repo.blob(target) + suffix

    def wiki(self, doc: Doc, href: str, image: bool = False) -> str:
        if not href or SCHEME_RE.match(href) or href.startswith(("//", "#")):
            return href
        target, frag, problem = self.target(doc, href)
        if problem or target is None:
            return href
        suffix = f"#{frag}" if frag else ""
        tdoc = self.docs.get(target) or (
            target in self.repo.dirs and self.docs.get(posixpath.join(target, "README.md"))
        )
        if tdoc and not image:
            return tdoc.wiki + suffix
        if image or re.search(r"\.(png|jpe?g|gif|svg|webp)$", target, re.I):
            return self.repo.raw(target)
        if target in self.repo.dirs:
            return self.repo.tree(target) + suffix
        return self.repo.blob(target) + suffix


# --------------------------------------------------------------------------- Markdown


def markdown() -> MarkdownIt:
    md = MarkdownIt("commonmark", {"html": True, "linkify": False, "typographer": False})
    md.enable(["table", "strikethrough"])
    return md


def inline_text(token) -> str:
    return "".join(
        c.content for c in (token.children or []) if c.type in ("text", "code_inline")
    )


def collect_slugs(md: MarkdownIt, doc: Doc) -> list:
    tokens = md.parse(doc.body)
    seen: dict[str, int] = {}
    doc.slugs, doc.headings = set(), []
    for i, tok in enumerate(tokens):
        if tok.type == "heading_open":
            text = inline_text(tokens[i + 1])
            base = github_slug(text)
            n = seen.get(base, 0)
            seen[base] = n + 1
            slug = base if n == 0 else f"{base}-{n}"
            tok.attrSet("id", slug)
            doc.slugs.add(slug)
            if tok.tag in ("h2", "h3"):
                doc.headings.append(text)
        elif tok.type in ("html_block", "html_inline"):
            doc.slugs.update(HTML_ID_RE.findall(tok.content))
        for child in tok.children or []:
            if child.type == "html_inline":
                doc.slugs.update(HTML_ID_RE.findall(child.content))
    return tokens


def walk_links(tokens):
    """Yield (token, attribute, line) for every link and image, raw HTML included."""
    for tok in tokens:
        line = (tok.map[0] + 1) if tok.map else 0
        if tok.type in ("html_block",):
            yield tok, None, line
        for child in tok.children or []:
            if child.type == "link_open":
                yield child, "href", line
            elif child.type == "image":
                yield child, "src", line
            elif child.type == "html_inline":
                yield child, None, line


def check_links(md: MarkdownIt, links: Links, doc: Doc, tokens) -> list[Finding]:
    found = []
    for tok, attr, line in walk_links(tokens):
        hrefs = [tok.attrGet(attr)] if attr else [m[2] for m in HTML_ATTR_RE.findall(tok.content)]
        for href in hrefs:
            problem = links.problem(doc, href or "")
            if problem:
                found.append(Finding("link", doc.path, line + doc.offset, problem))
    return found


def render_html(md: MarkdownIt, links: Links, doc: Doc, tokens) -> str:
    for tok, attr, _ in walk_links(tokens):
        if attr:
            tok.attrSet(attr, links.site(doc, tok.attrGet(attr) or "", image=attr == "src"))
        else:
            tok.content = HTML_ATTR_RE.sub(
                lambda m: m[1] + m[2] + html.escape(
                    links.site(doc, html.unescape(m[3]), image=m[1].strip().startswith("src")),
                    quote=True,
                ) + m[2],
                tok.content,
            )
    rules = md.renderer.rules
    default_fence = rules.get("fence")

    def fence(self, tokens, idx, options, env):
        tok = tokens[idx]
        if tok.info.strip().split(" ")[0] == "mermaid":
            src = html.escape(tok.content)
            return (
                '<figure class="diagram not-format relative my-6 rounded-lg border border-gray-200 '
                'bg-white p-4 dark:border-gray-700 dark:bg-gray-800">'
                '<button type="button" class="maximize absolute right-2 top-2 rounded-lg p-1.5 '
                'text-gray-500 hover:bg-gray-100 dark:text-gray-400 dark:hover:bg-gray-700" '
                'aria-label="Maximize">&#x26F6;</button>'
                f'<pre class="mermaid" data-src="{src}">{src}</pre></figure>'
            )
        return default_fence(tokens, idx, options, env)

    def table_open(self, tokens, idx, options, env):
        return '<div class="overflow-x-auto"><table>'

    def table_close(self, tokens, idx, options, env):
        return "</table></div>"

    md.add_render_rule("fence", fence)
    md.add_render_rule("table_open", table_open)
    md.add_render_rule("table_close", table_close)
    return md.renderer.render(tokens, md.options, {})


def rewrite_source_links(text: str, fn) -> str:
    """Rewrite link targets in Markdown source, leaving code fences and code spans alone."""
    out, in_fence, fence = [], False, ""
    links = re.compile(
        r"(?P<inl>(?P<open>!?\[[^\]]*\]\()(?P<target><[^>]*>|[^)\s]+))"
        r"""|(?P<attr>\s(?:href|src)=)(?P<q>["'])(?P<url>.*?)(?P=q)""",
        re.I,
    )
    refdef = re.compile(r"^(\s{0,3}\[[^\]]+\]:\s*)(\S+)")
    for line in text.splitlines(keepends=True):
        s = line.lstrip()
        if s.startswith(("```", "~~~")):
            mark = s[:3]
            if not in_fence:
                in_fence, fence = True, mark
            elif mark == fence:
                in_fence = False
            out.append(line)
            continue
        if in_fence:
            out.append(line)
            continue
        # A match that starts inside a code span is code, not a link; link text may contain one.
        spans = [m.span() for m in re.finditer(r"(`+)[^`]*?\1", line)]

        def outside(m: re.Match, rewrite) -> str:
            if any(a <= m.start() < b for a, b in spans):
                return m[0]
            return rewrite(m)

        ref = refdef.match(line)
        if ref:
            out.append(ref[1] + fn(ref[2], False) + line[ref.end():])
            continue

        def rewrite(m: re.Match) -> str:
            if m["inl"]:
                return m["open"] + fn(m["target"].strip("<>"), m["open"].startswith("!"))
            return m["attr"] + m["q"] + fn(m["url"], m["attr"].strip().lower().startswith("src")) + m["q"]

        out.append(links.sub(lambda m: outside(m, rewrite), line))
    return "".join(out)


# --------------------------------------------------------------------------- regions


def table(header: list[str], rows: list[list[str]]) -> str:
    esc = lambda s: str(s).replace("|", "\\|").replace("\n", " ")  # noqa: E731
    lines = ["| " + " | ".join(header) + " |", "|" + "|".join("---" for _ in header) + "|"]
    lines += ["| " + " | ".join(esc(c) for c in row) + " |" for row in rows]
    return "\n".join(lines)


def rel_md(doc: Doc, target: str) -> str:
    return posixpath.relpath(target, posixpath.dirname(doc.path) or ".")


class Regions:
    """Generators for the regions a document may carry. Each derives, none is hand-written."""

    def __init__(self, repo: Repo, docs: dict[str, Doc], layer: Layer):
        self.repo, self.docs, self.layer = repo, docs, layer

    def render(self, name: str, args: dict, doc: Doc) -> str:
        fn = getattr(self, "gen_" + name.replace("-", "_"), None)
        if fn is None:
            raise KeyError(name)
        return fn(args, doc).strip()

    def gen_docs_index(self, args: dict, doc: Doc) -> str:
        prefix = args.get("dir", "").strip("/")
        rows = []
        for d in self.docs.values():
            if d.path == doc.path or (prefix and not d.path.startswith(prefix + "/")):
                continue
            if args.get("depth") and d.path.count("/") - prefix.count("/") - (1 if prefix else 0) >= int(args["depth"]):
                continue
            rows.append([f"[{d.title}]({rel_md(doc, d.path)})", d.summary])
        return table(["Document", "What it is"], rows)

    def gen_ai_features(self, args: dict, doc: Doc) -> str:
        rows = [
            [f"[{f.get('short_title') or f.get('title')}]({rel_md(doc, f['path'])})",
             f.get("status", ""), f.get("headline") or f.get("summary", "")]
            for f in self.layer.features
        ]
        return table(["Feature", "Status", "What it gives"], rows)

    def gen_ai_adrs(self, args: dict, doc: Doc) -> str:
        rows = [
            [f"[{a.get('id', '')}]({rel_md(doc, a['path'])})", a.get("status", ""),
             a.get("title", "")]
            for a in self.layer.adrs
        ]
        return table(["Decision", "Status", "Title"], rows)

    def gen_ai_rules(self, args: dict, doc: Doc) -> str:
        origin = args.get("origin", "project")
        rows = [
            [f"[{r.get('id', '')}]({rel_md(doc, r['path'])})", r.get("class", ""),
             r.get("statement") or r.get("title", "")]
            for r in self.layer.rules
            if origin == "all" or r["origin"] == origin
        ]
        return table(["Rule", "Class", "Statement"], rows)

    def gen_ai_plan(self, args: dict, doc: Doc) -> str:
        rows = []
        for m in self.layer.milestones:
            issues = [i for i in self.layer.issues if i.get("milestone") == m.get("id")]
            done = sum(i["status"] == "done" for i in issues)
            rows.append([
                f"[{m.get('title', m.get('id'))}]({rel_md(doc, m['path'])})",
                f"{done}/{len(issues)}",
                ", ".join(i["id"] for i in issues if i["status"] != "done") or "—",
            ])
        return table(["Milestone", "Issues done", "Open issues"], rows)

    def gen_surfaces(self, args: dict, doc: Doc) -> str:
        r = self.repo
        rows = [
            ["Site", f"[{r.pages_url}]({r.pages_url})",
             "every published document, the .ai/ layer, charts, search"],
            ["Wiki", f"[{r.wiki_url}]({r.wiki_url})", "the same documents as wiki pages"],
            ["Markdown regions", "this repository",
             "tables like this one, regenerated by `tools/surfaces/surfaces sync`"],
        ]
        return table(["Surface", "Where", "What it carries"], rows)


def fenced_ranges(text: str) -> list[tuple[int, int]]:
    """Character ranges of the fenced code blocks in a Markdown text."""
    ranges, pos, start, fence = [], 0, None, ""
    for line in text.splitlines(keepends=True):
        s = line.lstrip()
        if s.startswith(("```", "~~~")):
            mark = s[:3]
            if start is None:
                start, fence = pos, mark
            elif mark == fence:
                ranges.append((start, pos + len(line)))
                start = None
        pos += len(line)
    if start is not None:
        ranges.append((start, len(text)))
    return ranges


def parse_args(raw: str) -> dict:
    return dict(kv.split("=", 1) for kv in raw.split())


def sync_regions(repo: Repo, docs: dict[str, Doc], layer: Layer, write: bool) -> tuple[list[Finding], list[dict]]:
    gens = Regions(repo, docs, layer)
    findings, report = [], []
    for doc in docs.values():
        if "<!-- surfaces:begin" not in doc.text:
            continue

        fenced = fenced_ranges(doc.text)

        def replace(m: re.Match) -> str:
            if any(a <= m.start() < b for a, b in fenced):
                return m[0]  # an example inside a code block, not a region
            line = doc.text[: m.start()].count("\n") + 1
            try:
                body = gens.render(m["name"], parse_args(m["args"]), doc)
            except KeyError:
                findings.append(Finding("region", doc.path, line, f"unknown generator '{m['name']}'"))
                return m[0]
            fresh = m[1] + body + "\n" + m[5]
            state = "current" if fresh == m[0] else "stale"
            report.append({"path": doc.path, "line": line, "name": m["name"], "state": state})
            if state == "stale" and not write:
                findings.append(Finding(
                    "region", doc.path, line,
                    f"region '{m['name']}' is stale: run tools/surfaces/surfaces sync"))
            return fresh

        new = REGION_RE.sub(replace, doc.text)
        if write and new != doc.text:
            (repo.root / doc.path).write_text(new, encoding="utf-8")
            doc.text = new
            doc.fm, doc.body, doc.offset = split_front_matter(new)
    return findings, report


# --------------------------------------------------------------------------- site


def esc(s) -> str:
    return html.escape(str(s), quote=True)


def fmt_date(ts: int) -> str:
    return dt.datetime.fromtimestamp(ts, dt.timezone.utc).strftime("%Y-%m-%d")


ICON = {
    "home": '<path d="M10.707 2.293a1 1 0 0 0-1.414 0l-7 7a1 1 0 0 0 1.414 1.414L4 10.414V17a1 1 0 0 0 1 1h2a1 1 0 0 0 1-1v-2a1 1 0 0 1 1-1h2a1 1 0 0 1 1 1v2a1 1 0 0 0 1 1h2a1 1 0 0 0 1-1v-6.586l.293.293a1 1 0 0 0 1.414-1.414l-7-7Z"/>',
    "doc": '<path fill-rule="evenodd" d="M4 4a2 2 0 0 1 2-2h4.586A2 2 0 0 1 12 2.586L15.414 6A2 2 0 0 1 16 7.414V16a2 2 0 0 1-2 2H6a2 2 0 0 1-2-2V4Zm2 6a1 1 0 0 1 1-1h6a1 1 0 1 1 0 2H7a1 1 0 0 1-1-1Zm1 3a1 1 0 1 0 0 2h6a1 1 0 1 0 0-2H7Z" clip-rule="evenodd"/>',
    "layer": '<path d="M7 3a1 1 0 0 0 0 2h6a1 1 0 1 0 0-2H7ZM4 7a1 1 0 0 1 1-1h10a1 1 0 1 1 0 2H5a1 1 0 0 1-1-1ZM2 11a2 2 0 0 1 2-2h12a2 2 0 0 1 2 2v4a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2v-4Z"/>',
    "folder": '<path d="M2 6a2 2 0 0 1 2-2h5l2 2h5a2 2 0 0 1 2 2v6a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V6Z"/>',
    "chevron": '<path fill-rule="evenodd" d="M5.293 7.293a1 1 0 0 1 1.414 0L10 10.586l3.293-3.293a1 1 0 1 1 1.414 1.414l-4 4a1 1 0 0 1-1.414 0l-4-4a1 1 0 0 1 0-1.414Z" clip-rule="evenodd"/>',
    "code": '<path fill-rule="evenodd" d="M12.316 3.051a1 1 0 0 1 .633 1.265l-4 12a1 1 0 1 1-1.898-.632l4-12a1 1 0 0 1 1.265-.633ZM5.707 6.293a1 1 0 0 1 0 1.414L3.414 10l2.293 2.293a1 1 0 1 1-1.414 1.414l-3-3a1 1 0 0 1 0-1.414l3-3a1 1 0 0 1 1.414 0Zm8.586 0a1 1 0 0 1 1.414 0l3 3a1 1 0 0 1 0 1.414l-3 3a1 1 0 1 1-1.414-1.414L16.586 10l-2.293-2.293a1 1 0 0 1 0-1.414Z" clip-rule="evenodd"/>',
    "book": '<path d="M9 4.804A7.968 7.968 0 0 0 5.5 4c-1.255 0-2.443.29-3.5.804v10A7.969 7.969 0 0 1 5.5 14c1.669 0 3.218.51 4.5 1.385A7.962 7.962 0 0 1 14.5 14c1.255 0 2.443.29 3.5.804v-10A7.968 7.968 0 0 0 14.5 4c-1.255 0-2.443.29-3.5.804V12a1 1 0 1 1-2 0V4.804Z"/>',
    "search": '<path fill-rule="evenodd" d="M8 4a4 4 0 1 0 0 8 4 4 0 0 0 0-8ZM2 8a6 6 0 1 1 10.89 3.476l4.817 4.817a1 1 0 0 1-1.414 1.414l-4.816-4.816A6 6 0 0 1 2 8Z" clip-rule="evenodd"/>',
    "moon": '<path d="M17.293 13.293A8 8 0 0 1 6.707 2.707a8.001 8.001 0 1 0 10.586 10.586Z"/>',
    "sun": '<path fill-rule="evenodd" d="M10 2a1 1 0 0 1 1 1v1a1 1 0 1 1-2 0V3a1 1 0 0 1 1-1Zm4 8a4 4 0 1 1-8 0 4 4 0 0 1 8 0Zm-.464 4.95.707.707a1 1 0 0 0 1.414-1.414l-.707-.707a1 1 0 0 0-1.414 1.414Zm2.12-10.607a1 1 0 0 1 0 1.414l-.706.707a1 1 0 1 1-1.414-1.414l.707-.707a1 1 0 0 1 1.414 0ZM17 11a1 1 0 1 0 0-2h-1a1 1 0 1 0 0 2h1Zm-7 4a1 1 0 0 1 1 1v1a1 1 0 1 1-2 0v-1a1 1 0 0 1 1-1ZM5.05 6.464A1 1 0 1 0 6.464 5.05l-.707-.707a1 1 0 0 0-1.414 1.414l.707.707Zm1.414 8.486-.707.707a1 1 0 0 1-1.414-1.414l.707-.707a1 1 0 0 1 1.414 1.414ZM4 11a1 1 0 1 0 0-2H3a1 1 0 0 0 0 2h1Z" clip-rule="evenodd"/>',
    "menu": '<path fill-rule="evenodd" d="M3 5a1 1 0 0 1 1-1h12a1 1 0 1 1 0 2H4a1 1 0 0 1-1-1Zm0 5a1 1 0 0 1 1-1h12a1 1 0 1 1 0 2H4a1 1 0 0 1-1-1Zm0 5a1 1 0 0 1 1-1h12a1 1 0 1 1 0 2H4a1 1 0 0 1-1-1Z" clip-rule="evenodd"/>',
}


def icon(name: str, cls: str = "h-5 w-5") -> str:
    return f'<svg class="{cls}" fill="currentColor" viewBox="0 0 20 20" aria-hidden="true">{ICON[name]}</svg>'


BTN_PRIMARY = (
    "inline-flex items-center rounded-lg bg-primary-700 px-3 py-2 text-center text-sm font-medium "
    "text-white hover:bg-primary-800 focus:ring-4 focus:ring-primary-300 dark:bg-primary-600 "
    "dark:hover:bg-primary-700 dark:focus:ring-primary-800"
)
BTN_SECONDARY = (
    "inline-flex items-center rounded-lg border border-gray-300 bg-white px-3 py-2 text-center "
    "text-sm font-medium text-gray-900 hover:bg-gray-100 focus:ring-4 focus:ring-primary-300 "
    "dark:border-gray-600 dark:bg-gray-800 dark:text-gray-400 dark:hover:bg-gray-700 "
    "dark:hover:text-white dark:focus:ring-gray-700"
)
CARD = "rounded-lg bg-white p-4 shadow-sm sm:p-6 dark:bg-gray-800"
BADGE = {
    "done": "bg-green-100 text-green-800 dark:bg-green-900 dark:text-green-300",
    "active": "bg-blue-100 text-blue-800 dark:bg-blue-900 dark:text-blue-300",
    "accepted": "bg-green-100 text-green-800 dark:bg-green-900 dark:text-green-300",
    "proposed": "bg-yellow-100 text-yellow-800 dark:bg-yellow-900 dark:text-yellow-300",
    "superseded": "bg-gray-100 text-gray-800 dark:bg-gray-700 dark:text-gray-300",
    "blocking": "bg-red-100 text-red-800 dark:bg-red-900 dark:text-red-300",
}
BADGE_DEFAULT = "bg-gray-100 text-gray-800 dark:bg-gray-700 dark:text-gray-300"


def badge(text: str) -> str:
    cls = BADGE.get(str(text).lower(), BADGE_DEFAULT)
    return f'<span class="me-2 rounded-md px-2.5 py-0.5 text-xs font-medium {cls}">{esc(text)}</span>'


TABLE = "w-full text-left text-sm text-gray-500 dark:text-gray-400"
THEAD = "bg-gray-50 text-xs uppercase text-gray-700 dark:bg-gray-700 dark:text-gray-400"
TR = "border-b border-gray-200 bg-white hover:bg-gray-50 dark:border-gray-700 dark:bg-gray-800 dark:hover:bg-gray-700"


def html_table(header: list[str], rows: list[list[str]], filterable: str = "") -> str:
    head = "".join(f'<th scope="col" class="px-4 py-3">{esc(h)}</th>' for h in header)
    body = "".join(
        f'<tr class="{TR}">' + "".join(f'<td class="px-4 py-3">{c}</td>' for c in r) + "</tr>"
        for r in rows
    )
    attr = f' data-filter="{filterable}"' if filterable else ""
    return (
        f'<div class="relative overflow-x-auto rounded-lg border border-gray-200 dark:border-gray-700">'
        f'<table class="{TABLE}"{attr}><thead class="{THEAD}"><tr>{head}</tr></thead>'
        f"<tbody>{body}</tbody></table></div>"
    )


class Site:
    def __init__(self, repo: Repo, cfg: dict, docs: dict[str, Doc], layer: Layer, out: Path):
        self.repo, self.cfg, self.docs, self.layer, self.out = repo, cfg, docs, layer, out
        self.links = Links(repo, docs)
        root_readme = docs.get("README.md")
        self.title = (root_readme.title if root_readme else repo.name) or repo.name
        self.summary = root_readme.summary if root_readme else ""

    # -- shell --------------------------------------------------------------

    def link(self, here: str, target: str) -> str:
        return posixpath.relpath(target, posixpath.dirname(here) or ".")

    def sidebar(self, here: str, active: Doc | None) -> str:
        item, on = "group nav-item", " nav-on"
        top = [("index.html", "Overview", "home"), ("docs.html", "All documents", "doc")]
        if self.layer.present:
            top.append(("ai.html", "AI layer", "layer"))
        parts = ['<ul class="space-y-2 pb-2">']
        for target, label, ic in top:
            cls = item + (on if here == target and (active is None or target != "index.html") else "")
            parts.append(
                f'<li><a href="{self.link(here, target)}" class="{cls}">'
                f'{icon(ic, "nav-icon")}'
                f'<span class="ms-3">{esc(label)}</span></a></li>'
            )
        parts.append("</ul>")
        groups: dict[str, list[Doc]] = {}
        for d in self.docs.values():
            groups.setdefault(d.group, []).append(d)
        parts.append('<ul class="space-y-2 pt-2">')
        for g in sorted(groups, key=lambda g: (g != "", g.startswith("."), g.lower())):
            gid = "nav-" + re.sub(r"[^a-z0-9]+", "-", (g or "root").lower())
            expanded = active is not None and active.group == g
            parts.append(
                f'<li><button type="button" class="{item} w-full" aria-controls="{gid}" '
                f'data-collapse-toggle="{gid}" aria-expanded="{str(expanded).lower()}">'
                f'{icon("folder", "nav-icon")}'
                f'<span class="ms-3 flex-1 whitespace-nowrap text-left">{esc(g or "/")}</span>'
                f'<span class="me-2 rounded-full bg-gray-100 px-2 text-xs dark:bg-gray-700">{len(groups[g])}</span>'
                f'{icon("chevron", "h-4 w-4")}</button>'
                f'<ul id="{gid}" class="{"" if expanded else "hidden "}space-y-1 py-1">'
            )
            for d in groups[g]:
                cls = "nav-doc" + (on if active is d else "")
                label = d.path[len(g) + 1 :] if g else d.path
                parts.append(
                    f'<li><a href="{self.link(here, d.page)}" class="{cls}" title="{esc(d.title)}">'
                    f'<span class="truncate">{esc(label)}</span></a></li>'
                )
            parts.append("</ul></li>")
        parts.append("</ul>")
        return "".join(parts)

    def shell(self, here: str, title: str, content: str, active: Doc | None = None,
              crumbs: list[tuple[str, str]] = (), actions: str = "", charts: bool = False) -> str:
        root = posixpath.relpath(".", posixpath.dirname(here) or ".")
        root = "" if root == "." else root + "/"
        crumb = "".join(
            f'<li><div class="flex items-center"><span class="mx-1 text-gray-400">/</span>'
            + (f'<a href="{self.link(here, t)}" class="ms-1 text-sm font-medium text-gray-700 '
               f'hover:text-primary-600 dark:text-gray-400 dark:hover:text-white">{esc(n)}</a>'
               if t else f'<span class="ms-1 text-sm font-medium text-gray-500 dark:text-gray-400" '
               f'aria-current="page">{esc(n)}</span>')
            + "</div></li>"
            for n, t in crumbs
        )
        sha = self.repo.sha[:10]
        return f"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{esc(title)} · {esc(self.repo.name)}</title>
<meta name="description" content="{esc(self.summary)}">
<meta name="generator" content="tools/surfaces/surfaces site @ {esc(sha)}">
<script>(function(){{try{{var t=localStorage.getItem('color-theme');if(t==='dark'||(!t&&matchMedia('(prefers-color-scheme: dark)').matches))document.documentElement.classList.add('dark')}}catch(e){{}}}})()</script>
<link rel="stylesheet" href="{root}css/app.css">
</head>
<body class="bg-gray-50 dark:bg-gray-900" data-root="{root}" data-charts="{str(charts).lower()}">
<nav class="fixed z-30 w-full border-b border-gray-200 bg-white dark:border-gray-700 dark:bg-gray-800">
  <div class="px-3 py-3 lg:px-5 lg:ps-3"><div class="flex items-center justify-between">
    <div class="flex items-center justify-start">
      <button data-drawer-target="sidebar" data-drawer-toggle="sidebar" aria-controls="sidebar" type="button" class="me-2 rounded-lg p-2 text-gray-600 hover:bg-gray-100 hover:text-gray-900 lg:hidden dark:text-gray-400 dark:hover:bg-gray-700 dark:hover:text-white"><span class="sr-only">Open sidebar</span>{icon("menu", "h-6 w-6")}</button>
      <a href="{root}index.html" class="me-8 flex items-center"><span class="self-center whitespace-nowrap text-xl font-semibold dark:text-white">{esc(self.repo.name)}</span></a>
      <form class="hidden lg:block" role="search" onsubmit="return false">
        <label for="search" class="sr-only">Search</label>
        <div class="relative w-96">
          <div class="pointer-events-none absolute inset-y-0 start-0 flex items-center ps-3 text-gray-500 dark:text-gray-400">{icon("search")}</div>
          <input type="search" id="search" autocomplete="off" placeholder="Search {len(self.docs)} documents  ( / )" class="block w-full rounded-lg border border-gray-300 bg-gray-50 p-2.5 ps-10 text-sm text-gray-900 focus:border-primary-500 focus:ring-primary-500 dark:border-gray-600 dark:bg-gray-700 dark:text-white dark:placeholder-gray-400">
          <div id="search-results" class="absolute z-50 mt-1 hidden max-h-96 w-full overflow-y-auto rounded-lg bg-white shadow-lg dark:bg-gray-700"></div>
        </div>
      </form>
    </div>
    <div class="flex items-center gap-1">
      <a href="{esc(self.repo.wiki_url)}" class="rounded-lg p-2 text-gray-500 hover:bg-gray-100 hover:text-gray-900 dark:text-gray-400 dark:hover:bg-gray-700 dark:hover:text-white" title="Wiki">{icon("book")}</a>
      <a href="{esc(self.repo.web)}" class="rounded-lg p-2 text-gray-500 hover:bg-gray-100 hover:text-gray-900 dark:text-gray-400 dark:hover:bg-gray-700 dark:hover:text-white" title="Repository">{icon("code")}</a>
      <button id="theme-toggle" type="button" class="rounded-lg p-2.5 text-sm text-gray-500 hover:bg-gray-100 focus:outline-none focus:ring-4 focus:ring-gray-200 dark:text-gray-400 dark:hover:bg-gray-700 dark:focus:ring-gray-700" title="Toggle dark mode"><span class="dark:hidden">{icon("moon")}</span><span class="hidden dark:inline">{icon("sun")}</span></button>
    </div>
  </div></div>
</nav>
<aside id="sidebar" class="fixed left-0 top-0 z-20 h-full w-72 -translate-x-full border-r border-gray-200 bg-white pt-16 transition-transform lg:translate-x-0 dark:border-gray-700 dark:bg-gray-800" aria-label="Sidebar">
  <div class="h-full overflow-y-auto px-3 pb-4 pt-5"><div class="divide-y divide-gray-200 dark:divide-gray-700">{self.sidebar(here, active)}</div></div>
</aside>
<div id="main-content" class="relative h-full w-full overflow-y-auto bg-gray-50 pt-16 lg:ms-72 lg:w-[calc(100%-18rem)] dark:bg-gray-900">
<main class="px-4 pt-6">
  <div class="mb-4">
    <nav class="mb-3 flex" aria-label="Breadcrumb"><ol class="inline-flex items-center">
      <li class="inline-flex items-center"><a href="{root}index.html" class="inline-flex items-center text-sm font-medium text-gray-700 hover:text-primary-600 dark:text-gray-400 dark:hover:text-white">{icon("home", "me-2 h-4 w-4")}Home</a></li>{crumb}
    </ol></nav>
    <div class="flex flex-wrap items-center justify-between gap-3">
      <h1 class="text-xl font-semibold text-gray-900 sm:text-2xl dark:text-white">{esc(title)}</h1>
      <div class="flex flex-wrap gap-2">{actions}</div>
    </div>
  </div>
  {content}
</main>
<footer class="mx-4 my-6 rounded-lg bg-white p-4 text-sm text-gray-500 shadow-sm md:flex md:items-center md:justify-between dark:bg-gray-800 dark:text-gray-400">
  <span>Generated from <a class="font-medium text-primary-600 hover:underline dark:text-primary-500" href="{esc(self.repo.web)}/commit/{esc(self.repo.sha)}">{esc(sha)}</a> by <code>tools/surfaces/surfaces</code>. Every page is derived; edit the source document, never this site.</span>
  <span class="mt-2 block md:mt-0"><a class="hover:underline" href="{esc(self.repo.web)}">{esc(self.repo.slug)}</a> · <a class="hover:underline" href="{esc(self.repo.wiki_url)}">wiki</a></span>
</footer>
</div>
<script src="{root}js/flowbite.min.js"></script>
<script src="{root}js/app.js"></script>
</body>
</html>
"""

    # -- pages --------------------------------------------------------------

    def front_matter_card(self, doc: Doc) -> str:
        if not doc.fm:
            return ""
        rows = []
        for k, v in doc.fm.items():
            if isinstance(v, (list, dict)):
                v = json.dumps(v, default=str, ensure_ascii=False)
            rows.append(
                f'<div class="grid grid-cols-3 gap-2 border-b border-gray-100 py-1.5 last:border-0 dark:border-gray-700">'
                f'<dt class="font-medium text-gray-900 dark:text-white">{esc(k)}</dt>'
                f'<dd class="col-span-2 break-words text-gray-500 dark:text-gray-400">{esc(v)}</dd></div>'
            )
        return (
            f'<details class="{CARD} mb-4"><summary class="cursor-pointer text-sm font-semibold '
            f'text-gray-900 dark:text-white">Record metadata ({len(doc.fm)} fields)</summary>'
            f'<dl class="mt-3 text-sm">{"".join(rows)}</dl></details>'
        )

    def article(self, md: MarkdownIt, doc: Doc) -> str:
        tokens = collect_slugs(md, doc)
        body = render_html(md, self.links, doc, tokens)
        return (
            f'<article class="{CARD} format format-blue max-w-none lg:format-lg dark:format-invert">'
            f"{body}</article>"
        )

    def doc_actions(self, doc: Doc) -> str:
        return (
            f'<a class="{BTN_SECONDARY}" href="{esc(self.repo.blob(doc.path))}">Source</a>'
            f'<a class="{BTN_SECONDARY}" href="{esc(self.repo.wiki_url)}/{esc(doc.wiki)}">Wiki</a>'
            f'<a class="{BTN_PRIMARY}" href="{esc(self.repo.edit(doc.path))}">Edit</a>'
        )

    def doc_page(self, md: MarkdownIt, doc: Doc) -> str:
        meta = (
            f'<div class="mb-4 flex flex-wrap items-center gap-y-2 text-sm text-gray-500 dark:text-gray-400">'
            f'{badge(doc.group or "/")}<code class="me-3">{esc(doc.path)}</code>'
            f'<span class="me-3">{doc.lines} lines</span><span>changed {fmt_date(doc.changed)}</span></div>'
        )
        crumbs = [(part, "") for part in doc.path.split("/")[:-1]] + [(posixpath.basename(doc.path), "")]
        return self.shell(
            doc.page, doc.title,
            meta + self.front_matter_card(doc) + self.article(md, doc),
            active=doc, crumbs=crumbs, actions=self.doc_actions(doc),
        )

    def kpis(self) -> str:
        lay = self.layer
        done = sum(i["status"] == "done" for i in lay.issues)
        cards = [
            (len(self.docs), "Markdown documents"),
            (sum(d.lines for d in self.docs.values()), "lines of documentation"),
        ]
        if lay.present:
            cards += [
                (len(lay.adrs), "architecture decisions"),
                (f"{done}/{len(lay.issues)}", "plan issues done"),
                (len([r for r in lay.rules if r["origin"] == "project"]), "project rules"),
                (len(lay.features), "features"),
            ]
        return '<div class="mb-4 grid grid-cols-2 gap-4 md:grid-cols-3 xl:grid-cols-6">' + "".join(
            f'<div class="{CARD}"><span class="text-2xl font-bold leading-none text-gray-900 sm:text-3xl '
            f'dark:text-white">{esc(v)}</span><h3 class="text-base font-normal text-gray-500 '
            f'dark:text-gray-400">{esc(k)}</h3></div>'
            for v, k in cards
        ) + "</div>"

    def chart_card(self, cid: str, title: str, subtitle: str, controls: str = "", span: str = "") -> str:
        return (
            f'<section id="card-{cid}" class="chart-card {CARD} {span} relative">'
            f'<div class="mb-4 flex flex-wrap items-start justify-between gap-2"><div>'
            f'<h3 class="text-xl font-bold leading-none text-gray-900 dark:text-white">{esc(title)}</h3>'
            f'<p class="mt-1 text-sm text-gray-500 dark:text-gray-400">{esc(subtitle)}</p></div>'
            f'<div class="flex items-center gap-2">{controls}'
            f'<button type="button" class="maximize rounded-lg p-1.5 text-gray-500 hover:bg-gray-100 '
            f'dark:text-gray-400 dark:hover:bg-gray-700" aria-label="Maximize" title="Maximize (Esc to close)">&#x26F6;</button>'
            f"</div></div>"
            f'<div class="chart h-80 w-full" data-chart="{cid}"></div></section>'
        )

    def select(self, param: str, options: list[tuple[str, str]], default: str) -> str:
        opts = "".join(
            f'<option value="{esc(v)}"{" selected" if v == default else ""}>{esc(l)}</option>'
            for v, l in options
        )
        return (
            f'<select data-param="{esc(param)}" class="rounded-lg border border-gray-300 bg-gray-50 p-1.5 '
            f'text-sm text-gray-900 focus:border-primary-500 focus:ring-primary-500 dark:border-gray-600 '
            f'dark:bg-gray-700 dark:text-white">{opts}</select>'
        )

    def index_page(self, md: MarkdownIt) -> str:
        readme = self.docs.get("README.md")
        hero = (
            f'<div class="{CARD} mb-4"><p class="mb-4 text-lg text-gray-500 dark:text-gray-400">'
            f"{esc(self.summary)}</p><div class=\"flex flex-wrap gap-2\">"
            f'<a class="{BTN_PRIMARY}" href="docs.html">Browse {len(self.docs)} documents</a>'
            + (f'<a class="{BTN_SECONDARY}" href="ai.html">AI layer</a>' if self.layer.present else "")
            + f'<a class="{BTN_SECONDARY}" href="{esc(self.repo.web)}">Repository</a>'
            f'<a class="{BTN_SECONDARY}" href="{esc(self.repo.wiki_url)}">Wiki</a></div></div>'
        )
        charts = (
            '<div class="mb-4 grid grid-cols-1 gap-4 xl:grid-cols-2">'
            + self.chart_card("activity", "Commit activity", "Commits per week up to HEAD",
                              self.select("activity.weeks", [("13", "13 weeks"), ("26", "26 weeks"),
                                                             ("52", "52 weeks"), ("104", "104 weeks"),
                                                             ("0", "all")], "52"))
            + self.chart_card("docs", "Documentation map", "Click a document to open it",
                              self.select("docs.metric", [("lines", "by lines"), ("count", "by documents")], "lines"))
            + self.chart_card("freshness", "Least recently changed", "Days since a document last changed, at HEAD",
                              self.select("freshness.n", [("10", "top 10"), ("20", "top 20"), ("40", "top 40")], "20"))
            + (self.chart_card("plan", "Plan progress", "Issues per milestone, by derived status")
               if self.layer.milestones else "")
            + "</div>"
        )
        body = self.article(md, readme) if readme else ""
        return self.shell("index.html", self.title, hero + self.kpis() + charts + body,
                          actions=self.doc_actions(readme) if readme else "", charts=True)

    def docs_page(self) -> str:
        rows = [
            [f'<a class="font-medium text-primary-600 hover:underline dark:text-primary-500" href="{esc(d.page)}">{esc(d.title)}</a>',
             f"<code>{esc(d.path)}</code>", str(d.lines), fmt_date(d.changed), esc(d.summary)]
            for d in sorted(self.docs.values(), key=lambda d: d.path.lower())
        ]
        filt = (
            '<div class="mb-4"><input type="search" data-filter-for="all-docs" placeholder="Filter by title, path or summary" '
            'class="block w-full rounded-lg border border-gray-300 bg-gray-50 p-2.5 text-sm text-gray-900 '
            'focus:border-primary-500 focus:ring-primary-500 md:w-96 dark:border-gray-600 dark:bg-gray-700 dark:text-white"></div>'
        )
        content = (
            '<div class="mb-4 grid grid-cols-1 gap-4">'
            + self.chart_card("docs", "Documentation map", "Area by size; click a document to open it",
                              self.select("docs.metric", [("lines", "by lines"), ("count", "by documents")], "lines"))
            + "</div>" + f'<div class="{CARD}">' + filt
            + html_table(["Title", "Path", "Lines", "Changed", "Summary"], rows, "all-docs") + "</div>"
        )
        return self.shell("docs.html", "All documents", content, crumbs=[("All documents", "")], charts=True)

    def page_of(self, path: str, here: str) -> str:
        d = self.docs.get(path)
        return self.link(here, d.page) if d else self.repo.blob(path)

    def ai_page(self) -> str:
        lay, here = self.layer, "ai.html"
        a = lambda path, text: (  # noqa: E731
            f'<a class="font-medium text-primary-600 hover:underline dark:text-primary-500" '
            f'href="{esc(self.page_of(path, here))}">{esc(text)}</a>'
        )
        parts = []
        if lay.features:
            parts.append('<h2 class="mb-4 mt-2 text-lg font-semibold text-gray-900 dark:text-white">Features</h2>'
                         '<div class="mb-6 grid grid-cols-1 gap-4 md:grid-cols-2 xl:grid-cols-3">')
            for f in lay.features:
                parts.append(
                    f'<div class="{CARD}"><div class="mb-2 flex items-center justify-between">'
                    f'<h3 class="text-lg font-bold text-gray-900 dark:text-white">'
                    f'{a(f["path"], f.get("short_title") or f.get("title", ""))}</h3>{badge(f.get("status", ""))}</div>'
                    f'<p class="text-sm text-gray-500 dark:text-gray-400">{esc(f.get("headline") or f.get("summary", ""))}</p></div>'
                )
            parts.append("</div>")
        if lay.milestones:
            parts.append('<h2 class="mb-4 text-lg font-semibold text-gray-900 dark:text-white">Plan</h2>'
                         '<div class="mb-4 grid grid-cols-1 gap-4">'
                         + self.chart_card("plan", "Plan progress", "Issues per milestone, by derived status")
                         + "</div>")
            rows = []
            for m in lay.milestones:
                issues = [i for i in lay.issues if i.get("milestone") == m.get("id")]
                done = sum(i["status"] == "done" for i in issues)
                pct = int(100 * done / len(issues)) if issues else 0
                bar = (f'<div class="h-2.5 w-32 rounded-full bg-gray-200 dark:bg-gray-700">'
                       f'<div class="h-2.5 rounded-full bg-primary-600" style="width:{pct}%"></div></div>')
                rows.append([a(m["path"], m.get("title", m.get("id"))), f"{done}/{len(issues)}", bar])
            parts.append(f'<div class="{CARD} mb-4">' + html_table(["Milestone", "Done", "Progress"], rows) + "</div>")
            graph = ["flowchart LR"]
            for i in lay.issues:
                label = (i.get("title") or i["id"]).replace('"', "'")
                if len(label) > 48:
                    label = label[:45] + "..."
                graph.append(f'  {i["id"]}["{i["id"]}: {label}"]:::{i["status"]}')
                for dep in i.get("depends_on") or []:
                    graph.append(f"  {dep} --> {i['id']}")
            graph += ["  classDef done fill:#def7ec,stroke:#31c48d,color:#03543f",
                      "  classDef active fill:#e1effe,stroke:#3f83f8,color:#1e429f",
                      "  classDef open fill:#f3f4f6,stroke:#9ca3af,color:#111827"]
            src = esc("\n".join(graph))
            parts.append(
                f'<figure class="diagram relative mb-6 {CARD}"><div class="mb-2 flex justify-between">'
                f'<h3 class="text-xl font-bold text-gray-900 dark:text-white">Issue dependency graph</h3>'
                f'<button type="button" class="maximize rounded-lg p-1.5 text-gray-500 hover:bg-gray-100 dark:text-gray-400 '
                f'dark:hover:bg-gray-700" aria-label="Maximize">&#x26F6;</button></div>'
                f'<pre class="mermaid overflow-x-auto" data-src="{src}">{src}</pre></figure>'
            )
            rows = [[a(i["path"], i["id"]), badge(i["status"]), esc(i.get("title", "")), esc(i.get("milestone", ""))]
                    for i in lay.issues]
            parts.append(f'<div class="{CARD} mb-6">' + html_table(["Issue", "Status", "Title", "Milestone"], rows) + "</div>")
        if lay.adrs:
            rows = [[a(x["path"], x.get("id", "")), badge(x.get("status", "")), esc(x.get("title", "")),
                     esc(x.get("date", ""))] for x in lay.adrs]
            parts.append('<h2 class="mb-4 text-lg font-semibold text-gray-900 dark:text-white">Architecture decisions</h2>'
                         f'<div class="{CARD} mb-6">' + html_table(["Decision", "Status", "Title", "Date"], rows) + "</div>")
        if lay.rules:
            rows = [[a(r["path"], r.get("id", "")), badge(r.get("class", "")), badge(r["origin"]),
                     esc(r.get("statement") or r.get("title", ""))] for r in lay.rules]
            parts.append('<h2 class="mb-4 text-lg font-semibold text-gray-900 dark:text-white">Rules</h2>'
                         f'<div class="{CARD} mb-6">' + html_table(["Rule", "Class", "Origin", "Statement"], rows) + "</div>")
        return self.shell(here, "AI layer", "".join(parts), crumbs=[("AI layer", "")], charts=True)

    # -- data ---------------------------------------------------------------

    def data(self) -> dict:
        times = [int(t) for t in git(self.repo.root, "log", "--format=%ct", "HEAD").split()]
        week = 7 * 86400
        head = self.repo.head_time
        counts: dict[int, int] = {}
        for t in times:
            counts[(head - t) // week] = counts.get((head - t) // week, 0) + 1
        span = max(counts) + 1 if counts else 0
        activity = [
            [fmt_date(head - k * week), counts.get(k, 0)] for k in range(span - 1, -1, -1)
        ]
        tree: dict = {}
        for d in self.docs.values():
            node = tree
            for part in d.path.split("/")[:-1]:
                node = node.setdefault(part, {})
            node[posixpath.basename(d.path)] = d
        def to_echarts(name: str, node) -> dict:
            if isinstance(node, Doc):
                return {"name": name, "value": node.lines, "lines": node.lines, "url": node.page,
                        "title": node.title}
            return {"name": name, "children": [to_echarts(k, v) for k, v in sorted(node.items())]}
        plan = [
            {"milestone": m.get("title", m.get("id")), "id": m.get("id"), "url": self.page_of(m["path"], "index.html"),
             **{s: sum(1 for i in self.layer.issues if i.get("milestone") == m.get("id") and i["status"] == s)
                for s in ("done", "active", "open")}}
            for m in self.layer.milestones
        ]
        return {
            "repository": self.repo.slug,
            "built_from": self.repo.sha,
            "history": "shallow" if self.repo.shallow else "full",
            "activity": activity,
            "docs": to_echarts("/", tree)["children"],
            "freshness": sorted(
                ({"title": d.title, "path": d.path, "url": d.page, "days": (head - d.changed) // 86400}
                 for d in self.docs.values()),
                key=lambda x: -x["days"],
            ),
            "plan": plan,
        }

    def search_index(self) -> list[dict]:
        return [
            {"t": d.title, "p": d.path, "u": d.page, "s": d.summary, "h": d.headings[:30]}
            for d in self.docs.values()
        ]

    # -- build --------------------------------------------------------------

    def write(self, rel: str, text: str) -> None:
        path = self.out / rel
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")

    def build(self, assets: bool) -> None:
        if self.out.exists():
            shutil.rmtree(self.out)
        self.out.mkdir(parents=True)
        stamp_changed(self.repo, self.docs)
        md = markdown()
        for d in self.docs.values():
            collect_slugs(md, d)
        for d in self.docs.values():
            if d.path != "README.md":
                self.write(d.page, self.doc_page(md, d))
        self.write("index.html", self.index_page(md))
        self.write("docs.html", self.docs_page())
        if self.layer.present:
            self.write("ai.html", self.ai_page())
        self.write("404.html", self.shell("404.html", "Not found",
                   f'<div class="{CARD}"><p class="text-gray-500 dark:text-gray-400">No page here. '
                   f'Search above, or start from the <a class="text-primary-600 hover:underline" href="index.html">overview</a>.</p></div>'))
        self.write("data/site.json", json.dumps(self.data(), ensure_ascii=False))
        self.write("data/search.json", json.dumps(self.search_index(), ensure_ascii=False))
        self.write(".nojekyll", "")
        declare(self.out, "site", self.cfg["site"]["mount"], f"{self.title} — documentation site",
                "documentation", self.repo, "tools/surfaces/surfaces site")
        if assets:
            build_assets(self.out)


def declare(out: Path, sid: str, mount: str, title: str, category: str, repo: Repo, producer: str) -> None:
    """The web-surface/v1 declaration Majordomus discovers under target/web/<id>/."""
    (out / "surface.json").write_text(json.dumps({
        "schema": SURFACE_SCHEMA, "id": sid, "mount": mount, "title": title,
        "producer": producer, "availability": "published-only", "category": category,
        "visibility": "public", "built_from": repo.sha,
    }, indent=2) + "\n")


def build_assets(out: Path) -> None:
    modules = HERE / "node_modules"
    if not (modules / ".bin/tailwindcss").exists():
        subprocess.run(["npm", "ci", "--no-audit", "--no-fund"], cwd=HERE, check=True)
    (out / "js").mkdir(exist_ok=True)
    for src in ("flowbite/dist/flowbite.min.js", "mermaid/dist/mermaid.min.js", "echarts/dist/echarts.min.js"):
        shutil.copy(modules / src, out / "js" / posixpath.basename(src))
    shutil.copy(HERE / "assets/app.js", out / "js/app.js")
    css = subprocess.run(
        [str(modules / ".bin/tailwindcss"), "-i", str(HERE / "assets/input.css"),
         "-o", str(out / "css/app.css"), "--minify"],
        cwd=HERE, capture_output=True, text=True,
    )
    if css.returncode != 0:
        raise SystemExit(f"surfaces: the stylesheet did not build:\n{css.stderr}")


# --------------------------------------------------------------------------- wiki


def build_wiki(repo: Repo, cfg: dict, docs: dict[str, Doc], out: Path) -> None:
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    links = Links(repo, docs)
    for d in docs.values():
        body = rewrite_source_links(d.text, lambda href, image, d=d: links.wiki(d, href, image))
        (out / f"{d.wiki}.md").write_text(body, encoding="utf-8")
    if "README.md" not in docs:
        (out / "Home.md").write_text(f"# {repo.name}\n\nSee the sidebar.\n")
    groups: dict[str, list[Doc]] = {}
    for d in docs.values():
        groups.setdefault(d.group, []).append(d)
    side = [f"**[{repo.name}]({repo.web})** · [site]({repo.pages_url})", ""]
    for g in sorted(groups, key=lambda g: (g != "", g.startswith("."), g.lower())):
        side.append(f"**{g or '/'}**")
        side.append("")
        side += [f"- [{d.title}]({d.wiki})" for d in groups[g]]
        side.append("")
    (out / "_Sidebar.md").write_text("\n".join(side))
    (out / "_Footer.md").write_text(
        f"Generated from [{repo.sha[:10]}]({repo.web}/commit/{repo.sha}) by `tools/surfaces/surfaces wiki`. "
        f"Edit the document in the repository, never this wiki: the next sync overwrites it.\n"
    )
    declare(out, "wiki", cfg["wiki"]["mount"], f"{repo.name} — wiki", "documentation", repo,
            "tools/surfaces/surfaces wiki")


# --------------------------------------------------------------------------- commands


# --------------------------------------------------------------------------- tool flags
#
# A document that shows a tool with a flag restates the tool's parser, so it is checked against
# it like a link against its target. Tools are the tracked executable scripts: shell scripts
# parse with `case` arms, Python scripts with argparse. Three findings:
#   usage  a flag the parser accepts is missing from the script's own usage header
#   help   --help is missing, exits non-zero, or prints a fixed line range that stops before the
#          end of that header or runs past it into the code
#   flag   a document shows `tool --flag` and the tool's parser does not accept --flag
# A shell script whose catch-all arm does not refuse (no `exit`) passes unknown flags on, so no
# document can show a flag it would refuse: the third check skips it.

SHEBANG_SHELL = re.compile(r"^#!.*\b(?:ba|z|da)?sh\b")
SHEBANG_PYTHON = re.compile(r"^#!.*\bpython")
CASE_ARM_RE = re.compile(
    r"^[ \t]*\(?((?:-{1,2}[A-Za-z0-9][\w-]*(?:=\*)?)(?:[ \t]*\|[ \t]*-{1,2}[A-Za-z0-9][\w-]*(?:=\*)?)*)[ \t]*\)",
    re.M)
CATCH_ALL_RE = re.compile(r"^[ \t]*\*\)(.*)$", re.M)
TEST_FLAG_RE = re.compile(r"==\s*[\"']?(--[A-Za-z][\w-]*)[\"']?\s*\]\]")
ARGV_FLAG_RE = re.compile(r"[\"'](--[A-Za-z][\w-]*)[\"']\s+in\s+(?:sys\.)?argv\b")
ARGPARSE_RE = re.compile(r"add_argument\(\s*((?:[\"']-{1,2}[\w-]+[\"']\s*,?\s*)+)")
HELP_ARM_RE = re.compile(r"^[ \t]*\(?[^\n]*--help[^\n]*\)(.*)$", re.M)
HELP_SED_RE = re.compile(r"sed -n ['\"]?(\d+),(\d+)p")
LONG_FLAG_RE = re.compile(r"(?<![\w-])--[A-Za-z][\w-]*")
SEGMENT_SPLIT_RE = re.compile(r"\|\||&&|[|;\n]")
INLINE_CODE_RE = re.compile(r"(`+)(.+?)\1", re.S)
ENV_ASSIGN_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*=")


@dataclass
class Tool:
    path: str
    name: str
    flags: set[str]
    strict: bool
    header: list[str]
    help_range: tuple[int, int] | None
    help_line: int
    help_exit: str = ""


def load_tools(repo: Repo, cfg: dict) -> dict[str, list[Tool]]:
    """Every tracked executable script with a parser, by file name."""
    tools: dict[str, list[Tool]] = {}
    for line in git(repo.root, "ls-files", "-s").splitlines():
        meta, _, path = line.partition("\t")
        if not meta.startswith("100755") or matches(path, cfg["flags"]["exclude"]):
            continue
        try:
            text = (repo.root / path).read_text(errors="replace")
        except OSError:
            continue
        first = text.split("\n", 1)[0]
        flags: set[str] = set()
        strict, help_range, help_line, help_exit = True, None, 0, ""
        if SHEBANG_SHELL.match(first):
            for m in CASE_ARM_RE.finditer(text):
                for alt in m.group(1).split("|"):
                    flags.add(alt.strip().removesuffix("=*"))
            catch = [m.group(1) for m in CATCH_ALL_RE.finditer(text)]
            strict = bool(catch) and all("exit" in c for c in catch)
            tested = set(TEST_FLAG_RE.findall(text)) - flags
            if tested:  # `[[ "$1" == --x ]]` refuses nothing it does not test
                flags |= tested
                strict = False
            for m in HELP_ARM_RE.finditer(text):
                help_line = text.count("\n", 0, m.start()) + 1
                s = HELP_SED_RE.search(m.group(1))
                if s:
                    help_range = (int(s.group(1)), int(s.group(2)))
                e = re.search(r"\bexit ([1-9]\d*)", m.group(1))
                if e:
                    help_exit = e.group(1)
        elif SHEBANG_PYTHON.match(first):
            parsed = list(ARGPARSE_RE.finditer(text))
            for m in parsed:
                flags.update(re.findall(r"-{1,2}[\w-]+", m.group(1)))
            strict = bool(parsed) and "parse_known_args" not in text
            if parsed and "add_help=False" not in text:
                flags.add("--help")  # argparse adds it
            argv = set(ARGV_FLAG_RE.findall(text))
            if argv:  # `"--x" in argv` refuses nothing
                flags |= argv
                strict = False
        else:
            continue
        if not flags:
            continue
        header = []
        for h in text.split("\n")[1:]:
            if not h.startswith("#"):
                break
            header.append(h)
        name = posixpath.basename(path)
        tools.setdefault(name, []).append(Tool(path, name, flags, strict, header, help_range, help_line, help_exit))
    return tools


def check_tool_headers(tools: dict[str, list[Tool]]) -> list[Finding]:
    found = []
    for tool in (t for ts in tools.values() for t in ts):
        head = "\n".join(tool.header)
        if re.search(r"\busage\b", head, re.I):
            for flag in sorted(f for f in tool.flags if f.startswith("--") and f != "--help"):
                if not re.search(rf"(?<![\w-]){re.escape(flag)}(?![\w-])", head):
                    found.append(Finding("usage", tool.path, 1, f"accepts {flag}, which its usage header never names"))
        if tool.help_exit:
            found.append(Finding("help", tool.path, tool.help_line,
                                 f"--help exits {tool.help_exit}: asking for help is not an error, exit 0"))
        if tool.strict and tool.header and "--help" not in tool.flags and re.search(r"\busage\b", head, re.I):
            found.append(Finding("help", tool.path, 1, "has a usage header but no -h|--help arm that prints it"))
        if tool.help_range and tool.header:
            last = 1 + max(i + 1 for i, h in enumerate(tool.header) if h.strip("# \t"))
            if tool.help_range[1] < last:
                found.append(Finding("help", tool.path, tool.help_line,
                                     f"--help prints lines {tool.help_range[0]}-{tool.help_range[1]}, "
                                     f"but the usage header runs to line {last}"))
            elif tool.help_range[1] > last + 1:
                found.append(Finding("help", tool.path, tool.help_line,
                                     f"--help prints lines {tool.help_range[0]}-{tool.help_range[1]}, "
                                     f"past the usage header's end at line {last}, so it prints code"))
    return found


def code_spans(doc: Doc) -> list[tuple[int, str]]:
    """(offset, text) of every fenced block and inline code span in a document."""
    spans, fenced = [], fenced_ranges(doc.text)
    for a, b in fenced:
        spans.append((a, doc.text[a:b]))
    rest, pos = [], 0
    for a, b in fenced:
        rest.append((pos, doc.text[pos:a]))
        pos = b
    rest.append((pos, doc.text[pos:]))
    for base, chunk in rest:
        for m in INLINE_CODE_RE.finditer(chunk):
            spans.append((base + m.start(2), m.group(2)))
    return spans


def check_doc_flags(tools: dict[str, list[Tool]], doc: Doc) -> list[Finding]:
    found = []
    for offset, code in code_spans(doc):
        pos = 0
        for segment in SEGMENT_SPLIT_RE.split(code):
            start = code.find(segment, pos)
            pos = start + len(segment)
            words = segment.split()
            while words and (ENV_ASSIGN_RE.match(words[0]) or words[0] in ("$", "!", "time", "exec")):
                words.pop(0)
            if not words:
                continue
            candidates = tools.get(posixpath.basename(words[0].strip("`'\"")), [])
            if not candidates or not all(t.strict for t in candidates):
                continue
            accepted = set().union(*(t.flags for t in candidates))
            for word in words[1:]:
                if word == "--":
                    break
                for flag in LONG_FLAG_RE.findall(word.split("=", 1)[0]):
                    if flag not in accepted:
                        line = doc.text.count("\n", 0, offset + start) + 1
                        found.append(Finding("flag", doc.path, line,
                                             f"shows `{candidates[0].name} {flag}`, which {candidates[0].path} does not accept"))
    return sorted(found, key=lambda f: f.line)


def run_check(repo: Repo, cfg: dict, docs: dict[str, Doc], layer: Layer) -> list[Finding]:
    findings, _ = sync_regions(repo, docs, layer, write=False)
    tools = load_tools(repo, cfg)
    findings += check_tool_headers(tools)
    for path, doc in docs.items():
        if not matches(path, cfg["check"]["exclude"]):
            findings += check_doc_flags(tools, doc)
    md = markdown()
    links = Links(repo, docs)
    parsed = {p: collect_slugs(md, d) for p, d in docs.items()}
    skip = cfg["check"]["exclude"]
    for path, doc in docs.items():
        if matches(path, skip):
            continue
        findings += check_links(md, links, doc, parsed[path])
    for name in ("site", "wiki"):
        out = Path(cfg[name]["out"])
        if out.is_absolute() or out.parts[:2] != tuple(GENERATED_ROOT.split("/")):
            findings.append(Finding("config", CONFIG_FILE, 0,
                                    f"{name}.out must stay under {GENERATED_ROOT}/, where surfaces are discovered"))
    return findings


def write_region_report(repo: Repo, cfg: dict, report: list[dict]) -> None:
    out = repo.root / cfg["regions"]["report"]
    out.mkdir(parents=True, exist_ok=True)
    (out / "regions.json").write_text(json.dumps({"built_from": repo.sha, "regions": report}, indent=2) + "\n")
    rows = "".join(
        f"<tr><td>{esc(r['path'])}:{r['line']}</td><td>{esc(r['name'])}</td><td>{esc(r['state'])}</td></tr>"
        for r in report
    )
    (out / "index.html").write_text(
        "<!doctype html><meta charset=utf-8><title>Markdown regions</title>"
        f"<h1>Markdown regions at {esc(repo.sha[:10])}</h1>"
        f"<table><tr><th>Where</th><th>Generator</th><th>State</th></tr>{rows}</table>\n"
    )
    declare(out, "markdown", "/markdown", "Generated Markdown regions and their state", "report", repo,
            "tools/surfaces/surfaces sync")


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(prog="surfaces", description=__doc__.split("\n\n")[0])
    ap.add_argument("--repo", default=None, help="repository root (default: the git toplevel of the cwd)")
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("check", help="fail on a stale region, a broken link or anchor (exit 10)")
    s = sub.add_parser("sync", help="regenerate the Markdown regions in place")
    s.add_argument("--check", action="store_true", help="write nothing; exit 11 when a region would change")
    s = sub.add_parser("site", help="build the static site")
    s.add_argument("--out", default=None)
    s.add_argument("--no-assets", action="store_true", help="skip the CSS/JS build (HTML only)")
    s = sub.add_parser("wiki", help="build the wiki pages")
    s.add_argument("--out", default=None)
    sub.add_parser("list", help="the documents each surface publishes, one per line")
    args = ap.parse_args(argv)

    start = Path(args.repo or ".").resolve()
    root = Path(git(start, "rev-parse", "--show-toplevel").strip())
    os.chdir(root)
    cfg = load_config(root)
    repo = load_repo(root)
    docs = load_docs(repo, cfg)
    layer = load_layer(repo)

    if args.cmd == "list":
        for d in docs.values():
            print(f"{d.path}\t{d.page}\t{d.wiki}\t{d.title}")
        return EXIT_OK
    if args.cmd == "check":
        findings = run_check(repo, cfg, docs, layer)
        for f in findings:
            print(f)
        print(f"surfaces check: {len(docs)} document(s), {len(findings)} finding(s)")
        return EXIT_FINDINGS if findings else EXIT_OK
    if args.cmd == "sync":
        findings, report = sync_regions(repo, docs, layer, write=not args.check)
        for f in findings:
            print(f)
        if not args.check:
            write_region_report(repo, cfg, report)
        stale = sum(r["state"] == "stale" for r in report)
        verb = "would change" if args.check else "regenerated"
        print(f"surfaces sync: {len(report)} region(s), {stale} {verb}")
        if any(f.message.startswith("unknown generator") for f in findings):
            return EXIT_FINDINGS
        return EXIT_DRIFT if args.check and stale else EXIT_OK
    if args.cmd == "site":
        out = Path(args.out or cfg["site"]["out"]).resolve()
        Site(repo, cfg, docs, layer, out).build(assets=not args.no_assets)
        print(f"surfaces site: {len(docs)} page(s) -> {out}")
        return EXIT_OK
    if args.cmd == "wiki":
        out = Path(args.out or cfg["wiki"]["out"]).resolve()
        build_wiki(repo, cfg, docs, out)
        print(f"surfaces wiki: {len(docs)} page(s) -> {out}")
        return EXIT_OK
    return EXIT_USAGE


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
