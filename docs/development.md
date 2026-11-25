# Development

This document gives more information for developers. For the quick start,
refer to the [README](../README.md).

## Libraries

The versions are the same as the versions that Freenove supplies in the
`Libraries/` folder of
[Freenove_ESP32_Display](https://github.com/Freenove/Freenove_ESP32_Display).
`platformio.ini` pins them.

| Library | Version | Notes |
|---|---|---|
| LVGL | 8.4.0 | LVGL 9 has a different API. This code does not compile with LVGL 9. |
| TFT_eSPI | 2.5.43 | `platformio.ini` sets the pins. You do not edit `User_Setup.h`. |
| ArduinoJson | 7.x | |
| TFT_Touch | 0.3 | Included in `lib/TFT_Touch/`. |

## Panel variants

The code is for a **240 × 320 panel in landscape mode** (2.4" or 2.8", ST7789
or ILI9341). Both variants use the same pins.

| PlatformIO environment | Panel | Freenove `TFT_eSPI_Setups` folder |
|---|---|---|
| `fnk0103_2p8_st7789` (default) | 2.4" or 2.8" ST7789 | `FNK0114A_2.4inch_ST7789`, `FNK0114B_2.8inch_ST7789` |
| `fnk0103_2p8_ili9341` | 2.8" ILI9341 | `FNK0114F_2.8inch_ILI9341` |

For the 3.5" and 4.0" ST7796 panels (320 × 480), you must change the geometry
in `SMHI_Display/theme.h` and `tools/build_preview.py`. The two files have the
same values on purpose. Thus the preview and the firmware always show the same
layout.

## Build with the Arduino IDE

PlatformIO is the recommended and tested method. The Arduino IDE also works,
but it needs more manual steps:

1. Copy `SMHI_Display/config.example.h` to `SMHI_Display/config.h` and edit it.
2. Install the libraries from Freenove's `Libraries/` folder, also `TFT_Touch`
   from `Libraries/FNK0114B_2.8inch_ST7789/TFT_Touch_v0.3.zip`. The Arduino
   IDE does not read `lib/`.
3. The Arduino IDE does not read `platformio.ini`. Select the correct
   `User_Setup` from `TFT_eSPI_Setups`, and put `include/lv_conf.h` next to
   the `lvgl` folder.
4. Select a partition scheme with a large app partition ("Huge APP").
5. Open `SMHI_Display/SMHI_Display.ino` and upload.

To build without the custom fonts, use `-DUSE_SV_FONTS=0`. The panel then uses
the built-in LVGL fonts. You lose å, ä, ö and °, but all other functions work.

## Generated files

The generated files are in the repository. Make them again only when you
change the fonts, sizes or icons.

```bash
./tools/make_fonts.sh        # Montserrat with Swedish characters -> SMHI_Display/fonts/
./tools/make_icons.py        # weather icons with Pillow -> SMHI_Display/icons/
```

`make_fonts.sh` must use `--no-compress`. Refer to
[pitfalls.md](pitfalls.md#compressed-fonts-draw-nothing).

## Memory use

Build from 2026-11-25, ST7789 environment:

| | Used |
|---|---|
| RAM | 36.2 % (118 656 of 327 680 bytes) |
| Flash | 40.4 % (1 269 353 of 3 145 728 bytes) |

The draw buffer has 24 rows. With more rows, the link step can fail. Refer to
[pitfalls.md](pitfalls.md#dram-is-full-before-flash-is-full).

## Serial console

The firmware reads one-character commands on the serial port (115200 baud).
Refer to `SMHI_Display/console.cpp`.

| Command | Function |
|---|---|
| `s` | Screenshot: draw the screen again and send each strip as base64. |
| `r` | Read back the panel memory (command `0x2E`). |
| `p` | Next page. This is the same as a touch on the screen. |
| `0` `1` `2` | Go to the meteogram, hours page 1 or hours page 2. |
| `m` | Free memory: LVGL heap (48 kB) and ESP32 heap. |
| `T` | Raw touch values for 3 seconds. With no touch, the value is `x=0 y=4095`. |
| `f` | Font check: dimensions **and** bitmap for each glyph. |
| `x` | Text sample with each font on top of the user interface. |
| `t` | Colour bars for 5 seconds. This shows if the panel or the user interface has a problem. |
| `b` | Backlight on/off. |

The `f` command found the cause of the empty panel: it showed `tom` (empty)
for each glyph in our fonts and `ok` for the LVGL fonts. This pointed directly
to the bitmap, not to colours, rotation or layout.

## Screenshots over USB

```bash
./tools/screenshot.py -p PORT                    # -> skarm.png
./tools/screenshot.py -p PORT --page 1 -o h.png  # hour table, page 1
./tools/screenshot.py -p PORT -c r -o panel.png  # panel memory
```

`PORT` is for example `/dev/ttyUSB0` (Linux) or `/dev/cu.usbserial-*`
(macOS). The default is `/dev/ttyUSB0`. Close `pio device monitor` first,
because only one program can open the port.

The ESP32 has no framebuffer. LVGL draws in a 24-row strip, sends it to the
panel and then uses the strip again. Thus the screenshot is captured during a
redraw: `s` starts a redraw with `lv_refr_now()`, and `disp_flush()` also
sends each strip as base64. These are the exact pixels that the panel gets.
A full screen takes approximately 18 seconds at 115200 baud.

`-c r` reads back the panel memory. This answers a different question: *what
does the panel contain?* and not *what did LVGL draw?*. This needs MISO, which
is connected on the FNK0103. The ST7789 answers in RGB666 with a dummy cycle
that TFT_eSPI does not read correctly here. The image thus has only eight
colours. The geometry is correct, but the colours are not.

## Design preview

```bash
python3 tools/build_preview.py      # build from the saved sample data
./tools/fetch_samples.sh            # get new sample data, then build
./tools/render_readme_images.py     # make docs/images/*.png from the preview
```

`build_preview.py` makes `design/preview.html` (standalone) and
`design/artifact.html` (for publication) from one template. Thus the two files
are always the same.

There are two real data sets: Skellefteå (dry) and Östersund (rain). A design
that you check only with dry data does not show if the precipitation layer
works.

### Palette: "Norrland night"

| Role | Colour | Notes |
|---|---|---|
| Background | `#0A0F13` | Almost black with a blue tint. Pure `#000` looks bad on the ST7789. |
| Chart area | `#151F27` | Daylight. |
| Night bands | `#080D11` | |
| Lines | `#22303A` | |
| Text | `#E8F0F4` | |
| Labels | `#7B919E` | |
| Temperature | `#F2A03D` | Amber. |
| Precipitation | `#3FAFD4` | Cold blue. |
| Fresh / old data | `#54C98A` / `#D2603C` | |

The panel has only a dark mode, on purpose. It is a backlit TFT, not a web
page.

### Night bands

The night bands behind the curve are calculated from the location. At
64.75° N the day length changes from **3 h 45 min** to **21 h 45 min** during
the year. Thus the dark/light ratio gives real information here. The night
bands are the only structural element in the design that shows true
information about the data, and is not only decoration.

## Continuous integration

The GitHub Actions workflow (`.github/workflows/ci.yml`) runs the host tests
and builds the firmware for both panel variants. It uses
`config.example.h` as `config.h`.
