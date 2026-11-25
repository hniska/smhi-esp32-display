#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# dependencies = ["pillow"]
# ///
"""Väderikoner för timtabellen, ritade här i stället för hämtade.

Varje ikon ritas med Pillow i 4x storlek i panelens palett och skalas ner till
20 x 20, så kanterna blir mjuka mot den mörka bakgrunden. Ingen extern
ikonuppsättning och alltså inga licensfrågor.

Utdata:
  SMHI_Display/icons/icons.c   LVGL 8, LV_IMG_CF_TRUE_COLOR_ALPHA, RGB565 utan
                               byteswap (LV_COLOR_DEPTH 16, LV_COLOR_16_SWAP 0)
  design/icons/*.png           samma ikoner för förhandsvisningen

Ordningen i ICONS måste vara densamma som enum IconId i SMHI_Display/detail.h.

    ./tools/make_icons.py
"""
import pathlib

from PIL import Image, ImageChops, ImageDraw

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT_C = ROOT / "SMHI_Display" / "icons" / "icons.c"
OUT_PNG = ROOT / "design" / "icons"

SIZE = 20
K = 4               # översampling
S = SIZE * K

# Samma hex som theme.h.
SUN = (0xF2, 0xA0, 0x3D)
MOON = (0xD8, 0xE2, 0xE8)
CLOUD = (0xB8, 0xC7, 0xD0)
CLOUD_DARK = (0x7B, 0x91, 0x9E)
RAIN = (0x3F, 0xAF, 0xD4)
SNOW = (0xE8, 0xF0, 0xF4)
BOLT = (0xF5, 0xD2, 0x3B)
FOG = (0x7B, 0x91, 0x9E)


def layer():
    return Image.new("RGBA", (S, S), (0, 0, 0, 0))


def cut(under, shape, grow):
    """Nolla alfa i `under` där `shape` (utvidgad med `grow` px) ligger.
    Ger den smala glipan mellan moln och sol som gör ikonen läsbar i 20 px."""
    m = shape.getchannel("A").point(lambda a: 255 if a else 0)
    if grow:
        from PIL import ImageFilter
        m = m.filter(ImageFilter.MaxFilter(grow * 2 + 1))
    a = ImageChops.subtract(under.getchannel("A"), m)
    under.putalpha(a)


def over(*layers):
    out = layer()
    for l in layers:
        out = Image.alpha_composite(out, l)
    return out


def sun(cx, cy, r, rays=True):
    im = layer()
    d = ImageDraw.Draw(im)
    if rays:
        import math
        for i in range(8):
            a = i * math.pi / 4
            x1, y1 = cx + math.cos(a) * (r + 5), cy + math.sin(a) * (r + 5)
            x2, y2 = cx + math.cos(a) * (r + 13), cy + math.sin(a) * (r + 13)
            d.line([(x1, y1), (x2, y2)], fill=SUN, width=6)
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=SUN)
    return im


def moon(cx, cy, r):
    im = layer()
    ImageDraw.Draw(im).ellipse([cx - r, cy - r, cx + r, cy + r], fill=MOON)
    bite = layer()
    # Mindre och längre bort förskjutet bett ger en fylligare skära; en tunn
    # skära försvinner helt i 20 px.
    br, ox, oy = r * 0.8, r * 0.75, -r * 0.45
    ImageDraw.Draw(bite).ellipse([cx + ox - br, cy + oy - br, cx + ox + br, cy + oy + br],
                                 fill=MOON)
    cut(im, bite, 0)
    return im


def cloud(x, y, w, color):
    """Moln med nederkant i y, vänsterkant i x, bredd w."""
    im = layer()
    d = ImageDraw.Draw(im)
    h = w * 0.42
    d.rounded_rectangle([x, y - h, x + w, y], radius=h / 2, fill=color)
    r1 = w * 0.26
    d.ellipse([x + w * 0.18 - r1 + r1, y - h - r1 * 0.9, x + w * 0.18 + r1 + r1, y - h + r1 * 1.1],
              fill=color)
    r2 = w * 0.20
    d.ellipse([x + w * 0.62 - r2, y - h - r2 * 0.5, x + w * 0.62 + r2, y - h + r2 * 1.5], fill=color)
    return im


