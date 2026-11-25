# Pitfalls

These are problems that we found during development. Each one cost time.
Most of them give no error message. Read this list before you change the
fonts, the display buffer, the touch code or the JSON client.

## Data and the MCP client

### The MCP response is JSON inside JSON

The tool result is a *string* in `result.content[0].text`. You must decode it
two times. `smhi_client.cpp` uses two `DeserializationOption::Filter` steps.
Thus the envelope and the unused fields never use heap memory.

### Cloudflare returns 403 when there is no `User-Agent`

The Cloudflare edge rejects the request before the worker receives it. Do not
remove the `User-Agent` header (`MCP_USER_AGENT` in `config.h`).

### `cloudCover` is in oktas (0–8), not in percent

The panel shows `5/8`. This is also the correct meteorological notation. If
you show `%`, the value is incorrect by a factor of 12.

### `precipitation` is an ensemble mean

The SMHI forecast uses an ensemble. A period can show precipitation and still
be dry. A bar means "expected", not "certain".

### The data has no feels-like temperature

The server supplies no feels-like temperature and no wind gusts. The ESP32
calculates the feels-like value as wind chill (JAG/TI, m/s form) when
T ≤ 10 °C and the wind is 1 m/s or more. In other conditions, it uses the air
temperature. This agrees with the SMHI app on all rows that we compared. The
panel does not show wind gusts.

## Fonts

### Compressed fonts draw nothing

`lv_font_conv` compresses fonts by default. LVGL decodes them only when
`LV_USE_FONT_COMPRESSED` is 1. If it is 0, `lv_font_get_glyph_bitmap()`
returns NULL, but `lv_font_get_glyph_dsc()` continues to return the *correct
dimensions*. Each label thus uses the correct area and draws zero pixels. The
layout is pixel-perfect and the panel shows no text.

LVGL logs a warning, but `LV_USE_LOG` is 0, so you do not see it.
`tools/make_fonts.sh` uses `--no-compress`. At start, `ui_init()` checks one
glyph and writes an error to the log if the bitmap is missing.

### The built-in LVGL fonts have no å, ä, ö or °

`lv_font_montserrat_*` contains only 0x20–0x7F. There is no error message. The
glyphs are simply not shown. Run `tools/make_fonts.sh`.

## Display and memory

### `hor_res`/`ver_res` must be the *rotated* dimensions

The native panel size is 240 × 320. In landscape mode, LVGL must get
320 × 240. If not, it draws outside the buffer.

### `lv_line` does not copy its points

For this reason, `curve_pts` is `static`. If you put it on the stack, LVGL
draws random data.

### DRAM is full before flash is full

The draw buffer is in `dram0_0_seg`, together with the 48 kB LVGL heap and the
Wi-Fi and TLS stacks. With 40 rows (25 kB), the *link* step fails:
`region 'dram0_0_seg' overflowed by 200 bytes`. 24 rows give a margin. You see
this error only at link time, not at compile time.

### The default partition table is too small

The 32 px font alone is approximately 140 kB of bitmap. With the default
`esp32dev` table, the build uses **91.9 %** of 1.3 MB.
`board_build.partitions = huge_app.csv` gives 3 MB and the build uses 38 %.

## Touch

### `TOUCH_CS` is a TFT_eSPI name

TFT_eSPI enables its own touch code when `TOUCH_CS` is defined. The
`TFT_eSPI` class then gets different members. If `config.h` defines this
name, files that include `config.h` before `TFT_eSPI.h` get a different class
layout than the other files. There is no compile error, only memory
corruption. For this reason, the touch pins have the prefix `TP_`.

### The touch controller is bit-banged, not on the SPI bus

The XPT2046 uses its own pins (CS 33, CLK 25, DIN 32, DOUT 39). The firmware
reads it with Freenove's `TFT_Touch` v0.3 in `lib/TFT_Touch/`.

The first reading with pressure counts as a touch. The next touch counts only
after the panel has been released for approximately 90 ms. Thus one touch
gives one page change. A rule that required two readings in sequence missed
light touches. With no touch, the panel reads exactly 0/4095, so there is no
noise to filter. The touch position is not used, so the panel needs no
calibration.

## Sun calculation

### The longitude sign

Use `n - lon/360` for east longitude. A wrong sign still gives the *correct
day length*, but incorrect clock times. A test that checks only the day length
does not find this bug. Verified for Skellefteå: 06:14/18:43 at the equinox,
09:41/13:26 at midwinter, 01:45/23:29 at midsummer.

### Sunset is 2–3 minutes different from the SMHI app

The SMHI app gave 18:35 for Skellefteå on 2026-09-23. `sun_times()` gives
18:38. The night bands use the same NOAA formula. The difference is probably
refraction or the exact coordinate. The tests have a 5-minute tolerance.

## Build and tools

### `-U` after an interpolated flag list in `platformio.ini`

PlatformIO changes it incorrectly, and gcc reports
`macro names must be identifiers`. Put the shared flags in `[common]` and let
each environment define its own display driver.

### `DNS Failed` at the first start

We saw this one time at a cold start, with a working Wi-Fi link. DHCP *had*
given a DNS server, so it was a race condition, not a configuration error.
`wifi_up()` now logs SSID, RSSI, IP, gateway and DNS, and resolves the host
name immediately. If the problem occurs again, one log line shows if the cause
is the link or the DNS lookup.

### The serial port needs permissions (Linux)

Users are often not in the `dialout` group, and a group change needs a new
login. A udev rule that gives the port to the `plugdev` group works
immediately when you connect the board. Example
`/etc/udev/rules.d/99-esp32-serial.rules`:

```
SUBSYSTEM=="tty", ATTRS{idVendor}=="1a86", ATTRS{idProduct}=="7523", GROUP="plugdev", MODE="0660"
```
