"""Replacement renders: mapping.toml -> Blender jobs -> cached PNGs per sprite and zoom.

A render is cached under $NOIRGFX_CACHE/renders by the hash of everything that decides its pixels
(the model file, the job parameters, blender_render.py itself), so a rebuild renders only what
changed and never calls Blender for an unchanged sprite.
"""

from __future__ import annotations

import hashlib
import json
import subprocess
import tempfile
import tomllib
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass
from pathlib import Path

from . import nfo

ZOOMS = (4, 2, 1)
HERE = Path(__file__).parent


@dataclass(frozen=True)
class Render:
    rgb: Path
    nrm: Path
    origin: tuple[int, int]  # pixel of the world origin (the sprite's anchor) in the render


def _sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def plan(root: Path, nfo_path: Path) -> dict[int, dict]:
    """Sprite number -> job parameters (without zoom) for every sprite mapping.toml replaces."""
    mapping = tomllib.loads((root / "mapping.toml").read_text())
    _, sprites = nfo.parse(nfo_path.read_text(encoding="latin-1"))
    by_num = {s.num: s for s in sprites.values()}
    assets = root / "assets" / "kenney-curated"
    jobs: dict[int, dict] = {}
    kind = mapping["kinds"]["tree"]
    for entry in mapping.get("tree", []):
        glb = assets / f"{entry['model']}.glb"
        if not glb.exists():
            raise SystemExit(f"mapping.toml: model {entry['model']} not found at {glb}; the Kenney "
                             "models are not in git, run: noirgfx curate <Kenney Game Assets 2 zip> "
                             "<Kenney Game Assets 3 zip>")
        for index in entry["index"]:
            for stage, extra in enumerate(kind["stages"]):
                num = kind["first"] + kind["stride"] * index + stage
                s = by_num.get(num)
                if s is None:
                    continue
                jobs[num] = {"glb": str(glb), "glb_sha": _sha(glb), "height_1x": s.h,
                             "snow": bool(entry.get("snow", False)),
                             "dark": float(extra.get("dark", 0.0))}
    return jobs


def _key(job: dict, zoom: int, script_sha: str) -> str:
    body = {k: v for k, v in job.items() if k != "glb"}
    return hashlib.sha256(json.dumps([body, zoom, script_sha], sort_keys=True).encode()).hexdigest()


def render_all(root: Path, nfo_path: Path, cache: Path, blender: str, procs: int) -> dict[int, dict[int, Render]]:
    jobs = plan(root, nfo_path)
    script = HERE / "blender_render.py"
    script_sha = _sha(script)
    cache.mkdir(parents=True, exist_ok=True)
    todo: list[dict] = []
    out: dict[int, dict[int, Render]] = {}
    for num, job in sorted(jobs.items()):
        out[num] = {}
        for zoom in ZOOMS:
            key = _key(job, zoom, script_sha)
            meta = cache / f"{key}.json"
            if not meta.exists():
                todo.append({"glb": job["glb"], "out": str(cache / key), "zoom": zoom,
                             "target_h": job["height_1x"] * zoom, "snow": job["snow"],
                             "dark": job["dark"], "key": key})
            out[num][zoom] = key  # type: ignore[assignment]
    if todo:
        print(f"  rendering {len(todo)} sprites in {procs} Blender processes", flush=True)
        chunks = [todo[i::procs] for i in range(procs) if todo[i::procs]]

        def run(chunk: list[dict]) -> None:
            with tempfile.TemporaryDirectory() as tmp:
                jf, rf = Path(tmp) / "jobs.json", Path(tmp) / "res.json"
                jf.write_text(json.dumps(chunk))
                r = subprocess.run([blender, "-b", "--factory-startup", "-P", str(script), "--",
                                    str(jf), str(rf)], capture_output=True, text=True)
                if r.returncode != 0 or not rf.exists():
                    raise SystemExit(f"blender failed:\n{r.stdout[-3000:]}\n{r.stderr[-3000:]}")
                for job, res in zip(chunk, json.loads(rf.read_text())):
                    (cache / f"{job['key']}.json").write_text(json.dumps(res))

        with ThreadPoolExecutor(len(chunks)) as ex:
            list(ex.map(run, chunks))
    for num, per in out.items():
        for zoom, key in per.items():
            meta = json.loads((cache / f"{key}.json").read_text())
            per[zoom] = Render(cache / f"{key}_rgb.png", cache / f"{key}_nrm.png",
                               tuple(meta["origin"]))
    return out
