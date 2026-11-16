"""noirgfx CLI. Paths come from the environment the wrapper script sets (see ../../noirgfx)."""

from __future__ import annotations

import argparse
import os
from pathlib import Path

from . import build, preview
from .stylize import Style


def paths() -> build.Paths:
    env = os.environ
    return build.Paths(
        root=Path(env["NOIRGFX_ROOT"]),
        work=Path(env["NOIRGFX_WORK"]),
        dist=Path(env["NOIRGFX_DIST"]),
        grfcodec=Path(env["NOIRGFX_GRFCODEC"]),
        source=Path(env["NOIRGFX_SOURCE"]),
    )


def main() -> None:
    ap = argparse.ArgumentParser(prog="noirgfx")
    sub = ap.add_subparsers(dest="cmd", required=True)
    b = sub.add_parser("build", help="build the base set tar")
    b.add_argument("--jobs", type=int, default=build.default_jobs())
    b.add_argument("--only", nargs="*", help="GRF stems to (re)build, e.g. ogfx1_base")
    pv = sub.add_parser("preview", help="render an iso scene and a contact sheet")
    pv.add_argument("--zoom", type=int, default=4, choices=(1, 2, 4))
    pv.add_argument("--out", type=Path, required=True)
    pv.add_argument("--contact", default="1400-1520", help="sprite number range for the sheet")
    args = ap.parse_args()
    p = paths()

    if args.cmd == "build":
        tar = build.build(p, args.jobs, args.only)
        print(tar)
    elif args.cmd == "preview":
        stem = "ogfx1_base"
        dec = p.work / stem / "dec"
        if not (dec / f"{stem}.nfo").exists():
            build.decode(p, stem)
        style = Style.load(p.root / "style.toml")
        src = preview.Source(dec, stem)
        args.out.mkdir(parents=True, exist_ok=True)
        preview.scene(src, style, args.zoom, args.out / f"scene_zi{args.zoom}.png")
        preview.scene(src, style, args.zoom, args.out / f"scene_original_zi{args.zoom}.png", True)
        lo, hi = (int(x) for x in args.contact.split("-"))
        preview.contact(src, style, list(range(lo, hi)), args.zoom, args.out / f"contact_{lo}_{hi}.png")
        print(args.out)


if __name__ == "__main__":
    main()
