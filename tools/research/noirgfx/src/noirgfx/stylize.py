"""The deterministic SIG NIHL stylizer: one 8bpp OpenGFX sprite in, a 32bpp sprite + mask out.

Everything that textures a sprite is a function of *screen position modulo the tile lattice*,
never of the sprite's own pixel grid: tile origins sit on (32a, 8b) * zoom screen pixels (a tile is
32 px across per axis step and a height level is 8 px), so a pattern periodic on that lattice is
identical wherever two neighbouring sprites meet, at any height. That is what makes ground seamless.
"""

from __future__ import annotations

import hashlib
import tomllib
from dataclasses import dataclass
from pathlib import Path

import numpy as np
from scipy import ndimage

TRANSPARENT = 0
CC_FIRST, CC_LAST = 198, 205  # company-colour ramp (recoloured by the game)
ANIM_FIRST, ANIM_LAST = 227, 254  # palette-animated indices (water, lights)
LATTICE_U, LATTICE_V = 32, 8  # tile lattice in screen pixels at 1x


def _hex(c: str) -> np.ndarray:
    c = c.lstrip("#")
    return np.array([int(c[i:i + 2], 16) for i in (0, 2, 4)], dtype=np.uint8)


@dataclass(frozen=True)
class Style:
    raw: dict
    digest: str

    @classmethod
    def load(cls, path: Path) -> "Style":
        data = path.read_bytes()
        return cls(tomllib.loads(data.decode()), hashlib.sha256(data).hexdigest())

    def colour(self, name: str) -> np.ndarray:
        return _hex(self.raw["palette"][name])

    def seed(self, salt: str) -> int:
        """Noise seeds come from screen.seed alone, so tuning any other value never reshuffles
        the pattern (and every GRF of one build shares it)."""
        key = self.raw["screen"]["seed"]
        return int(hashlib.sha256(f"{key}:{salt}".encode()).hexdigest()[:8], 16)


# --- upscaling -------------------------------------------------------------------------------

def scale2x(a: np.ndarray) -> np.ndarray:
    """EPX / Scale2x on palette indices: doubles resolution, rounds staircase diagonals."""
    p = np.pad(a, 1, mode="edge")
    up, right, left, down = p[:-2, 1:-1], p[1:-1, 2:], p[1:-1, :-2], p[2:, 1:-1]
    h, w = a.shape
    out = np.empty((2 * h, 2 * w), a.dtype)
    out[0::2, 0::2] = np.where((left == up) & (left != down) & (up != right), up, a)
    out[0::2, 1::2] = np.where((up == right) & (up != left) & (right != down), right, a)
    out[1::2, 0::2] = np.where((down == left) & (down != right) & (left != up), left, a)
    out[1::2, 1::2] = np.where((right == down) & (right != up) & (down != left), down, a)
    return out


def upscale(idx: np.ndarray, zoom: int, exact_silhouette: bool) -> np.ndarray:
    """Upscale indices by 1, 2 or 4. With exact_silhouette the transparency mask is the
    nearest-neighbour one, so tiles keep tiling pixel-perfectly at every zoom."""
    out = idx
    for _ in range({1: 0, 2: 1, 4: 2}[zoom]):
        out = scale2x(out)
    if exact_silhouette and zoom > 1:
        # Ground: plain nearest neighbour, so tiles tile pixel-perfectly and thin markings stay
        # crisp; the smoothing downstream does the rest.
        return np.kron(idx, np.ones((zoom, zoom), dtype=idx.dtype))
    return out


# --- lattice-periodic fields -----------------------------------------------------------------