def drops(xs, y, n_len=16, color=RAIN):
    im = layer()
    d = ImageDraw.Draw(im)
    for x in xs:
        d.line([(x, y), (x - 5, y + n_len)], fill=color, width=7)
    return im


def flakes(pts, color=SNOW, r=4):
    im = layer()
    d = ImageDraw.Draw(im)
    for x, y in pts:
        d.ellipse([x - r, y - r, x + r, y + r], fill=color)
    return im


def bolt():
    im = layer()
    ImageDraw.Draw(im).polygon([(44, 50), (32, 66), (40, 66), (34, 79), (52, 60), (43, 60), (50, 50)],
                               fill=BOLT)
    return im


def with_cloud(back, c):
    cut(back, c, 4)
    return over(back, c)


def sky_with_cloud(body, cloud_w, cloud_x, cloud_y, color=CLOUD):
    c = cloud(cloud_x, cloud_y, cloud_w, color)
    return with_cloud(body, c)


# --- ikonerna, i IconId-ordning -------------------------------------------------

def clear_d(): return sun(40, 40, 15)
def clear_n(): return moon(40, 40, 22)
def fair_d(): return sky_with_cloud(sun(32, 32, 14), 44, 32, 74)
def fair_n(): return sky_with_cloud(moon(32, 32, 22), 44, 32, 74)
def partly_d(): return sky_with_cloud(sun(30, 30, 13), 58, 16, 70)
def partly_n(): return sky_with_cloud(moon(30, 28, 21), 58, 16, 70)
def cloudy(): return over(cloud(6, 64, 68, CLOUD))
def overcast():
    back = cloud(20, 52, 56, CLOUD_DARK)
    return with_cloud(back, cloud(4, 68, 62, CLOUD))
def fog():
    im = cloud(10, 46, 60, CLOUD)
    d = ImageDraw.Draw(im)
    for i, y in enumerate((56, 66, 76)):
        x0 = 8 + (i % 2) * 8
        d.line([(x0, y), (x0 + 60, y)], fill=FOG, width=6)
    return im


def _rain_cloud(top):
    # Mindre moln så att solen/månen bakom syns: det är den enda skillnaden
    # mellan skurar och regn.
    return with_cloud(top, cloud(4, 54, 58, CLOUD))


def shower_d(): return over(_rain_cloud(sun(54, 24, 14)), drops((24, 44), 60))
def shower_n(): return over(_rain_cloud(moon(54, 22, 21)), drops((24, 44), 60))
def rain(): return over(cloud(6, 52, 66, CLOUD_DARK), drops((22, 40, 58), 60))
def thunder(): return over(cloud(6, 50, 66, CLOUD_DARK), bolt(), drops((22, 62), 60))
def sleet(): return over(cloud(6, 52, 66, CLOUD), drops((24, 56), 60), flakes([(40, 68)], r=5))
def snowshower_d(): return over(_rain_cloud(sun(54, 24, 14)), flakes([(20, 64), (38, 72), (46, 60)], r=5))
def snowshower_n(): return over(_rain_cloud(moon(54, 22, 21)), flakes([(20, 64), (38, 72), (46, 60)], r=5))
def snow(): return over(cloud(6, 52, 66, CLOUD), flakes([(20, 64), (38, 74), (40, 60), (58, 68)], r=5))


