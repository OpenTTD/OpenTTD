"""Invariants of the last NoirGFX build. Exit status 1 on any failure; every line says what it proved.

1. The .obg lists every GRF with the checksum OpenTTD computes.
2. Each rebuilt GRF keeps every OpenGFX sprite (number, size, offsets) and adds exactly one 32bpp
   alternative per zoom level to every restyled sprite.
3. Seamless: a ground sprite's stylized pixels do not change when it moves by a tile-lattice
   vector, i.e. its texture is a function of screen position modulo the lattice.
4. --rebuild: a second build from scratch gives a byte-identical tar.
"""

from __future__ import annotations

import json
import shutil
import sys
import tempfile
from pathlib import Path

import numpy as np

from . import build, nfo
from .__main__ import paths
from .preview import FLAT_GRASS, RAIL_Y, ROAD_X, Source
from .stylize import LATTICE_U, LATTICE_V, Style, Stylizer, classify

FAILED: list[str] = []


def report(ok: bool, what: str) -> None:
    print(f"{'PASS' if ok else 'FAIL'}  {what}")
    if not ok:
        FAILED.append(what)


def check_obg(dist: Path) -> None:
    top = next(d for d in sorted(dist.iterdir()) if d.is_dir() and d.name.startswith("noirgfx-"))
    obg = (top / "noirgfx.obg").read_text().splitlines()
    md5s = dict(line.split(" = ") for line in obg[obg.index("[md5s]") + 1:] if " = " in line)
    for new, _ in build.NAMES.values():
        grf = top / f"{new}.grf"
        report(grf.exists() and md5s.get(f"{new}.grf") == build.grf_md5(grf),
               f"obg checksum of {new}.grf matches OpenTTD's data-section md5")
    report("fallback    = 1" in obg,
           "obg is marked fallback: selectable, never OpenTTD's automatic default")


def check_grfs(p: build.Paths, dist: Path) -> None:
    manifest = json.loads((dist / "manifest.json").read_text())
    top = dist / manifest["tar"].removesuffix(".tar")
    for stem, (new, _) in build.NAMES.items():
        with tempfile.TemporaryDirectory() as tmp:
            t = Path(tmp)
            shutil.copy2(top / f"{new}.grf", t / f"{new}.grf")
            build.run([str(p.grfcodec), "-d", "-p", "1", "-w", "8192", f"{new}.grf", "dec"], t)
            text = (t / "dec" / f"{new}.nfo").read_text(encoding="latin-1")
        _, got = nfo.parse(text)
        _, src = nfo.parse((p.work / stem / "dec" / f"{stem}.nfo").read_text(encoding="latin-1"))
        key = lambda s: (s.num, s.w, s.h, s.xrel, s.yrel)  # noqa: E731
        same = sorted(map(key, got.values())) == sorted(map(key, src.values()))
        report(same, f"{new}: all {len(src)} OpenGFX sprites kept with number, size and offsets")
        restyled = manifest["stats"].get(stem, {}).get("restyled")
        for zoom in ("zi4", "zi2", "normal"):
            # "    | file 32bpp x y w h xrel yrel zoom [flags]"
            n = sum(1 for tok in map(str.split, text.splitlines())
                    if len(tok) >= 10 and tok[0] == "|" and tok[2] == "32bpp" and tok[9] == zoom)
            report(restyled is not None and n == restyled,
                   f"{new}: {n} 32bpp {zoom} alternatives for {restyled} restyled sprites")


def check_seams(p: build.Paths) -> None:
    style = Style.load(p.root / "style.toml")
    src = Source(p.work / "ogfx1_base" / "dec", "ogfx1_base")
    st = Stylizer(style, src.palette)
    for num in (FLAT_GRASS, RAIL_Y, ROAD_X):
        idx, s = src.idx(num)
        cat = classify(s.w, s.h, s.xrel, s.yrel)
        for zoom in (1, 2, 4):
            base = st.run(idx, s.xrel, s.yrel, zoom, cat).rgba
            ok = all(np.array_equal(base, st.run(idx, s.xrel + du, s.yrel + dv, zoom, cat).rgba)
                     for du, dv in ((LATTICE_U, 0), (0, LATTICE_V), (32, 16), (-32, 16)))
            report(ok and cat == "ground",
                   f"sprite {num} ({cat}) at {zoom}x is invariant under tile-lattice shifts: seamless")


def check_rebuild(p: build.Paths) -> None:
    first = json.loads((p.dist / "manifest.json").read_text())["tar_md5"]
    with tempfile.TemporaryDirectory() as tmp:
        q = build.Paths(p.root, Path(tmp) / "work", Path(tmp) / "dist", p.grfcodec, p.source)
        tar = build.build(q, build.default_jobs())
        report(build.md5(tar) == first, f"rebuild from scratch is byte-identical (tar md5 {first})")


def main() -> None:
    p = paths()
    if not (p.dist / "manifest.json").exists():
        sys.exit("no build to check; run: noirgfx build")
    check_obg(p.dist)
    check_grfs(p, p.dist)
    check_seams(p)
    if "--rebuild" in sys.argv:
        check_rebuild(p)
    print(f"{len(FAILED)} failed" if FAILED else "all checks passed")
    sys.exit(1 if FAILED else 0)


if __name__ == "__main__":
    main()
