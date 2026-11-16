"""Build NoirGFX: decode OpenGFX, stylize every sprite, re-encode, write the .obg, pack a tar."""

from __future__ import annotations

import hashlib
import io
import json
import os
import shutil
import subprocess
import tarfile
from concurrent.futures import ProcessPoolExecutor, ThreadPoolExecutor
from dataclasses import dataclass
from pathlib import Path

import numpy as np
from PIL import Image

from . import nfo, render
from .stylize import Result, Style, Stylizer, classify

ZOOMS = (("zi4", 4), ("zi2", 2), ("normal", 1))
SHEET_W = {4: 4096, 2: 2048, 1: 1024}
SHEET_H = 4096
# Sprites left exactly as OpenGFX draws them: the three sprite fonts (recoloured by the game for
# every text colour) in the base GRF.
UNTOUCHED = {"ogfx1_base": range(2, 682)}
NAMES = {
    "ogfx1_base": ("noir1_base", "base"),
    "ogfxc_arctic": ("noirc_arctic", "arctic"),
    "ogfxh_tropical": ("noirh_tropical", "tropical"),
    "ogfxt_toyland": ("noirt_toyland", "toyland"),
    "ogfxi_logos": ("noiri_logos", "logos"),
    "ogfxe_extra": ("noire_extra", "extra"),
}


@dataclass
class Paths:
    root: Path  # tools/research/noirgfx
    work: Path
    dist: Path
    grfcodec: Path
    source: Path  # directory holding the OpenGFX .grf/.obg files


# --- sheet packing ---------------------------------------------------------------------------

class Shelf:
    """Deterministic shelf packer writing fixed-size RGBA sheets and matching mask sheets."""

    def __init__(self, prefix: str, width: int) -> None:
        self.prefix, self.width = prefix, width
        self.sheets: list[tuple[np.ndarray, np.ndarray]] = []
        self.used: list[int] = []  # rows occupied per sheet, transparent sprites included
        self.x = self.y = self.row_h = 0
        self._new()

    def _new(self) -> None:
        self.sheets.append((np.zeros((SHEET_H, self.width, 4), np.uint8),
                            np.zeros((SHEET_H, self.width), np.uint8)))
        self.used.append(0)
        self.x = self.y = self.row_h = 0

    def place(self, rgba: np.ndarray, mask: np.ndarray | None) -> tuple[int, int, int]:
        h, w = rgba.shape[:2]
        if w > self.width or h > SHEET_H:
            raise ValueError(f"sprite {w}x{h} exceeds sheet {self.width}x{SHEET_H}")
        if self.x + w > self.width:
            self.x, self.y, self.row_h = 0, self.y + self.row_h + 1, 0
        if self.y + h > SHEET_H:
            self._new()
        sheet, msheet = self.sheets[-1]
        sheet[self.y:self.y + h, self.x:self.x + w] = rgba
        if mask is not None:
            msheet[self.y:self.y + h, self.x:self.x + w] = mask
        pos = (len(self.sheets) - 1, self.x, self.y)
        self.used[-1] = max(self.used[-1], self.y + h)
        self.x += w + 1
        self.row_h = max(self.row_h, h)
        return pos

    def file(self, n: int) -> str:
        return f"noir/{self.prefix}{n:02d}.png"

    def mask_file(self, n: int) -> str:
        return f"noir/{self.prefix}{n:02d}m.png"

    def save(self, base: Path, palette: list[int]) -> None:
        (base / "noir").mkdir(parents=True, exist_ok=True)
        for n, (sheet, msheet) in enumerate(self.sheets):
            used = max(1, self.used[n])
            Image.fromarray(sheet[:used], "RGBA").save(base / self.file(n), optimize=False)
            m = Image.fromarray(msheet[:used], "P")
            m.putpalette(palette)
            m.save(base / self.mask_file(n), optimize=False)


# --- per-sprite work (process pool) ----------------------------------------------------------

_W: dict = {}


def _init(style_path: str, palette: bytes) -> None:
    pal = np.frombuffer(palette, np.uint8).reshape(256, 3)
    _W["st"] = Stylizer(Style.load(Path(style_path)), pal)


def _work(job: tuple) -> tuple[int, list[Result]]:
    line, idx, xrel, yrel, cat, renders = job
    st: Stylizer = _W["st"]
    out = []
    for _, z in ZOOMS:
        r = renders.get(z) if renders else None
        out.append(_from_render(st, r, z) if r else st.run(idx, xrel, yrel, z, cat))
    return line, out