def _horizon_sun(up):
    im = sun(40, 58, 13)
    # bara övre halvan ovanför horisonten
    ImageDraw.Draw(im).rectangle([0, 60, S, S], fill=(0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.line([(6, 64), (74, 64)], fill=SUN, width=5)
    # pil: upp vid soluppgång, ner vid solnedgång
    if up:
        d.line([(40, 30), (40, 6)], fill=SNOW, width=5)
        d.polygon([(30, 16), (50, 16), (40, 3)], fill=SNOW)
    else:
        d.line([(40, 4), (40, 26)], fill=SNOW, width=5)
        d.polygon([(30, 20), (50, 20), (40, 33)], fill=SNOW)
    return im


def sunrise(): return _horizon_sun(True)
def sunset(): return _horizon_sun(False)


ICONS = [
    ("clear_d", clear_d), ("clear_n", clear_n),
    ("fair_d", fair_d), ("fair_n", fair_n),
    ("partly_d", partly_d), ("partly_n", partly_n),
    ("cloudy", cloudy), ("overcast", overcast), ("fog", fog),
    ("shower_d", shower_d), ("shower_n", shower_n),
    ("rain", rain), ("thunder", thunder), ("sleet", sleet),
    ("snowshower_d", snowshower_d), ("snowshower_n", snowshower_n),
    ("snow", snow),
    ("sunrise", sunrise), ("sunset", sunset),
]


def to_lvgl(im: Image.Image) -> bytes:
    out = bytearray()
    px = im.convert("RGBA").tobytes()
    for r, g, b, a in zip(px[0::4], px[1::4], px[2::4], px[3::4]):
        v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
        out += bytes((v & 0xFF, v >> 8, a))
    return bytes(out)


def main():
    OUT_C.parent.mkdir(parents=True, exist_ok=True)
    OUT_PNG.mkdir(parents=True, exist_ok=True)

    parts = [
        "// GENERERAD av tools/make_icons.py — redigera inte för hand.\n",
        "// LV_IMG_CF_TRUE_COLOR_ALPHA, RGB565 (little endian) + alfa, "
        f"{SIZE}x{SIZE}.\n",
        '#include "lvgl.h"\n#include "icons.h"\n\n',
    ]
    names = []
    for name, fn in ICONS:
        big = fn()
        small = big.resize((SIZE, SIZE), Image.LANCZOS)
        small.save(OUT_PNG / f"{name}.png")
        data = to_lvgl(small)
        rows = [", ".join(f"0x{b:02x}" for b in data[i:i + 24]) for i in range(0, len(data), 24)]
        parts.append(f"static const uint8_t {name}_map[] = {{\n  " + ",\n  ".join(rows) + "\n};\n")
        parts.append(
            f"static const lv_img_dsc_t {name} = {{\n"
            f"  .header = {{ .cf = LV_IMG_CF_TRUE_COLOR_ALPHA, .always_zero = 0, .reserved = 0,\n"
            f"              .w = {SIZE}, .h = {SIZE} }},\n"
            f"  .data_size = {len(data)},\n"
            f"  .data = {name}_map,\n}};\n\n")
        names.append(name)

    parts.append(f"const lv_img_dsc_t *const ICON_IMG[{len(names)}] = {{\n")
    parts.append("".join(f"  &{n},\n" for n in names))
    parts.append("};\n")
    OUT_C.write_text("".join(parts))

    (OUT_C.parent / "icons.h").write_text(
        "#pragma once\n"
        "// GENERERAD av tools/make_icons.py. Index = enum IconId i detail.h.\n"
        '#include "lvgl.h"\n\n'
        f"#define ICON_SIZE {SIZE}\n"
        f"#define ICON_COUNT {len(names)}\n"
        "#ifdef __cplusplus\nextern \"C\" {\n#endif\n"
        f"extern const lv_img_dsc_t *const ICON_IMG[{len(names)}];\n"
        "#ifdef __cplusplus\n}\n#endif\n")

    # Kontaktark i 4x för att granska ikonerna på riktigt panelunderlag.
    sheet = Image.new("RGB", (len(names) * (SIZE + 4) * 4, (SIZE + 4) * 4), (0x0A, 0x0F, 0x13))
    for i, (name, _) in enumerate(ICONS):
        im = Image.open(OUT_PNG / f"{name}.png").resize((SIZE * 4, SIZE * 4), Image.NEAREST)
        sheet.paste(im, (i * (SIZE + 4) * 4 + 8, 8), im)
    sheet.save(OUT_PNG / "_sheet.png")
    print(f"{len(names)} ikoner -> {OUT_C.relative_to(ROOT)}, {OUT_PNG.relative_to(ROOT)}/")


if __name__ == "__main__":
    main()
