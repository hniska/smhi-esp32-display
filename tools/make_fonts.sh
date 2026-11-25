#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# Builds Latin-1 Montserrat fonts for LVGL.
#
# WHY THIS EXISTS: LVGL's built-in lv_font_montserrat_* faces are compiled with
# the range 0x20-0x7F only. They have no å, ä, ö — and no ° either. Without
# this step "SKELLEFTEÅ" renders as "SKELLEFTE", "Måttligt regn" loses its å,
# and every temperature loses its degree sign. Nothing errors; glyphs just
# silently vanish, which is the worst way to find out.
#
# Needs: node/npx (for lv_font_conv) and uvx (for fontTools).
# Output: SMHI_Display/fonts/montserrat_sv_*.c — add them to the sketch folder
# and Arduino compiles them automatically.
# ---------------------------------------------------------------------------
set -euo pipefail

cd "$(dirname "$0")/.."
OUT="SMHI_Display/fonts"
WORK=".fontwork"
mkdir -p "$OUT" "$WORK"

VAR_TTF="$WORK/Montserrat[wght].ttf"
if [ ! -f "$VAR_TTF" ]; then
  echo "==> hämtar Montserrat (variabel)"
  # Google Fonts ships Montserrat ONLY as a variable font now; the old
  # static/Montserrat-Regular.ttf path is a 404.
  curl -sL -o "$VAR_TTF" \
    "https://github.com/google/fonts/raw/main/ofl/montserrat/Montserrat%5Bwght%5D.ttf"
fi

# lv_font_conv cannot pick a weight out of a variable font, so each weight is
# instanced to a static TTF first.
instance() {  # $1 = weight
  local w="$1" f="$WORK/Montserrat-$1.ttf"
  [ -f "$f" ] && { echo "$f"; return; }
  uvx --quiet --from fonttools fonttools varLib.instancer \
      "$VAR_TTF" "wght=$w" -o "$f" >/dev/null 2>&1
  echo "$f"
}

# size : weight  — matches theme.h's F_TINY / F_SMALL / F_BODY / F_TEMP / F_HERO
build() {
  local size="$1" weight="$2"
  local ttf; ttf="$(instance "$weight")"
  echo "==> montserrat_sv_$size (vikt $weight)"
  # --no-compress är inte en optimering, det är ett krav.
  #
  # lv_font_conv komprimerar som standard (RLE+XY). LVGL avkodar det bara om
  # LV_USE_FONT_COMPRESSED är 1, och är det 0 returnerar
  # lv_font_get_glyph_bitmap() NULL — medan lv_font_get_glyph_dsc() fortsätter
  # svara med rätt mått. Följden är att varje etikett reserverar exakt rätt yta
  # och ritar noll pixlar. Ingen krasch, inget felmeddelande (LVGL loggar en
  # varning, men LV_USE_LOG är 0), bara en panel utan text.
  #
  # Okomprimerat ritar också snabbare, vilket är rätt avvägning på en ESP32 med
  # 3 MB flash.
  npx --yes lv_font_conv@latest \
    --font "$ttf" \
    --size "$size" \
    --bpp 4 \
    --no-compress \
    --format lvgl \
    --force-fast-kern-format \
    --lv-include lvgl.h \
    -r 0x20-0x7F \
    -r 0xB0 \
    -r 0xC0-0xFF \
    -o "$OUT/montserrat_sv_$size.c"
}

build 10 700   # etiketter, axel
build 12 600   # ortnamn, klocka, väderlek
build 14 500   # värden i vänsterspalten
build 16 700   # temperatur i timtabellen
build 32 600   # hjältesiffran

echo
echo "Klart. Filer i $OUT:"
ls -la "$OUT"
echo
echo "Kontrollera att å/ä/ö/° finns med:"
grep -l 'montserrat_sv' "$OUT"/*.c >/dev/null && \
  echo "  (glyferna ligger i punktkartan; ser du rutor på skärmen är USE_SV_FONTS=0)"