def _from_render(st: Stylizer, r, zoom: int) -> Result:
    """A Kenney render, cropped to its silhouette, anchored where the model touches the origin."""
    rgba = np.asarray(Image.open(r.rgb).convert("RGBA"))
    nrm = np.asarray(Image.open(r.nrm).convert("RGBA"))
    ys, xs = np.nonzero(rgba[..., 3] > 127)
    if len(ys) == 0:
        return Result(np.zeros((1, 1, 4), np.uint8), None, (0, 0))
    t, l, b, rr = ys.min(), xs.min(), ys.max() + 1, xs.max() + 1
    ox, oy = r.origin
    return st.run_rgb(rgba[t:b, l:rr], nrm[t:b, l:rr], int(l - ox), int(t - oy), zoom)


# --- stages ----------------------------------------------------------------------------------

def run(cmd: list[str], cwd: Path) -> None:
    r = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)
    if r.returncode != 0:
        raise SystemExit(f"{' '.join(cmd)} failed in {cwd}:\n{r.stdout}\n{r.stderr}")


def decode(p: Paths, stem: str) -> Path:
    d = p.work / stem
    if d.exists():
        shutil.rmtree(d)
    d.mkdir(parents=True)
    shutil.copy2(p.source / f"{stem}.grf", d / f"{stem}.grf")
    run([str(p.grfcodec), "-d", "-p", "1", "-w", "2048", f"{stem}.grf", "dec"], d)
    return d


def stylize_grf(p: Paths, style_path: Path, stem: str, jobs: int, limit: set[int] | None,
                renders: dict | None = None) -> dict:
    d = decode(p, stem)
    new, _ = NAMES[stem]
    lines, sprites = nfo.parse((d / "dec" / f"{stem}.nfo").read_text(encoding="latin-1"))
    sheets: dict[str, Image.Image] = {}
    palette: list[int] | None = None
    work = []
    skip = UNTOUCHED.get(stem, range(0))
    for i, s in sprites.items():
        if s.num in skip or (limit is not None and s.num not in limit):
            continue
        if s.file not in sheets:
            sheets[s.file] = Image.open(d / s.file)
            palette = palette or sheets[s.file].getpalette()[:768]
        img = sheets[s.file]
        idx = np.asarray(img.crop((s.x, s.y, s.x + s.w, s.y + s.h)), dtype=np.uint8)
        rend = (renders or {}).get(s.num)
        work.append((i, idx, s.xrel, s.yrel, classify(s.w, s.h, s.xrel, s.yrel), rend))
    if palette is None:
        palette = list(Image.open(next((d / "dec").glob("*.png"))).getpalette()[:768])

    shelves = {z: Shelf(f"{new}_{name}_", SHEET_W[z]) for name, z in ZOOMS}
    extra: dict[int, list[str]] = {}
    counts = {"ground": 0, "object": 0, "gui": 0, "rendered": 0}
    with ProcessPoolExecutor(jobs, initializer=_init,
                             initargs=(str(style_path), bytes(palette))) as ex:
        results = dict(ex.map(_work, work, chunksize=16))
    for i, _, xrel, yrel, cat, rend in work:  # deterministic order: NFO order
        counts["rendered" if rend else cat] += 1
        add: list[str] = []
        for (zname, z), res in zip(ZOOMS, results[i]):
            n, x, y = shelves[z].place(res.rgba, res.mask)
            h, w = res.rgba.shape[:2]
            sh = shelves[z]
            ox, oy = res.offset if res.offset is not None else (xrel * z, yrel * z)
            add += nfo.alternative(sh.file(n), x, y, w, h, ox, oy, zname,
                                   sprites[i].flags, sh.mask_file(n) if res.mask is not None else None)
        extra[i] = add
    for sh in shelves.values():
        sh.save(d, palette)

    out = []
    for i, line in enumerate(lines):
        out.append(line)
        out.extend(extra.get(i, []))
    (d / "dec" / f"{new}.nfo").write_text("\n".join(out) + "\n", encoding="latin-1")
    return {"grf": f"{new}.grf", "sprites": len(sprites), "restyled": len(work), **counts}


def md5(path: Path) -> str:
    return hashlib.md5(path.read_bytes()).hexdigest()


GRF_V2_SIG = b"\x00\x00GRF\x82\x0d\x0a\x1a\x0a"


def grf_md5(path: Path) -> str:
    """The checksum a base set .obg lists: OpenTTD hashes only the data section of a container
    version 2 GRF (src/newgrf_config.cpp:GRFGetSizeOfDataSection), the whole file otherwise."""
    data = path.read_bytes()
    if data[:10] == GRF_V2_SIG:
        data = data[:14 + int.from_bytes(data[10:14], "little")]
    return hashlib.md5(data).hexdigest()


