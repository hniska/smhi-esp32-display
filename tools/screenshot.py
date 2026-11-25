#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# dependencies = ["pyserial", "pillow"]
# ///
"""Hämta en skärmdump från panelen över USB och spara som PNG.

Firmwaren svarar på 's' med en fullständig omritning där varje remsa LVGL
lämnar ifrån sig skrivs ut som base64 — samma pixlar som går till skärmen.
Det finns ingen framebuffer i ESP32:n att läsa av, så dumpen fångas i farten.

    ./tools/screenshot.py                     -> skarm.png
    ./tools/screenshot.py -c r -o panel.png   panelens eget minne i stället
    ./tools/screenshot.py --page 1            timtabellen (0 = meteogram)

Stäng `pio device monitor` först; porten går bara att öppna en gång.
"""
import argparse
import base64
import sys
import time

import serial
from PIL import Image


def rgb565(word: int) -> tuple[int, int, int]:
    r, g, b = (word >> 11) & 0x1F, (word >> 5) & 0x3F, word & 0x1F
    # Skala så att max blir 255 och inte 248/252 — annars blir vitt grått.
    return (r * 255 // 31, g * 255 // 63, b * 255 // 31)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("-p", "--port", default="/dev/ttyUSB0")
    ap.add_argument("-b", "--baud", type=int, default=115200)
    ap.add_argument("-o", "--out", default="skarm.png")
    ap.add_argument("-c", "--cmd", default="s", choices=["s", "r"],
                    help="s = vad LVGL ritar (exakt), r = panelens minne")
    ap.add_argument("--page", type=int, choices=[0, 1, 2],
                    help="byt till den här sidan före dumpen")
    ap.add_argument("--swap", action="store_true", help="byt byteordning")
    ap.add_argument("--timeout", type=float, default=90.0)
    args = ap.parse_args()

    img = None
    painted = 0
    seen: set[int] = set()

    with serial.Serial(args.port, args.baud, timeout=2) as ser:
        time.sleep(0.2)
        ser.reset_input_buffer()
        if args.page is not None:
            ser.write(str(args.page).encode())
            ser.flush()
            time.sleep(0.5)
        ser.write(args.cmd.encode())
        ser.flush()

        deadline = time.time() + args.timeout
        area = None          # (x1, y1, x2, y2)
        row = 0

        while time.time() < deadline:
            raw = ser.readline()
            if not raw:
                continue
            line = raw.decode("utf-8", "replace").strip()

            if line.startswith("[shot] begin"):
                w, h = (int(x) for x in line.split()[2:4])
                img = Image.new("RGB", (w, h), (255, 0, 255))   # magenta = oritad
                print(f"tar emot {w}x{h}…", file=sys.stderr)
                continue
            if line.startswith("[shot] end"):
                break
            if line.startswith("[shot] area"):
                area = tuple(int(x) for x in line.split()[2:6])
                row = 0
                continue
            if img is None or area is None:
                continue                      # firmwarens vanliga loggrader

            try:
                data = base64.b64decode(line, validate=True)
            except Exception:
                continue
            x1, y1, x2, _ = area
            w = x2 - x1 + 1
            if len(data) < w * 2:
                continue
            px = img.load()
            y = y1 + row
            if 0 <= y < img.height:
                for i in range(w):
                    lo, hi = data[2 * i], data[2 * i + 1]
                    word = (lo << 8) | hi if args.swap else (hi << 8) | lo
                    seen.add(word)
                    px[x1 + i, y] = rgb565(word)
                painted += 1
            row += 1

    if img is None:
        print("panelen svarade aldrig — sitter console.cpp i firmwaren?",
              file=sys.stderr)
        return 1

    img.save(args.out)
    print(f"sparade {args.out}  ({painted} rader, {len(seen)} unika färger)")
    if painted < img.height:
        print(f"  varning: {img.height - painted} rader oritade (magenta)",
              file=sys.stderr)
    if len(seen) <= 8:
        print("  Väldigt få färger. Med -c r betyder det att panelens "
              "minnesläsning inte fungerar här; med -c s är det på riktigt.",
              file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
