"""Read and rewrite grfcodec's NFO (info version 32).

Only real-sprite lines are interpreted; every other line (pseudo sprites, comments, existing
alternative sprites) is carried through verbatim so the rebuilt GRF keeps the source's actions.
"""

from __future__ import annotations

import re
from dataclasses import dataclass, field

# "  123 dec/file00.png  8bpp   18    8   13   18    0    0 normal [flags...]"
_REAL = re.compile(
    r"^\s*(?P<num>\d+)\s+(?P<file>\S+\.png)\s+(?P<depth>8bpp)\s+"
    r"(?P<x>\d+)\s+(?P<y>\d+)\s+(?P<w>\d+)\s+(?P<h>\d+)\s+"
    r"(?P<xrel>-?\d+)\s+(?P<yrel>-?\d+)\s+(?P<zoom>normal)(?P<flags>(\s+\w+)*)\s*$"
)


@dataclass(frozen=True)
class RealSprite:
    """One 8bpp normal-zoom real sprite as grfcodec decoded it."""

    line: int
    num: int
    file: str
    x: int
    y: int
    w: int
    h: int
    xrel: int
    yrel: int
    flags: tuple[str, ...] = field(default_factory=tuple)


def parse(text: str) -> tuple[list[str], dict[int, RealSprite]]:
    """Return the NFO lines and the 8bpp normal sprites keyed by line index."""
    lines = text.splitlines()
    sprites: dict[int, RealSprite] = {}
    for i, line in enumerate(lines):
        m = _REAL.match(line)
        if m is None:
            continue
        sprites[i] = RealSprite(
            line=i,
            num=int(m["num"]),
            file=m["file"],
            x=int(m["x"]),
            y=int(m["y"]),
            w=int(m["w"]),
            h=int(m["h"]),
            xrel=int(m["xrel"]),
            yrel=int(m["yrel"]),
            flags=tuple(m["flags"].split()),
        )
    return lines, sprites


def alternative(file: str, x: int, y: int, w: int, h: int, xrel: int, yrel: int,
                zoom: str, flags: tuple[str, ...], mask_file: str | None) -> list[str]:
    """NFO lines adding a 32bpp alternative (and its palette mask) to the preceding sprite."""
    keep = [f for f in flags if f == "nocrop"]
    out = [f"    | {file} 32bpp {x:4d} {y:4d} {w:4d} {h:4d} {xrel:4d} {yrel:4d} {zoom}"
           + "".join(f" {f}" for f in keep)]
    if mask_file is not None:
        out.append(f"    | {mask_file} mask {x:4d} {y:4d}")
    return out