class Fields:
    """Tileable noise on the (32, 8) * zoom screen lattice, cached per zoom."""

    def __init__(self, style: Style) -> None:
        self.style = style
        self._cache: dict[int, tuple[np.ndarray, np.ndarray, np.ndarray]] = {}

    def at(self, zoom: int) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
        if zoom not in self._cache:
            U, V = LATTICE_U * zoom, LATTICE_V * zoom
            v, u = np.mgrid[0:V, 0:U].astype(np.float64)
            rng = np.random.default_rng(self.style.seed(f"wobble:{zoom}"))
            wobble = np.zeros((V, U))
            for a, b in [(1, 0), (2, 1), (-1, 1), (3, -1), (1, 2), (-4, 1), (5, 0)]:
                amp = 1.0 / (abs(a) + abs(b))
                wobble += amp * np.sin(2 * np.pi * (a * u / U + b * v / V) + rng.uniform(0, 2 * np.pi))
            wobble /= np.abs(wobble).max()
            rng = np.random.default_rng(self.style.seed(f"grain:{zoom}"))
            white = rng.random((V, U))
            sigma = self.style.raw["screen"]["grain_sigma"] * zoom
            grain = ndimage.gaussian_filter(white, sigma=sigma, mode="wrap")
            grain = (grain - grain.min()) / (np.ptp(grain) or 1.0)
            speckle = np.random.default_rng(self.style.seed(f"speckle:{zoom}")).random((V, U))
            self._cache[zoom] = (wobble, grain * 2 - 1, speckle)
        return self._cache[zoom]


def _tile(field: np.ndarray, u: np.ndarray, v: np.ndarray) -> np.ndarray:
    V, U = field.shape
    return field[np.mod(v, V), np.mod(u, U)]


# --- classification --------------------------------------------------------------------------

def classify(w: int, h: int, xrel: int, yrel: int) -> str:
    """ground: tile-shaped sprites drawn at the tile origin (terrain, track and road beds, water,
    foundations); gui: sprites anchored at their top-left (icons, cursors); object: the rest."""
    if w == 64 and xrel == -31:
        return "ground"
    if xrel >= 0 and yrel >= 0:
        return "gui"
    return "object"


# --- the stylizer ----------------------------------------------------------------------------

@dataclass
class Result:
    rgba: np.ndarray  # (h, w, 4) uint8
    mask: np.ndarray | None  # (h, w) uint8 palette mask, None when no pixel is recoloured
    offset: tuple[int, int] | None = None  # (xrel, yrel) in pixels of this zoom, if not the source's