def write_obg(p: Paths, style: Style, out: Path) -> None:
    ident = style.raw["identity"]
    lines = ["[metadata]",
             f"name        = {ident['name']}",
             f"shortname   = {ident['shortname']}",
             f"version     = {ident['version']}",
             "palette     = DOS",
             "blitter     = 32bpp",
             # Listed and selectable, never chosen on its own: OpenTTD's automatic pick
             # (DetermineBestSet) prefers any non-fallback set, so OpenGFX stays the default.
             "fallback    = 1",
             f"description = {ident['description']}",
             "", "[files]"]
    lines += [f"{key:<8} = {new}.grf" for new, key in NAMES.values()]
    lines += ["", "[md5s]"]
    lines += [f"{new}.grf = {grf_md5(out / f'{new}.grf')}" for new, _ in NAMES.values()]
    lines += ["", "[origin]", "default = Built by tools/research/noirgfx in korczis/OpenTTD.", ""]
    (out / "noirgfx.obg").write_text("\n".join(lines))


def readme(style: Style) -> str:
    ident = style.raw["identity"]
    return (f"{ident['name']} {ident['version']}\n\n{ident['description']}\n\n"
            "Derived from OpenGFX 8.0 (c) the OpenGFX authors, GNU GPL version 2; see license.txt.\n"
            "Every sprite was restyled by tools/research/noirgfx (korczis/OpenTTD), which is its "
            "source code together with the OpenGFX sources.\nStyle: SIG NIHL.\n")


def deterministic_tar(src: Path, dest: Path, top: str) -> None:
    buf = io.BytesIO()
    with tarfile.open(fileobj=buf, mode="w", format=tarfile.USTAR_FORMAT) as tar:
        for f in sorted(src.iterdir()):
            info = tarfile.TarInfo(f"{top}/{f.name}")
            data = f.read_bytes()
            info.size, info.mtime, info.mode = len(data), 0, 0o644
            info.uid = info.gid = 0
            info.uname = info.gname = ""
            tar.addfile(info, io.BytesIO(data))
    dest.write_bytes(buf.getvalue())


def build(p: Paths, jobs: int, only: list[str] | None = None, limit: set[int] | None = None) -> Path:
    style_path = p.root / "style.toml"
    style = Style.load(style_path)
    p.work.mkdir(parents=True, exist_ok=True)
    stats = {}
    renders: dict = {}
    if "ogfx1_base" in NAMES and (not only or "ogfx1_base" in only):
        base = decode(p, "ogfx1_base")
        renders = render.render_all(p.root, base / "dec" / "ogfx1_base.nfo", p.work.parent / "renders",
                                    os.environ.get("NOIRGFX_BLENDER", "blender"), max(1, jobs // 2))
    for stem in NAMES:
        if only and stem not in only:
            continue
        stats[stem] = stylize_grf(p, style_path, stem, jobs, limit,
                                  renders if stem == "ogfx1_base" else None)
        print(f"  stylized {stem}: {stats[stem]}", flush=True)
    # grfcodec encodes on one thread; the six GRFs are independent, so encode them side by side.
    def encode(stem: str) -> str:
        new = NAMES[stem][0]
        run([str(p.grfcodec), "-e", "-g", "2", "-s", f"{new}.grf", "dec"], p.work / stem)
        return stem
    with ThreadPoolExecutor(len(stats) or 1) as ex:
        for stem in ex.map(encode, stats):
            print(f"  encoded {stem}", flush=True)
    top = f"noirgfx-{style.raw['identity']['version']}"
    out = p.dist / top
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    for stem, (new, _) in NAMES.items():
        built = p.work / stem / f"{new}.grf"
        if not built.exists():
            raise SystemExit(f"missing {built}; build every GRF before packing")
        shutil.copy2(built, out / f"{new}.grf")
    write_obg(p, style, out)
    shutil.copy2(p.source / "license.txt", out / "license.txt")
    (out / "readme.txt").write_text(readme(style))
    tar = p.dist / f"{top}.tar"
    deterministic_tar(out, tar, top)
    manifest = {"style_sha256": style.digest, "tar": tar.name, "tar_md5": md5(tar),
                "files": {f.name: md5(f) for f in sorted(out.iterdir())}, "stats": stats}
    (p.dist / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    return tar


def default_jobs() -> int:
    return max(1, (os.cpu_count() or 2) - 2)
