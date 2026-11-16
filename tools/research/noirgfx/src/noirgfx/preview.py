"""Offline previews: an isometric scene and a contact sheet, rendered straight from the stylizer.

The scene places tiles exactly as the game does (origin of tile (x, y) at screen
(32 (y - x), 16 (x + y)) * zoom, sprite at origin + offset * zoom), so a seam visible here would be
a seam in the game, and none visible here is the seamlessness check made visible.
"""

from __future__ import annotations

from pathlib import Path

import numpy as np
from PIL import Image

from . import nfo
from .stylize import Style, Stylizer, classify

FLAT_GRASS, FLAT_WATER, RAIL_X, RAIL_Y, ROAD_X, ROAD_Y = 3981, 4061, 1012, 1011, 1333, 1332


class Source:
    def __init__(self, dec: Path, stem: str) -> None:
        self.dec = dec
        _, sprites = nfo.parse((dec / f"{stem}.nfo").read_text(encoding="latin-1"))
        self.by_num = {s.num: s for s in sprites.values()}
        self.sheets: dict[str, Image.Image] = {}
        first = next(iter(self.by_num.values()))
        self.palette = np.array(self.sheet(first.file).getpalette()[:768], np.uint8).reshape(256, 3)

    def sheet(self, f: str) -> Image.Image:
        if f not in self.sheets:
            self.sheets[f] = Image.open(self.dec.parent / f)
        return self.sheets[f]

    def idx(self, num: int) -> tuple[np.ndarray, nfo.RealSprite]:
        s = self.by_num[num]
        img = self.sheet(s.file).crop((s.x, s.y, s.x + s.w, s.y + s.h))
        return np.asarray(img, np.uint8), s


def _blit(canvas: np.ndarray, rgba: np.ndarray, x: int, y: int) -> None:
    h, w = rgba.shape[:2]
    H, W = canvas.shape[:2]
    x0, y0, x1, y1 = max(x, 0), max(y, 0), min(x + w, W), min(y + h, H)
    if x0 >= x1 or y0 >= y1:
        return
    src = rgba[y0 - y:y1 - y, x0 - x:x1 - x]
    a = src[..., 3:4] > 0
    canvas[y0:y1, x0:x1] = np.where(a, src[..., :3], canvas[y0:y1, x0:x1])


def scene(src: Source, style: Style, zoom: int, out: Path, original: bool = False) -> None:
    st = Stylizer(style, src.palette)
    n = 8
    W, H = 64 * n * zoom, 32 * n * zoom + 64 * zoom
    canvas = np.zeros((H, W, 3), np.uint8)
    canvas[:] = style.colour("ink") // 2
    ox, oy = W // 2, 32 * zoom
    cache: dict[int, np.ndarray] = {}

    def sprite(num: int) -> tuple[np.ndarray, nfo.RealSprite]:
        idx, s = src.idx(num)
        if num not in cache:
            if original:
                rgb = src.palette[idx]
                a = np.where(idx > 0, 255, 0).astype(np.uint8)[..., None]
                cache[num] = np.kron(np.concatenate([rgb, a], 2), np.ones((zoom, zoom, 1), np.uint8))
            else:
                cache[num] = st.run(idx, s.xrel, s.yrel, zoom, classify(s.w, s.h, s.xrel, s.yrel)).rgba
        return cache[num], s

    def layout(x: int, y: int) -> int:
        if x == 3:
            return RAIL_Y
        if y == 5:
            return ROAD_X
        if x >= 6 and y <= 2:
            return FLAT_WATER
        return FLAT_GRASS

    for s_ in range(2 * n - 1):
        for x in range(n):
            y = s_ - x
            if not 0 <= y < n:
                continue
            rgba, s = sprite(layout(x, y))
            px = ox + 32 * (y - x) * zoom + s.xrel * zoom
            py = oy + 16 * (x + y) * zoom + s.yrel * zoom
            _blit(canvas, rgba, px, py)
    Image.fromarray(canvas, "RGB").save(out)


def contact(src: Source, style: Style, nums: list[int], zoom: int, out: Path) -> None:
    st = Stylizer(style, src.palette)
    tiles = []
    for num in nums:
        if num not in src.by_num:
            continue
        idx, s = src.idx(num)
        tiles.append(st.run(idx, s.xrel, s.yrel, zoom, classify(s.w, s.h, s.xrel, s.yrel)).rgba)
    cols, pad = 12, 8 * zoom
    cw = max(t.shape[1] for t in tiles) + pad
    ch = max(t.shape[0] for t in tiles) + pad
    rows = (len(tiles) + cols - 1) // cols
    canvas = np.zeros((rows * ch, cols * cw, 3), np.uint8)
    canvas[:] = style.colour("ink") // 2
    for i, t in enumerate(tiles):
        r, c = divmod(i, cols)
        _blit(canvas, t, c * cw + (cw - t.shape[1]) // 2, r * ch + (ch - t.shape[0]) // 2)
    Image.fromarray(canvas, "RGB").save(out)