class Stylizer:
    def __init__(self, style: Style, palette: np.ndarray) -> None:
        self.s = style
        self.fields = Fields(style)
        rgb = palette.astype(np.float64) / 255.0
        self.lum = rgb @ np.array([0.2126, 0.7152, 0.0722])
        mx, mn = rgb.max(1), rgb.min(1)
        sat = np.where(mx > 0, (mx - mn) / np.maximum(mx, 1e-9), 0)
        hue = np.zeros(256)
        d = np.maximum(mx - mn, 1e-9)
        r, g, b = rgb.T
        hue = np.where(mx == r, (60 * (g - b) / d) % 360,
                       np.where(mx == g, 60 * (b - r) / d + 120, 60 * (r - g) / d + 240))
        sig = style.raw["signal"]
        red_hue = (hue >= sig["hue_min"]) | (hue <= sig["hue_max"])
        self.red = red_hue & (sat >= sig["saturation"]) & (mx >= sig["value"])
        self.blue = (hue > 170) & (hue < 260) & (sat > 0.25)
        # Materials, one per palette index: OpenGFX's luminance alone does not tell grass from
        # asphalt, hue and saturation do. Each material has its own tone curve and its own cut
        # direction, the woodcut way of separating areas of equal value.
        mat = style.raw["material"]
        idx = np.arange(256)
        anim = (idx >= ANIM_FIRST) & (idx <= ANIM_LAST)
        self.water = anim & self.blue
        self.light = anim & ~self.blue
        self.cc = (idx >= CC_FIRST) & (idx <= CC_LAST)
        self.red &= ~self.cc
        veg = (hue >= 60) & (hue <= 170) & (sat > 0.18)
        earth = (hue > 15) & (hue < 60) & (sat > 0.22) & (mx < 0.7)
        neutral = sat <= 0.18
        self.material = np.select([self.water, veg, earth, neutral], [4, 1, 2, 3], 0).astype(np.int8)

        def curve(m: dict) -> np.ndarray:
            return m["base"] + m["slope"] * (self.lum - m["pivot"])

        self.tone_table = np.clip(np.select(
            [veg, earth, neutral], [curve(mat["vegetation"]), curve(mat["earth"]), curve(mat["neutral"])],
            curve(mat["other"])), 0, 1)
        self.ink, self.paper = style.colour("ink"), style.colour("paper")
        self.signal, self.signal_dark = style.colour("signal"), style.colour("signal_dark")

    @staticmethod
    def _masked_blur(x: np.ndarray, a: np.ndarray, sigma: float) -> np.ndarray:
        """Gaussian blur inside the silhouette only (normalised convolution)."""
        num = ndimage.gaussian_filter(x * a, sigma)
        den = ndimage.gaussian_filter(a, sigma)
        return np.where(den > 1e-6, num / np.maximum(den, 1e-6), x)

    @staticmethod
    def _heal_rim(up: np.ndarray, alpha: np.ndarray, zoom: int) -> np.ndarray:
        """OpenGFX shades the outermost pixel row of ground tiles darker, which reads as a grid
        once contrast is raised. Give the rim the colour of the nearest interior pixel; features
        that cross the border (roads, rails, rivers) continue because the interior carries them."""
        inner = ndimage.binary_erosion(alpha, iterations=zoom, border_value=0)
        if not inner.any():
            return up
        _, (iy, ix) = ndimage.distance_transform_edt(~inner, return_indices=True)
        return np.where(alpha & ~inner, up[iy, ix], up)

    def _materials(self, up: np.ndarray, a: np.ndarray, zoom: int) -> np.ndarray:
        """Material map cleaned by a smoothed majority vote, so the source's speckle (brown
        pixels in grass) does not become a forest of boundaries."""
        raw = self.material[up]
        votes = np.stack([self._masked_blur((raw == m).astype(np.float64), a, 0.8 * zoom)
                          for m in range(5)])
        return votes.argmax(0).astype(np.int8)

    def run(self, idx: np.ndarray, xrel: int, yrel: int, zoom: int, category: str) -> Result:
        """Restyle one OpenGFX 8bpp sprite (palette indices) at the given zoom."""
        ground = category == "ground"
        up = upscale(idx, zoom, exact_silhouette=ground)
        alpha = up != TRANSPARENT
        h, w = up.shape
        if not alpha.any():
            return Result(np.zeros((h, w, 4), np.uint8), None)
        if ground:
            up = self._heal_rim(up, alpha, zoom)
        t = self.s.raw["tone"]
        a = alpha.astype(np.float64)
        # Smooth inside the silhouette so pixel steps and the source's dithering become continuous
        # fields for the line screen; local contrast keeps the source's detail (markings, sleepers,
        # windows) that a material tone alone would flatten.
        smooth = self._masked_blur(self.lum[up], a, 0.45 * zoom)
        broad = self._masked_blur(smooth, a, t["local_radius"] * zoom)
        lift = t["object_lift"] if category == "object" else 0.0
        raw = np.clip(self.tone_table[up] + lift + t["local_contrast"] * (self.lum[up] - broad), 0, 1)
        tone = self._masked_blur(self.tone_table[up], a, 0.45 * zoom)
        tone = np.clip(tone + lift + t["local_contrast"] * (smooth - broad), 0, 1)
        mat = self._materials(up, a, zoom)
        red = self.red[up] & (not ground)
        cc = np.where(self.cc[up], up, 0).astype(np.uint8)
        hatch = self.s.raw["hatch"]
        names = ["other", "vegetation", "earth", "neutral", "water"]
        specs = [hatch["object"] if not ground and m != 4 else hatch[n] for m, n in enumerate(names)]
        return self._core(alpha, smooth, raw, tone, mat, mat, specs, red, self.light[up], cc,
                          xrel * zoom, yrel * zoom, zoom, category)

    def run_rgb(self, rgba: np.ndarray, nrm: np.ndarray, ox: int, oy: int, zoom: int,
                category: str = "object") -> Result:
        """Restyle a rendered model: lit colour plus world normals, offsets (ox, oy) in pixels of
        this zoom. Cut directions follow the faces (roof, left wall, right wall) instead of a
        material, as a carver follows form."""
        alpha = rgba[..., 3] > 127
        h, w = alpha.shape
        if not alpha.any():
            return Result(np.zeros((h, w, 4), np.uint8), None)
        rgb = rgba[..., :3].astype(np.float64) / 255.0
        n = nrm[..., :3].astype(np.float64) / 255.0 * 2 - 1
        a = alpha.astype(np.float64)
        lum = rgb @ np.array([0.2126, 0.7152, 0.0722])
        mx, mn = rgb.max(-1), rgb.min(-1)
        sat = np.where(mx > 0, (mx - mn) / np.maximum(mx, 1e-9), 0)
        d = np.maximum(mx - mn, 1e-9)
        r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
        hue = np.where(mx == r, (60 * (g - b) / d) % 360,
                       np.where(mx == g, 60 * (b - r) / d + 120, 60 * (r - g) / d + 240))
        rc = self.s.raw["render"]
        sig = self.s.raw["signal"]
        # Kenney's palette is warm (salmon trunks, orange roofs): only true reds become signal.
        red = (((hue >= 360 - rc["red_hue"]) | (hue <= rc["red_hue"])) & (sat >= rc["red_saturation"])
               & (mx >= sig["value"]) & alpha)
        mat = np.select([(hue >= 60) & (hue <= 200) & (sat > 0.18), (hue > 15) & (hue < 60) & (sat > 0.22),
                         sat <= 0.18], [1, 2, 3], 0).astype(np.int8)
        base = np.choose(mat, [rc["other"], rc["vegetation"], rc["earth"], rc["neutral"]])
        smooth = self._masked_blur(lum, a, 0.35 * zoom)
        broad = self._masked_blur(smooth, a, self.s.raw["tone"]["local_radius"] * zoom)
        # Shading relative to the model's own median, so the result does not depend on how bright
        # the render's lights are, only on how its faces differ.
        shade = rc["shade"] * (smooth - np.median(smooth[alpha]))
        tone = np.clip(base + shade + rc["local_contrast"] * (smooth - broad), 0, 1)
        raw = tone
        # Faces from normals: 0 roof / top, 1 left wall (faces map +x), 2 right wall (faces +y).
        face = np.where(n[..., 2] > 0.55, 0, np.where(n[..., 0] > n[..., 1], 1, 2)).astype(np.int8)
        hatch = self.s.raw["hatch"]
        specs = [hatch["top"], hatch["left"], hatch["right"]]
        # Carved edges where the surface turns: neighbouring normals more than ~35 degrees apart.
        dn = np.zeros((h, w), bool)
        for ax in (0, 1):
            nb = np.roll(n, 1, axis=ax)
            dn |= (np.einsum("ijk,ijk->ij", n, nb) < rc["crease"]) & np.roll(alpha, 1, axis=ax)
        dn &= alpha
        res = self._core(alpha, smooth, raw, tone, face, mat, specs, red, np.zeros_like(alpha),
                         np.zeros((h, w), np.uint8), ox, oy, zoom, category, creases=dn)
        res.offset = (ox, oy)
        return res

    def _core(self, alpha, smooth, raw, tone, dir_idx, mat, specs, red, light, cc, ox, oy,
              zoom, category, creases=None) -> Result:
        """Shared cut: line screen, edges, colour. dir_idx selects a hatch spec per pixel; mat is
        the material map (4 = water) used for tone overrides and ground boundaries."""
        ground = category == "ground"
        h, w = alpha.shape
        t, scr, e = self.s.raw["tone"], self.s.raw["screen"], self.s.raw["edges"]
        a = alpha.astype(np.float64)
        water = mat == 4
        if red.any():  # dithered brick becomes one red shape, not red noise
            red = self._masked_blur(red.astype(np.float64), a, 0.9 * zoom) > self.s.raw["signal"]["region"]
        tone = np.where(water, self.s.raw["material"]["water"], tone)

        v, u = np.mgrid[0:h, 0:w]
        u = u + ox
        v = v + oy
        wobble, grain, speckle = self.fields.at(zoom)
        if zoom == 4:
            P = scr["line_period_zi4"] if ground else scr["line_period_object_zi4"]
        else:
            P = scr["line_period"] * zoom
        wob = _tile(wobble, u, v) * scr["wobble"] * (1.0 if ground else 0.5)
        grn = _tile(grain, u, v) * scr["grain"]
        spk = _tile(speckle, u, v)

        def dist(phase: np.ndarray) -> np.ndarray:  # 0 at a cut line's centre, 1 between lines
            return np.abs(np.mod(phase, 1.0) - 0.5) * 2

        lines = {
            "a": dist((u - 2 * v) / P + wob),
            "b": dist((u + 2 * v) / P - wob),
            "v": dist(u / P + 0.5 * wob),
            "h": dist(v / (P * 0.5) + 2.5 * wob),
            "-": np.ones((h, w)),
        }
        l1 = np.choose(dir_idx, [lines[s["dirs"][0]] for s in specs])
        l2 = np.choose(dir_idx, [lines[(s["dirs"] + ["-"])[1]] for s in specs])
        cross = np.choose(dir_idx, [s["cross"] for s in specs])

        d = 1.0 - tone + grn
        a1 = np.clip((d - t["single"]) / (t["solid"] - t["single"]), 0, 1)
        a2 = np.clip((d - cross) / np.maximum(t["solid"] - cross, 1e-6), 0, 1)
        if category == "gui":
            ink = d > 0.55
        else:
            ink = (l1 < a1) | (l2 < a2)
            ink = np.where(raw >= t["keep_paper"], False, np.where(raw <= t["keep_ink"], True, ink))
        ink = np.where(spk < scr["speckle"], ~ink, ink)

        if ground:
            # Cut a paper line where vegetation meets built ground or water (kerbs, ballast,
            # shores): coarse regions only, smoothed hard, so alternating sleepers do not become a
            # chain of bubbles; and only inside the sprite, so no tile grid.
            r = max(1, zoom // 2)
            coarse = np.choose(mat, [0, 1, 0, 0, 2]).astype(np.int8)
            votes = np.stack([self._masked_blur((coarse == m).astype(np.float64), a, 1.6 * zoom)
                              for m in range(3)])
            coarse = votes.argmax(0).astype(np.int8)
            grown = ndimage.grey_dilation(coarse, size=2 * r + 1)
            shrunk = ndimage.grey_erosion(coarse, size=2 * r + 1)
            inner = ndimage.binary_erosion(alpha, iterations=r + 1, border_value=0)
            boundary = (grown != shrunk) & inner & ~water
            ink = np.where(boundary, False, ink)
        else:
            # Carved outline, internal edges, and a paper highlight along the lit upper-left rim.
            gm = np.hypot(ndimage.sobel(smooth, 0), ndimage.sobel(smooth, 1)) / 8.0
            edge = (gm > e["internal"] / zoom) & alpha
            ink = np.where(edge, tone > 0.5, ink)
            if creases is not None:  # facets carved like any cut: paper in dark, ink in light
                cr = ndimage.binary_dilation(creases, iterations=max(0, zoom // 4)) & alpha
                ink = np.where(cr, tone > 0.5, ink)
            r = max(1, round(e["outline"] * zoom))
            inner = ndimage.binary_erosion(alpha, iterations=r, border_value=0)
            lit = alpha & ~np.roll(np.roll(alpha, zoom, 0), zoom, 1)
            ink = np.where(lit & inner & (tone > 0.30), False, ink)
            rim = alpha & ~inner
            if e["outline_colour"] == "paper":
                # The world is mostly ink, so objects are cut free of it by a paper rim with an
                # ink line just inside, the way a figure is cleared from a block.
                inner2 = ndimage.binary_erosion(inner, iterations=r, border_value=0)
                ink = np.where(rim, False, np.where(inner & ~inner2, True, ink))
            else:
                ink |= rim

        rgb = np.where(ink[..., None], self.ink, np.where(red[..., None], self.signal, self.paper))
        rgb = np.where((light & ~ink)[..., None], self.signal, rgb)
        mask = None
        keep = (cc > 0) & ~ink
        if keep.any():
            mask = np.where(keep, cc, 0).astype(np.uint8)
            rgb = np.where(keep[..., None], np.uint8(128), rgb)  # neutral brightness for the remap
        out = np.zeros((h, w, 4), np.uint8)
        out[..., :3] = rgb
        out[..., 3] = np.where(alpha, 255, 0)
        out[~alpha, :3] = 0
        return Result(out, mask)
